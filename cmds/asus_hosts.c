/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <arpa/inet.h>
#include <endian.h>
#include <net/if.h>
#include <netinet/in.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define ASUS_DISCOVERY_PORT 9999
#define PACKET_LENGTH       512

/*
 * The packet format and magic numbers come from
 * asuswrt/release/src/router/networkmap/asusdiscovery.c
 * in GPL_RT-N66U_3.0.0.4.374.5517-g302e4dc.tgz,
 * available from
 * http://support.asus.com/Download.aspx?p=11&m=RT-N66U%20(VER.B1)&os=8
 */

typedef struct __attribute__((packed)) {
  uint8_t    service_id;
#define SERVICE_ID_IBOX_INFO  12
  uint8_t    packet_type;
#define PACKET_TYPE_REQUEST   21
#define PACKET_TYPE_RESULT    22
  uint16_t   opcode;    // always little-endian
#define OPCODE_GETINFO        31
  uint32_t   transaction_id;

  uint8_t    printer_info[128];
  uint8_t    SSID[32];
  uint8_t    netmask[32];
  uint8_t    product_id[32];
  uint8_t    firmware_version[16];
  uint8_t    operation_mode;
  uint8_t    mac_address[6];
  uint8_t    regulation;
} asus_discovery_packet_t;


int make_socket(const char *ifname)
{
  int s;
  struct ifreq ifr;
  struct sockaddr_in sin;
  int broadcast = 1;

  if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    perror("socket");
    exit(1);
  }

  memset(&ifr, 0, sizeof(ifr));
  snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
  if (setsockopt(s, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr))) {
    perror("SO_BINDTODEVICE");
    exit(1);
  }

  if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast))) {
    perror("SO_BROADCAST");
    exit(1);
  }

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(ASUS_DISCOVERY_PORT);
  sin.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(s, (struct sockaddr *)&sin, sizeof(sin))) {
    perror("bind");
    exit(1);
  }

  return s;
}

void send_discovery(int s)
{
  struct sockaddr_in sin;
  uint8_t buf[PACKET_LENGTH];
  asus_discovery_packet_t *discovery = (asus_discovery_packet_t *)buf;

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(ASUS_DISCOVERY_PORT);
  sin.sin_addr.s_addr = htonl(INADDR_BROADCAST);

  memset(buf, 0, sizeof(buf));
  discovery->service_id = SERVICE_ID_IBOX_INFO;
  discovery->packet_type = PACKET_TYPE_REQUEST;
  discovery->opcode = htole16(OPCODE_GETINFO);

  if (sendto(s, buf, sizeof(buf), MSG_DONTROUTE,
             (struct sockaddr *)&sin, sizeof(sin)) < 0) {
    perror("sendto");
    exit(1);
  }
}

static const char *replace_newlines(const uint8_t *src, int srclen,
                                    char *dst, int dstlen)
{
  int i, j;

  for (i = 0, j = 0; i < srclen && j < (dstlen - 1); i++) {
    dst[j++] = (src[i] == '\n') ? '.' : src[i];
  }
  dst[j] = '\0';

  return dst;
}

int receive_response(int s, char *response, int responselen)
{
  struct timeval tv;
  fd_set rfds;

  memset(&tv, 0, sizeof(tv));
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  FD_ZERO(&rfds);
  FD_SET(s, &rfds);

  if (select(s + 1, &rfds, NULL, NULL, &tv) < 0) {
    perror("select");
    exit(1);
  }
  if (FD_ISSET(s, &rfds)) {
    uint8_t buf[PACKET_LENGTH + 64];
    char addrbuf[16], namebuf[80];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    asus_discovery_packet_t *discovery = (asus_discovery_packet_t *)buf;
    int id_len;

    if (recvfrom(s, buf, sizeof(buf), 0,
                 (struct sockaddr *)&from, &fromlen) != PACKET_LENGTH) {
      /* Not an ASUS discovery response */
      return 1;
    }
    inet_ntop(AF_INET, &(from.sin_addr), addrbuf, sizeof(addrbuf));

    if (discovery->packet_type != PACKET_TYPE_RESULT) {
      /* We receive our own broadcast, and we send packet_type
       * PACKET_TYPE_REQUEST. Ignore our own packet. */
      return 1;
    }

    if ((discovery->service_id != SERVICE_ID_IBOX_INFO) ||
        (strlen((char *)discovery->product_id) == 0)) {
      /* Malformed packet, or isn't an ASUS response at all. */
      return 1;
    }

    id_len = strnlen((char *)discovery->product_id,
                     sizeof(discovery->product_id));
    replace_newlines(discovery->product_id, id_len, namebuf, sizeof(namebuf));
    snprintf(response, responselen, "%s|%s", addrbuf, namebuf);

    return 0;
  } else {
    return -1;
  }
}

#ifndef UNIT_TESTS
static void usage(char *progname)
{
  fprintf(stderr, "usage: %s [-i ifname]\n", progname);
  fprintf(stderr, "\t-i ifname - interface to use (default: lan0)\n");
  exit(1);
}

int main(int argc, char **argv)
{
  int s, opt, i;
  char *ifname = "br0";

  while ((opt = getopt(argc, argv, "i:")) != -1) {
    switch (opt) {
      case 'i':
        ifname = optarg;
        break;
      default:
        usage(argv[0]);
        break;
    }
  }

  if ((s = make_socket(ifname)) < 0) {
    exit(1);
  }

  send_discovery(s);
  for (i = 0; i < 128; i++) {
    char response[128];
    int rc = receive_response(s, response, sizeof(response));
    if (rc < 0) {
      break;
    } else if (rc == 0) {
      printf("%s\n", response);
    }
  }
}
#endif
