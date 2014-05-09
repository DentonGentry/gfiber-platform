/* ssdp_poll
 *
 * A client implementing the API described in
 * http://miniupnp.free.fr/minissdpd.html
 *
 * Requests the list of all known SSDP nodes and the
 * services they export, and prints it to stdout in
 * a format which is simple to parse.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

/* Encode length by using 7bit per Byte :
 * Most significant bit of each byte specifies that the
 * following byte is part of the code */
#define DECODELENGTH(n, p) { \
  n = 0; \
  do { n = (n << 7) | (*p & 0x7f); } \
  while (*(p++)&0x80); \
}

#define CODELENGTH(n, p) { \
  if(n>=0x10000000) *(p++) = (n >> 28) | 0x80; \
  if(n>=0x200000) *(p++) = (n >> 21) | 0x80; \
  if(n>=0x4000) *(p++) = (n >> 14) | 0x80; \
  if(n>=0x80) *(p++) = (n >> 7) | 0x80; \
  *(p++) = n & 0x7f; \
}

#define SOCK_PATH "/var/run/minissdpd.sock"

int connect_to_ssdpd()
{
  struct sockaddr_un addr;
  int s;

  s = socket(AF_UNIX, SOCK_STREAM, 0);
  if(s < 0) {
    perror("socket AF_UNIX failed");
    exit(1);
  }
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path));
  if(connect(s, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) < 0) {
    perror("connect to minisspd failed");
    exit(1);
  }

  return s;
}

int main()
{
  unsigned char *buffer;
  unsigned char *p;
  const char *device = "ssdp:all";
  int device_len = (int)strlen(device);
  int socket = connect_to_ssdpd();
  size_t siz = 65536;
  ssize_t len;
  fd_set readfds;
  struct timeval tv;

  if ((buffer = (unsigned char *)malloc(siz)) == NULL) {
    fprintf(stderr, "malloc(%zu) failed\n", siz);
    exit(1);
  }
  memset(buffer, 0, siz);

  buffer[0] = 5; /* request type : request all device server IDs */
  p = buffer + 1;
  CODELENGTH(device_len, p);
  memcpy(p, device, device_len);
  p += device_len;
  if (write(socket, buffer, p - buffer) < 0) {
    perror("write to minissdpd failed");
    exit(1);
  }

  FD_ZERO(&readfds);
  FD_SET(socket, &readfds);
  memset(&tv, 0, sizeof(tv));
  tv.tv_sec = 2;

  if (select(socket + 1, &readfds, NULL, NULL, &tv) < 1) {
    fprintf(stderr, "select failed\n");
    exit(1);
  }

  if ((len = read(socket, buffer, siz)) < 0) {
    perror("read from minissdpd failed");
    exit(1);
  }

  int num = buffer[0];
  p = buffer + 1;
  while (num-- > 0) {
    size_t copylen, slen;
    char url[256];
    char server[512];

    DECODELENGTH(slen, p);
    copylen = (slen >= sizeof(url)) ? sizeof(url) - 1 : slen;
    memcpy(url, p, copylen);
    url[copylen] = '\0';
    p += slen;

    DECODELENGTH(slen, p);
    copylen = (slen >= sizeof(server)) ? sizeof(server) - 1 : slen;
    memcpy(server, p, copylen);
    server[copylen] = '\0';
    p += slen;

    printf("%s|%s\n", url, server);
  }

  free(buffer);
  exit(0);
}
