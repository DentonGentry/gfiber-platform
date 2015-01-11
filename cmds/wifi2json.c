/*
 * Portions of this code are derived from iw-3.17.
 *
 * Copyright (c) 2007, 2008 Johannes Berg
 * Copyright (c) 2007   Andy Lutomirski
 * Copyright (c) 2007   Mike Kershaw
 * Copyright (c) 2008-2009    Luis R. Rodriguez
 * Copyright (c) 2015   Google, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <ctype.h>
#include <inttypes.h>
#include <linux/nl80211.h>
#include <net/if.h>
#include <netlink/attr.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/netlink.h>
#include <netlink/socket.h>
#include <search.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>


/* Returns a copy of *original with all unprintable characters
 * replaced by exclamation points. */
static char *ReplaceUnsafe(const char *original, size_t len)
{
  size_t i;
  char *copy = (char *)malloc(len + 1);

  memset(copy, 0, len + 1);
  for (i = 0; i < len; ++i) {
    char c = original[i];
    int not_safe = (c == '"') || (c == '\'') || (c == '\\') || !isprint(c);
    copy[i] = not_safe ? '!' : c;
  }
  return copy;
}  /* ReplaceUnsafe */


static int ieee80211_frequency_to_channel(uint32_t freq)
{
  /* see 802.11-2007 17.3.8.3.2 and Annex J */
  if (freq == 2484)
    return 14;
  else if (freq < 2484)
    return (freq - 2407) / 5;
  else if (freq >= 4910 && freq <= 4980)
    return (freq - 4000) / 5;
  else if (freq <= 45000)
    return (freq - 5000) / 5;
  else if (freq >= 58320 && freq <= 64800)
    return (freq - 56160) / 2160;
  else
    return 0;
}  /* ieee80211_frequency_to_channel */


