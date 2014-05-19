/*
 *  This file is derived from avahi_client_browse.c, part of avahi.
 *
 *  avahi is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2.1 of the
 *  License, or (at your option) any later version.
 *
 *  avahi is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with avahi; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 *  USA.
 */

/*
 * dnssd_hosts
 * Find all stations sending DNS-SD notifications by looking for
 * _workstation._tcp. For all stations where we have a MAC address,
 * output the MAC address and hostname.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

static AvahiSimplePoll *simple_poll = NULL;


static int is_mac_address(const char *s, size_t len)
{
  if (len < 17) {
    /* Too short to be a MAC address */
    return 0;
  }

  if ((s[2] == ':') && (s[5] == ':') && (s[8] == ':') &&
      (s[11] == ':') && (s[14] == ':') &&
      isxdigit(s[0]) && isxdigit(s[1]) &&
      isxdigit(s[3]) && isxdigit(s[4]) &&
      isxdigit(s[6]) && isxdigit(s[7]) &&
      isxdigit(s[9]) && isxdigit(s[10]) &&
      isxdigit(s[12]) && isxdigit(s[13]) &&
      isxdigit(s[15]) && isxdigit(s[16])) {
    return 1;
  }

  return 0;
}


static void print_split_strings(const char *str)
{
  size_t i, len = strlen(str);
  for (i = 2; i < len; i++) {
    if (is_mac_address(&str[i], len - i)) {
      char host[128];
      char mac[18];
      size_t hlen = i - 2;

      if (hlen >= sizeof(host)) {
        hlen = sizeof(host) - 1;
      }
      memset(mac, 0, sizeof(mac));
      memset(host, 0, sizeof(host));
      strncpy(mac, &str[i], 17);
      strncpy(host, str, hlen);

      printf ("%s|%s\n", mac, host);
    }
  }
}


static void service_browser_callback(
    AvahiServiceBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *name,
    const char *type,
    const char *domain,
    AvahiLookupResultFlags flags,
    void *userdata)
{
  int err;

  switch (event) {
    case AVAHI_BROWSER_FAILURE:
      err = avahi_client_errno(avahi_service_browser_get_client(b));
      fprintf(stderr, "AVAHI_BROWSER_FAILURE %s\n", avahi_strerror(err));
      avahi_simple_poll_quit(simple_poll);
      return;

    case AVAHI_BROWSER_NEW:
      print_split_strings(name);
      break;

    case AVAHI_BROWSER_REMOVE:
      break;

    case AVAHI_BROWSER_ALL_FOR_NOW:
      avahi_simple_poll_quit(simple_poll);
      break;

    case AVAHI_BROWSER_CACHE_EXHAUSTED:
      break;
  }
}

static void client_callback(AvahiClient *c, AvahiClientState state,
                            void * userdata)
{
  switch(state) {
    case AVAHI_CLIENT_FAILURE:
      fprintf(stderr, "Client failure: %s\n",
              avahi_strerror(avahi_client_errno(c)));
      avahi_simple_poll_quit(simple_poll);
      break;

    case AVAHI_CLIENT_S_REGISTERING:
    case AVAHI_CLIENT_S_RUNNING:
    case AVAHI_CLIENT_S_COLLISION:
    case AVAHI_CLIENT_CONNECTING:
      break;
  }
}

int main(int argc, char *argv[])
{
  AvahiClient *c = NULL;
  AvahiServiceBrowser *sb = NULL;
  int error;

  if ((simple_poll = avahi_simple_poll_new()) == NULL) {
    fprintf(stderr, "avahi_simple_poll_new failed.\n");
    exit(1);
  }

  if ((c = avahi_client_new(avahi_simple_poll_get(simple_poll), 0,
                            client_callback, NULL, &error)) == NULL) {
    fprintf(stderr, "avahi_client_new failed.\n");
    exit(1);
  }

  if ((sb = avahi_service_browser_new(c, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                                      "_workstation._tcp", NULL, 0,
                                      service_browser_callback, c)) == NULL) {
    fprintf(stderr, "avahi_service_browser_new failed.\n");
    exit(1);
  }

  avahi_simple_poll_loop(simple_poll);

  exit(0);
}
