/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2013 Keichi Takahashi keichi.t@me.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Substantially derived from * https://github.com/keichi/tiny-lldpd
 * also under the MIT license */

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <sys/socket.h>

#define MAXINTERFACES 8
const char *ifnames[MAXINTERFACES] = {0};
int ninterfaces = 0;

uint8_t sendbuf[1024];

const uint8_t lldpaddr[ETH_ALEN] = {0x01, 0x80, 0xc2, 0x00, 0x00, 0x0e};
#define ETH_P_LLDP            0x88cc
#define TLV_END               0
#define TLV_CHASSIS_ID        1
#define TLV_PORT_ID           2
#define TLV_TTL               3
#define TLV_PORT_DESCRIPTION  4
#define TLV_SYSTEM_NAME       5

#define CHASSIS_ID_MAC_ADDRESS  4
#define PORT_ID_MAC_ADDRESS     3

static int write_lldp_tlv_header(void *p, int type, int length)
{
  *((uint16_t *)p) = htons((type & 0x7f) << 9 | (length & 0x1ff));
  return 2;
}


static int write_lldp_type_subtype_tlv(size_t offset,
    uint8_t type, uint8_t subtype, int length, const void *data)
{
  uint8_t *p = sendbuf + offset;

  if ((offset + 2 + 1 + length) > sizeof(sendbuf)) {
    fprintf(stderr, "LLDP frame too large %zd > %zd\n",
        (offset + 2 + 1 + length), sizeof(sendbuf));
    exit(1);
  }

  p += write_lldp_tlv_header(p, type, length + 1);
  *p++ = subtype;
  memcpy(p, data, length);
  p += length;

  return (p - sendbuf);
}


static int write_lldp_type_tlv(size_t offset, uint8_t type,
    int length, const void *data)
{
  uint8_t *p = sendbuf + offset;

  if ((offset + 2 + length) > sizeof(sendbuf)) {
    fprintf(stderr, "LLDP frame too large %zd > %zd\n",
        (offset + 2 + length), sizeof(sendbuf));
    exit(1);
  }

  p += write_lldp_tlv_header(p, type, length);
  memcpy(p, data, length);
  p += length;

  return (p - sendbuf);
}


static int write_lldp_end_tlv(size_t offset)
{
  uint8_t *p = sendbuf + offset;

  if ((offset + 2) > sizeof(sendbuf)) {
    fprintf(stderr, "LLDP frame too large %zd > %zd\n",
        (offset + 2), sizeof(sendbuf));
    exit(1);
  }

  offset += write_lldp_tlv_header(p, TLV_END, 0);
  return offset;
}


static void mac_str_to_bytes(const char *macstr, uint8_t *mac)
{
  if (sscanf(macstr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
        &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
    fprintf(stderr, "Invalid MAC address: %s\n", macstr);
    exit(1);
  }
}


static size_t format_lldp_packet(const char *macaddr, const char *ifname,
    const char *serial)
{
  uint8_t saddr[ETH_ALEN];
  size_t offset = 0;
  struct ether_header *eh = (struct ether_header *)sendbuf;
  uint16_t ttl;

  mac_str_to_bytes(macaddr, saddr);
  memset(sendbuf, 0, sizeof(sendbuf));

  eh = (struct ether_header *)sendbuf;
  memcpy(eh->ether_shost, saddr, sizeof(eh->ether_shost));
  memcpy(eh->ether_dhost, lldpaddr, sizeof(eh->ether_dhost));
  eh->ether_type = htons(ETH_P_LLDP);
  offset = sizeof(*eh);

  offset = write_lldp_type_subtype_tlv(offset,
      TLV_CHASSIS_ID, CHASSIS_ID_MAC_ADDRESS, ETH_ALEN, saddr);
  offset = write_lldp_type_subtype_tlv(offset,
      TLV_PORT_ID, PORT_ID_MAC_ADDRESS, ETH_ALEN, saddr);

  ttl = htons(120);
  offset = write_lldp_type_tlv(offset, TLV_TTL, sizeof(ttl), &ttl);

  offset = write_lldp_type_tlv(offset,
      TLV_PORT_DESCRIPTION, strlen(ifname), ifname);
  offset = write_lldp_type_tlv(offset,
      TLV_SYSTEM_NAME, strlen(serial), serial);
  offset = write_lldp_end_tlv(offset);

  return offset;
}


#ifndef UNIT_TESTS
static void send_lldp_packet(int s, size_t len, const char *ifname)
{
  struct sockaddr_ll sll;

  memset(&sll, 0, sizeof(sll));
  sll.sll_family = PF_PACKET;
  sll.sll_ifindex = if_nametoindex(ifname);
  sll.sll_hatype = ARPHRD_ETHER;
  sll.sll_halen = ETH_ALEN;
  sll.sll_pkttype = PACKET_OTHERHOST;
  memcpy(sll.sll_addr, lldpaddr, ETH_ALEN);
  if (sendto(s, sendbuf, len, 0, (struct sockaddr*)&sll, sizeof(sll)) < 0) {
    fprintf(stderr, "LLDP sendto failed\n");
    exit(1);
  }
}


static void usage(const char *progname)
{
  fprintf(stderr, "usage: %s -i eth# -m 00:11:22:33:44:55 -s G0123456789\n",
      progname);
  exit(1);
}


int main(int argc, char *argv[])
{
  const char *macaddr = NULL;
  const char *serial = NULL;
  int c;
  int s;

  while ((c = getopt(argc, argv, "i:m:s:")) != -1) {
    switch (c) {
      case 'i':
        if (ninterfaces == (MAXINTERFACES - 1)) {
          usage(argv[0]);
        }
        ifnames[ninterfaces++] = optarg;
        break;
      case 'm':
        macaddr = optarg;
        break;
      case 's':
        serial = optarg;
        break;
      default:
        usage(argv[0]);
        break;
    }
  }

  if (ninterfaces == 0 || macaddr == NULL || serial == NULL) {
    usage(argv[0]);
  }

  if ((s = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
    fprintf(stderr, "socket(PF_PACKET) failed\n");
    exit(1);
  }

  while (1) {
    int i;

    for (i = 0; i < ninterfaces; ++i) {
      if (ifnames[i] != NULL) {
        size_t len = format_lldp_packet(macaddr, ifnames[i], serial);
        send_lldp_packet(s, len, ifnames[i]);
      }
      usleep(10000 + (rand() % 80000));
    }

    usleep(500000 + (rand() % 1000000));
  }

  return 0;
}
#endif  /* UNIT_TESTS */