static int StationDumpCallback(struct nl_msg *msg, void *arg)
{
  struct genlmsghdr *gh = nlmsg_data(nlmsg_hdr(msg));
  struct nlattr *tb[NL80211_ATTR_MAX + 1] = {0};
  struct nlattr *si[NL80211_STA_INFO_MAX + 1] = {0};
  uint8_t *mac;
  static struct nla_policy stats_policy[NL80211_STA_INFO_MAX + 1] = {
    [NL80211_STA_INFO_INACTIVE_TIME] = { .type = NLA_U32 },
    [NL80211_STA_INFO_RX_BYTES64] = { .type = NLA_U64 },
    [NL80211_STA_INFO_RX_BYTES] = { .type = NLA_U32 },
    [NL80211_STA_INFO_RX_PACKETS] = { .type = NLA_U32 },
    [NL80211_STA_INFO_RX_DROP_MISC] = { .type = NLA_U64 },
    [NL80211_STA_INFO_TX_BYTES64] = { .type = NLA_U64 },
    [NL80211_STA_INFO_TX_BYTES] = { .type = NLA_U32 },
    [NL80211_STA_INFO_TX_PACKETS] = { .type = NLA_U32 },
    [NL80211_STA_INFO_TX_RETRIES] = { .type = NLA_U32 },
    [NL80211_STA_INFO_TX_FAILED] = { .type = NLA_U32 },
    [NL80211_STA_INFO_SIGNAL] = { .type = NLA_U8 },
    [NL80211_STA_INFO_SIGNAL_AVG] = { .type = NLA_U8 },
    [NL80211_STA_INFO_T_OFFSET] = { .type = NLA_U64 },
    [NL80211_STA_INFO_EXPECTED_THROUGHPUT] = { .type = NLA_U32 },
    [NL80211_STA_INFO_STA_FLAGS] = {
      .minlen = sizeof(struct nl80211_sta_flag_update) },

    /* There are NL80211_STA_INFO_TX_BITRATE and NL80211_STA_INFO_RX_BITRATE
     * fields defined which look like they would be useful, but we've found
     * they are almost never implemented by Wifi drivers. nl80211 nonetheless
     * populates them, with all bits set to False. This is not useful. */
  };

  if (nla_parse(tb, NL80211_ATTR_MAX,
                genlmsg_attrdata(gh, 0), genlmsg_attrlen(gh, 0), NULL)) {
    fprintf(stderr, "nla_parse failed.\n");
    return NL_SKIP;
  }

  if (!tb[NL80211_ATTR_STA_INFO]) {
    return NL_SKIP;
  }

  if (nla_parse_nested(si, NL80211_STA_INFO_MAX,
                       tb[NL80211_ATTR_STA_INFO],
                       stats_policy)) {
    fprintf(stderr, "nla_parse_nested failed\n");
    return NL_SKIP;
  }

  if (!tb[NL80211_ATTR_MAC]) {
    fprintf(stderr, "No NL80211_ATTR_MAC\n");
    return NL_SKIP;
  }

  printf("    {\n");
  mac = (uint8_t *)nla_data(tb[NL80211_ATTR_MAC]);
  printf("      \"macaddr\": \"%02x:%02x:%02x:%02x:%02x:%02x\",\n",
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  if (si[NL80211_STA_INFO_INACTIVE_TIME])
    printf("      \"inactive_msec\": %u,\n",
           nla_get_u32(si[NL80211_STA_INFO_INACTIVE_TIME]));

  if (si[NL80211_STA_INFO_RX_BYTES64])
    printf("      \"rx_bytes64\": %" PRIu64 ",\n",
           nla_get_u64(si[NL80211_STA_INFO_RX_BYTES64]));
  if (si[NL80211_STA_INFO_RX_BYTES])
    printf("      \"rx_bytes\": %u,\n",
           nla_get_u32(si[NL80211_STA_INFO_RX_BYTES]));
  if (si[NL80211_STA_INFO_RX_PACKETS])
    printf("      \"rx_packets\": %u,\n",
           nla_get_u32(si[NL80211_STA_INFO_RX_PACKETS]));
  if (si[NL80211_STA_INFO_RX_DROP_MISC])
    printf("      \"rx_drop64\": %" PRIu64 ",\n",
           nla_get_u64(si[NL80211_STA_INFO_RX_DROP_MISC]));
  if (si[NL80211_STA_INFO_TX_BYTES64])
    printf("      \"tx_bytes64\": %" PRIu64 ",\n",
           nla_get_u64(si[NL80211_STA_INFO_TX_BYTES64]));
  if (si[NL80211_STA_INFO_TX_BYTES])
    printf("      \"tx_bytes\": %u,\n",
           nla_get_u32(si[NL80211_STA_INFO_TX_BYTES]));
  if (si[NL80211_STA_INFO_TX_PACKETS])
    printf("      \"tx_packets\": %u,\n",
           nla_get_u32(si[NL80211_STA_INFO_TX_PACKETS]));
  if (si[NL80211_STA_INFO_TX_RETRIES])
    printf("      \"tx_retries\": %u,\n",
           nla_get_u32(si[NL80211_STA_INFO_TX_RETRIES]));
  if (si[NL80211_STA_INFO_TX_FAILED])
    printf("      \"tx failed\": %u,\n",
           nla_get_u32(si[NL80211_STA_INFO_TX_FAILED]));

  if (si[NL80211_STA_INFO_SIGNAL]) {
    printf("      \"signal_dbm\": %hhd,\n",
           (int8_t)nla_get_u8(si[NL80211_STA_INFO_SIGNAL]));
  }

  if (si[NL80211_STA_INFO_SIGNAL_AVG]) {
    printf("      \"signal_avg\": %hhd,\n",
           (int8_t)nla_get_u8(si[NL80211_STA_INFO_SIGNAL_AVG]));
  }

  if (si[NL80211_STA_INFO_T_OFFSET])
    printf("      \"Toffset_usec\": %" PRIu64 ",\n",
           nla_get_u64(si[NL80211_STA_INFO_T_OFFSET]));

  if (si[NL80211_STA_INFO_EXPECTED_THROUGHPUT]) {
    uint32_t thr = nla_get_u32(si[NL80211_STA_INFO_EXPECTED_THROUGHPUT]);
    printf("      \"expected_throughput\": %u.%u,\n",
           thr / 1000, thr % 1000);
  }

  if (si[NL80211_STA_INFO_STA_FLAGS]) {
    struct nl80211_sta_flag_update *sta_flags;
    sta_flags = (struct nl80211_sta_flag_update *)nla_data(
        si[NL80211_STA_INFO_STA_FLAGS]);
    #define BIT(x) (1ULL<<(x))
    #define PRINT_BOOL(name, bit) if (sta_flags->mask & BIT(bit)) \
      printf("      \"%s\": %s,\n", name, \
             (sta_flags->set & BIT(bit) ? "true" : "false"));

    PRINT_BOOL("authorized", NL80211_STA_FLAG_AUTHORIZED);
    PRINT_BOOL("authenticated", NL80211_STA_FLAG_AUTHENTICATED);
    PRINT_BOOL("preamble", NL80211_STA_FLAG_SHORT_PREAMBLE);
    PRINT_BOOL("WMM_WME", NL80211_STA_FLAG_WME);
  }

  printf("      \"dummy\": 0\n");
  printf("    },\n");
  return NL_OK;
}  /* StationDumpCallback */


void RequestAssociatedDevices(struct nl_sock *nlsk, int nl80211_id,
                              int ifindex)
{
  struct nl_msg *msg;

  if (nl_socket_modify_cb(nlsk, NL_CB_VALID, NL_CB_CUSTOM,
                          StationDumpCallback, NULL)) {
    fprintf(stderr, "nl_socket_modify_cb failed\n");
    exit(1);
  }

  if ((msg = nlmsg_alloc()) == NULL) {
    fprintf(stderr, "nlmsg_alloc failed\n");
    exit(1);
  }
  if (genlmsg_put(msg, 0, 0, nl80211_id, 0, NLM_F_DUMP,
                  NL80211_CMD_GET_STATION, 0) == NULL) {
    fprintf(stderr, "genlmsg_put failed\n");
    exit(1);
  }
  if (nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifindex)) {
    fprintf(stderr, "NL80211_CMD_GET_STATION put IFINDEX failed\n");
    exit(1);
  }

  if (nl_send_auto(nlsk, msg) < 0) {
    fprintf(stderr, "nl_send_auto failed\n");
    exit(1);
  }
  nlmsg_free(msg);
}  /* RequestAssociatedDevices */


static int BssidInfoCallback(struct nl_msg *msg, void *arg)
{
  struct genlmsghdr *gh = nlmsg_data(nlmsg_hdr(msg));
  struct nlattr *tb[NL80211_ATTR_MAX + 1] = {0};

  if (nla_parse(tb, NL80211_ATTR_MAX,
                genlmsg_attrdata(gh, 0), genlmsg_attrlen(gh, 0), NULL)) {
    fprintf(stderr, "nla_parse BssidInfoCallback failed.\n");
    return NL_SKIP;
  }

  if (tb[NL80211_ATTR_MAC]) {
    uint8_t *mac = (uint8_t *)nla_data(tb[NL80211_ATTR_MAC]);
    printf("    \"bssid\": \"%02x:%02x:%02x:%02x:%02x:%02x\",\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }

  if (tb[NL80211_ATTR_SSID]) {
    char *safestr = ReplaceUnsafe(nla_data(tb[NL80211_ATTR_SSID]),
                                  nla_len(tb[NL80211_ATTR_SSID]));
    printf("    \"ssid\": \"%s\",\n", safestr);
    free(safestr);
  }

  if (tb[NL80211_ATTR_REG_ALPHA2]) {
    char *country = nla_data(tb[NL80211_ATTR_REG_ALPHA2]);
    printf("    \"regdomain\": \"%c%c\",\n", country[0], country[1]);
  }

  if (tb[NL80211_ATTR_WIPHY_FREQ]) {
    uint32_t freq = nla_get_u32(tb[NL80211_ATTR_WIPHY_FREQ]);
    printf("    \"channel\": %u,\n", ieee80211_frequency_to_channel(freq));
  }

  return NL_OK;
}  /* BssidInfoCallback */


void RequestBssidInfo(struct nl_sock *nlsk, int nl80211_id, int ifindex)
{
  struct nl_msg *msg;

  if (nl_socket_modify_cb(nlsk, NL_CB_VALID, NL_CB_CUSTOM,
                          BssidInfoCallback, NULL)) {
    fprintf(stderr, "nl_socket_modify_cb failed\n");
    exit(1);
  }

  if ((msg = nlmsg_alloc()) == NULL) {
    fprintf(stderr, "nlmsg_alloc failed\n");
    exit(1);
  }
  if (genlmsg_put(msg, 0, 0, nl80211_id, 0, 0,
                  NL80211_CMD_GET_INTERFACE, 0) == NULL) {
    fprintf(stderr, "genlmsg_put failed\n");
    exit(1);
  }
  if (nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifindex)) {
    fprintf(stderr, "NL80211_CMD_GET_INTERFACE put IFINDEX failed\n");
    exit(1);
  }

  if (nl_send_auto(nlsk, msg) < 0) {
    fprintf(stderr, "nl_send_auto failed\n");
    exit(1);
  }
  nlmsg_free(msg);
}  /* RequestBssidInfo */


int GetIfIndex(const char *ifname)
{
  int fd;
  struct ifreq ifr;

  if (strlen(ifname) >= sizeof(ifr.ifr_name)) {
    fprintf(stderr, "interface name %s is too long\n", ifname);
    return -1;
  }

  if ((fd = socket(AF_PACKET, SOCK_DGRAM, 0)) < 0) {
    perror("socket");
    return -1;
  }

  memset(&ifr, 0, sizeof(ifr));
  snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);

  if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
    perror("SIOCGIFINDEX");
    close(fd);
    return -1;
  }

  close(fd);
  return ifr.ifr_ifindex;
}  /* GetIfIndex */


struct nl_sock *InitNetlinkSocket()
{
  struct nl_sock *nlsk;
  if ((nlsk = nl_socket_alloc()) == NULL) {
    fprintf(stderr, "socket allocation failed\n");
    exit(1);
  }

  if (genl_connect(nlsk) != 0) {
    fprintf(stderr, "genl_connect failed\n");
    exit(1);
  }

  if (nl_socket_set_nonblocking(nlsk)) {
    fprintf(stderr, "nl_socket_set_nonblocking failed\n");
    exit(1);
  }

  return nlsk;
}  /* InitNetlinkSocket */


void ProcessNetlinkMessages(struct nl_sock *nlsk)
{
  for (;;) {
    int s = nl_socket_get_fd(nlsk);
    fd_set rfds;
    struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };

    FD_ZERO(&rfds);
    FD_SET(s, &rfds);

    if (select(s + 1, &rfds, NULL, NULL, &timeout) <= 0) {
      break;
    }

    if (FD_ISSET(s, &rfds)) {
      nl_recvmsgs_default(nlsk);
    }
  }
}


void usage(const char *progname)
{
  printf("usage: %s -i wifi0\n", progname);
  printf("where:\n");
  printf("\t-i wifi0 the name of the Wifi interface.\n");
  exit(1);
}  /* usage */


int main(int argc, char **argv)
{
  int opt;
  char *interface = NULL;
  int nl80211_id = -1;
  int ifindex = -1;
  struct nl_sock *nlsk = NULL;

  while ((opt = getopt(argc, argv, "i:")) > 0) {
    switch(opt) {
      case 'i':
        interface = optarg;
        break;
      default:
        usage(argv[0]);
        break;
    }
  }

  if (!interface) {
    usage(argv[0]);
  }

  if ((ifindex = GetIfIndex(interface)) < 0) {
    fprintf(stderr, "GetIfIndex failed\n");
    exit(1);
  }

  nlsk = InitNetlinkSocket();

  if ((nl80211_id = genl_ctrl_resolve(nlsk, "nl80211")) < 0) {
    fprintf(stderr, "genl_ctrl_resolve failed\n");
    exit(1);
  }

  printf("{\n");
  printf("  \"associated_devices\": [\n");
  RequestAssociatedDevices(nlsk, nl80211_id, ifindex);
  ProcessNetlinkMessages(nlsk);
  printf("    { \"dummy\": 0 }\n");
  printf("  ],\n");

  printf("  \"bssid_info\": {\n");
  RequestBssidInfo(nlsk, nl80211_id, ifindex);
  ProcessNetlinkMessages(nlsk);
  printf("    \"dummy\": 0\n");
  printf("  }\n");
  printf("}\n");

  exit(0);
}
