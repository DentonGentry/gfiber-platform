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

/* for inttypes.h */
#define __STDC_FORMAT_MACROS

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <linux/if_ether.h>
#include <linux/nl80211.h>
#include <math.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netlink/attr.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/netlink.h>
#include <netlink/socket.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <cinttypes>
#include <string>
#include <tr1/unordered_map>

#ifndef UNIT_TESTS
#define STATIONS_DIR "/tmp/stations"
#define WIFIINFO_DIR "/tmp/wifi/wifiinfo"
#endif


#define MAX_CLIENT_AGE_SECS  (4 * 60 * 60)


#ifndef UNIT_TESTS
static time_t monotime(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
    return time(NULL);
  } else {
    return ts.tv_sec;
  }
}
#endif


typedef struct wifi_stats {
  uint64_t rx_drop64;

  // Accumulated values from the 32 bit counters.
  uint64_t rx_bytes64;
  uint64_t tx_bytes64;
  uint64_t rx_packets64;
  uint64_t tx_packets64;
  uint64_t tx_retries64;
  uint64_t tx_failed64;

  uint32_t rx_bitrate;
  uint32_t rx_bytes;
  uint32_t rx_packets;

  uint32_t tx_bitrate;
  uint32_t tx_bytes;
  uint32_t tx_packets;
  uint32_t tx_retries;
  uint32_t tx_failed;
  uint32_t expected_mbps;

#define MAX_SAMPLE_INDEX 150
  int rx_sample_index;
  uint8_t rx_ht_mcs_samples[MAX_SAMPLE_INDEX];
  uint8_t rx_vht_mcs_samples[MAX_SAMPLE_INDEX];
  uint8_t rx_width_samples[MAX_SAMPLE_INDEX];
  uint8_t rx_ht_nss_samples[MAX_SAMPLE_INDEX];
  uint8_t rx_vht_nss_samples[MAX_SAMPLE_INDEX];
  uint8_t rx_short_gi_samples[MAX_SAMPLE_INDEX];
  int8_t signal_samples[MAX_SAMPLE_INDEX];

  int tx_sample_index;
  uint8_t tx_ht_mcs_samples[MAX_SAMPLE_INDEX];
  uint8_t tx_vht_mcs_samples[MAX_SAMPLE_INDEX];
  uint8_t tx_width_samples[MAX_SAMPLE_INDEX];
  uint8_t tx_ht_nss_samples[MAX_SAMPLE_INDEX];
  uint8_t tx_vht_nss_samples[MAX_SAMPLE_INDEX];
  uint8_t tx_short_gi_samples[MAX_SAMPLE_INDEX];

  /*
   * Clients spend a lot of time mostly idle, where they
   * are only sending management frames and ACKs. These
   * tend to be sent at much lower MCS rates than bulk data;
   * if we report that MCS rate it gives a misleading
   * picture of what the client is capable of getting.
   *
   * Instead, we choose the largest sample over the reporting
   * interval. This is more likely to report a meaningful
   * MCS rate.
   */
  uint8_t rx_ht_mcs;
  uint8_t rx_vht_mcs;
  uint8_t rx_width;
  uint8_t rx_ht_nss;
  uint8_t rx_vht_nss;
  uint8_t rx_short_gi;

  uint8_t tx_ht_mcs;
  uint8_t tx_vht_mcs;
  uint8_t tx_width;
  uint8_t tx_ht_nss;
  uint8_t tx_vht_nss;
  uint8_t tx_short_gi;

  /* Track the largest value we've ever seen from this client. This
   * shows client capabilities, even if current interference
   * conditions don't allow it to use its full capability. */
  uint8_t rx_max_ht_mcs;
  uint8_t rx_max_vht_mcs;
  uint8_t rx_max_width;
  uint8_t rx_max_ht_nss;
  uint8_t rx_max_vht_nss;
  uint8_t ever_rx_short_gi;

  double max_signal;
  double min_signal;
  double avg_signal;

  uint8_t tx_max_ht_mcs;
  uint8_t tx_max_vht_mcs;
  uint8_t tx_max_width;
  uint8_t tx_max_ht_nss;
  uint8_t tx_max_vht_nss;
  uint8_t ever_tx_short_gi;

  int8_t signal;
  int8_t signal_avg;

  uint8_t authorized:1;
  uint8_t authenticated:1;
  uint8_t preamble:1;
  uint8_t wmm_wme:1;
  uint8_t mfp:1;
  uint8_t tdls_peer:1;
  uint8_t preamble_length:1;

  double inactive_since;
  uint32_t inactive_msec;
  uint32_t connected_secs;
} wifi_stats_t;


#define MAC_STR_LEN 18
#define IFNAME_STR_LEN 16


/*
 * Saved state for each associated Wifi device. Wifi clients drop out
 * after 5 minutes inactive, we want to export information about the
 * client for a while longer than that.
 */
typedef struct client_state {
  char macstr[MAC_STR_LEN];
  char ifname[IFNAME_STR_LEN];

  time_t first_seen;  // CLOCK_MONOTONIC
  time_t last_seen;  // CLOCK_MONOTONIC

  wifi_stats_t s;
} client_state_t;


// Hash table of known Wifi clients.
typedef std::tr1::unordered_map<std::string, client_state_t *> ClientMapType;
ClientMapType clients;


/* Data about each wifi interface. */
typedef struct wifi_interface {
  int ifindex;
  char ifname[IFNAME_STR_LEN];
  uint8_t bssid[ETH_ALEN];

  int is_client:1;
  uint32_t freq;

  wifi_stats_t s;
} wifi_interface_t;


/* List of wifi interfaces in the system. */
#define NINTERFACES 16
wifi_interface_t interfaces[NINTERFACES];
int ninterfaces = 0;


typedef struct callback_data {
  time_t mono_now;
} callback_data_t;


/* FILE handle to /tmp/wifi/wifiinfo, while open. */
static FILE *wifi_info_handle = NULL;


static void ClearClientStateCounters(client_state_t *state)
{
  char macstr[MAC_STR_LEN];

  memcpy(macstr, state->macstr, sizeof(macstr));
  memset(state, 0, sizeof(*state));
  memcpy(state->macstr, macstr, sizeof(state->macstr));
}


static int GetIfIndex(const char *ifname)
{
  int fd;
  struct ifreq ifr;

  if (strlen(ifname) >= sizeof(ifr.ifr_name)) {
    fprintf(stderr, "interface name %s is too long\n", ifname);
    exit(1);
  }

  if ((fd = socket(AF_PACKET, SOCK_DGRAM, 0)) < 0) {
    perror("socket");
    exit(1);
  }

  memset(&ifr, 0, sizeof(ifr));
  snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);

  if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
    char errbuf[128];
    snprintf(errbuf, sizeof(errbuf), "SIOCGIFINDEX %s", ifname);
    perror(errbuf);
    close(fd);
    exit(1);
  }

  close(fd);
  return ifr.ifr_ifindex;
}  /* GetIfIndex */


static void ProcessNetlinkMessages(struct nl_sock *nlsk, int *done)
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

    if (*done) {
      break;
    }
  }
}


static uint32_t GetBitrate(struct nlattr *attr)
{
  int rate = 0;
  struct nlattr *ri[NL80211_RATE_INFO_MAX + 1];
  static struct nla_policy rate_policy[NL80211_RATE_INFO_MAX + 1];

  memset(&rate_policy, 0, sizeof(rate_policy));
  rate_policy[NL80211_RATE_INFO_BITRATE].type = NLA_U16;

  if (nla_parse_nested(ri, NL80211_RATE_INFO_MAX, attr, rate_policy)) {
    fprintf(stderr, "nla_parse_nested NL80211_RATE_INFO_MAX failed");
    return 0;
  }

  if (ri[NL80211_RATE_INFO_BITRATE]) {
    rate = nla_get_u16(ri[NL80211_RATE_INFO_BITRATE]);
  }

  return rate;
}


static void GetMCS(struct nlattr *attr,
    int *mcs, int *vht_mcs, int *width, int *short_gi, int *vht_nss)
{
  int w160 = 0, w80_80 = 0, w80 = 0, w40 = 0;
  struct nlattr *ri[NL80211_RATE_INFO_MAX + 1];
  static struct nla_policy rate_policy[NL80211_RATE_INFO_MAX + 1];

  memset(&rate_policy, 0, sizeof(rate_policy));
  rate_policy[NL80211_RATE_INFO_MCS].type = NLA_U8;
  rate_policy[NL80211_RATE_INFO_VHT_MCS].type = NLA_U8;
  rate_policy[NL80211_RATE_INFO_VHT_NSS].type = NLA_U8;
  rate_policy[NL80211_RATE_INFO_40_MHZ_WIDTH].type = NLA_FLAG;
  rate_policy[NL80211_RATE_INFO_80_MHZ_WIDTH].type = NLA_FLAG;
  rate_policy[NL80211_RATE_INFO_80P80_MHZ_WIDTH].type = NLA_FLAG;
  rate_policy[NL80211_RATE_INFO_160_MHZ_WIDTH].type = NLA_FLAG;
  rate_policy[NL80211_RATE_INFO_SHORT_GI].type = NLA_FLAG;

  if (nla_parse_nested(ri, NL80211_RATE_INFO_MAX, attr, rate_policy)) {
    fprintf(stderr, "nla_parse_nested NL80211_RATE_INFO_MAX failed");
    return;
  }

  if (ri[NL80211_RATE_INFO_MCS]) {
    *mcs = nla_get_u8(ri[NL80211_RATE_INFO_MCS]);
  }
  if (ri[NL80211_RATE_INFO_VHT_MCS]) {
    *vht_mcs = nla_get_u8(ri[NL80211_RATE_INFO_VHT_MCS]);
  }
  if (ri[NL80211_RATE_INFO_VHT_NSS]) {
    *vht_nss = nla_get_u8(ri[NL80211_RATE_INFO_VHT_NSS]);
  }
  if (ri[NL80211_RATE_INFO_160_MHZ_WIDTH])   w160 = 1;
  if (ri[NL80211_RATE_INFO_80P80_MHZ_WIDTH]) w80_80 = 1;
  if (ri[NL80211_RATE_INFO_80_MHZ_WIDTH])    w80 = 1;
  if (ri[NL80211_RATE_INFO_40_MHZ_WIDTH])    w40 = 1;
  if (ri[NL80211_RATE_INFO_SHORT_GI])        *short_gi = 1;

  if (w160 || w80_80) {
    *width = 160;
  } else if (w80) {
    *width = 80;
  } else if (w40) {
    *width = 40;
  } else {
    *width = 20;
  }
}


static int HtMcsToNss(int rxmcs)
{
  /* https://en.wikipedia.org/wiki/IEEE_802.11n-2009 */
  switch(rxmcs) {
    case 0 ... 7:   return 1;
    case 8 ... 15:  return 2;
    case 16 ... 23: return 3;
    case 24 ... 31: return 4;
    case 32:        return 1;
    case 33 ... 38: return 2;
    case 39 ... 52: return 3;
    case 53 ... 76: return 4;
    default:        return 0;
  }
}


static int ParseWifiStats(struct nlattr *sta_info, wifi_stats_t *stats)
{
  struct nlattr *si[NL80211_STA_INFO_MAX + 1] = {0};
  static struct nla_policy stats_policy[NL80211_STA_INFO_MAX + 1];

  memset(&stats_policy, 0, sizeof(stats_policy));
  stats_policy[NL80211_STA_INFO_INACTIVE_TIME].type = NLA_U32;
  stats_policy[NL80211_STA_INFO_RX_BITRATE].type = NLA_NESTED;
  stats_policy[NL80211_STA_INFO_RX_BYTES].type = NLA_U32;
  stats_policy[NL80211_STA_INFO_RX_PACKETS].type = NLA_U32;
  stats_policy[NL80211_STA_INFO_TX_BITRATE].type = NLA_NESTED;
  stats_policy[NL80211_STA_INFO_TX_BYTES].type = NLA_U32;
  stats_policy[NL80211_STA_INFO_TX_PACKETS].type = NLA_U32;
  stats_policy[NL80211_STA_INFO_TX_RETRIES].type = NLA_U32;
  stats_policy[NL80211_STA_INFO_TX_FAILED].type = NLA_U32;
  stats_policy[NL80211_STA_INFO_CONNECTED_TIME].type = NLA_U32;
  stats_policy[NL80211_STA_INFO_SIGNAL].type = NLA_U8;
  stats_policy[NL80211_STA_INFO_SIGNAL_AVG].type = NLA_U8;
  stats_policy[NL80211_STA_INFO_STA_FLAGS].minlen = sizeof(struct nl80211_sta_flag_update);

#ifdef NL80211_RECENT_FIELDS
  stats_policy[NL80211_STA_INFO_RX_DROP_MISC].type = NLA_U64;
  stats_policy[NL80211_STA_INFO_EXPECTED_THROUGHPUT].type = NLA_U32;
#endif

  if (nla_parse_nested(si, NL80211_STA_INFO_MAX, sta_info, stats_policy)) {
    fprintf(stderr, "nla_parse_nested failed\n");
    return NL_SKIP;
  }

  if (si[NL80211_STA_INFO_INACTIVE_TIME]) {
    uint32_t inactive_msec = nla_get_u32(si[NL80211_STA_INFO_INACTIVE_TIME]);
    double inactive_since = time(NULL) - ((double)inactive_msec / 1000.0);

    stats->inactive_msec = inactive_msec;
    if ((fabs(inactive_since - stats->inactive_since)) > 2.0) {
      stats->inactive_since = inactive_since;
    }
  }

  if (si[NL80211_STA_INFO_RX_BITRATE]) {
    int rx_ht_mcs=0, rx_vht_mcs=0, rx_vht_nss=0, rx_width=0, rx_short_gi=0;
    int ht_nss;
    int n = stats->rx_sample_index + 1;

    if (n >= MAX_SAMPLE_INDEX) n = 0;

    stats->rx_bitrate = GetBitrate(si[NL80211_STA_INFO_RX_BITRATE]);
    GetMCS(si[NL80211_STA_INFO_RX_BITRATE], &rx_ht_mcs, &rx_vht_mcs,
        &rx_width, &rx_short_gi, &rx_vht_nss);

    stats->rx_ht_mcs_samples[n] = rx_ht_mcs;
    if (rx_ht_mcs > stats->rx_max_ht_mcs) stats->rx_max_ht_mcs = rx_ht_mcs;

    ht_nss = HtMcsToNss(rx_ht_mcs);
    stats->rx_ht_nss_samples[n] = ht_nss;
    if (ht_nss > stats->rx_max_ht_nss) stats->rx_max_ht_nss = ht_nss;

    stats->rx_vht_mcs_samples[n] = rx_vht_mcs;
    if (rx_vht_mcs > stats->rx_max_vht_mcs) stats->rx_max_vht_mcs = rx_vht_mcs;

    stats->rx_vht_nss_samples[n] = rx_vht_nss;
    if (rx_vht_nss > stats->rx_max_vht_nss) stats->rx_max_vht_nss = rx_vht_nss;

    stats->rx_short_gi_samples[n] = rx_short_gi;
    if (rx_short_gi) stats->ever_rx_short_gi = 1;

    stats->rx_width_samples[n] = rx_width;
    if (rx_width > stats->rx_max_width) stats->rx_max_width = rx_width;

    if (si[NL80211_STA_INFO_SIGNAL]) {
      int8_t signal = (int8_t)nla_get_u8(si[NL80211_STA_INFO_SIGNAL]);
      stats->signal_samples[n] = signal;
    }

    stats->rx_sample_index = n;
  }
  if (si[NL80211_STA_INFO_RX_BYTES]) {
    uint32_t last_rx_bytes = stats->rx_bytes;
    stats->rx_bytes = nla_get_u32(si[NL80211_STA_INFO_RX_BYTES]);
    stats->rx_bytes64 += (stats->rx_bytes - last_rx_bytes);
  }
  if (si[NL80211_STA_INFO_RX_PACKETS]) {
    uint32_t last_rx_packets = stats->rx_packets;
    stats->rx_packets = nla_get_u32(si[NL80211_STA_INFO_RX_PACKETS]);
    stats->rx_packets64 += (stats->rx_packets - last_rx_packets);
  }
  if (si[NL80211_STA_INFO_TX_BITRATE]) {
    int tx_ht_mcs=0, tx_vht_mcs=0, tx_vht_nss=0, tx_width=0, tx_short_gi=0;
    int ht_nss;
    int n = stats->tx_sample_index + 1;

    if (n >= MAX_SAMPLE_INDEX) n = 0;

    stats->tx_bitrate = GetBitrate(si[NL80211_STA_INFO_TX_BITRATE]);
    GetMCS(si[NL80211_STA_INFO_TX_BITRATE], &tx_ht_mcs, &tx_vht_mcs,
        &tx_width, &tx_short_gi, &tx_vht_nss);

    stats->tx_ht_mcs_samples[n] = tx_ht_mcs;
    if (tx_ht_mcs > stats->tx_max_ht_mcs) stats->tx_max_ht_mcs = tx_ht_mcs;

    ht_nss = HtMcsToNss(tx_ht_mcs);
    stats->tx_ht_nss_samples[n] = ht_nss;
    if (ht_nss > stats->tx_max_ht_nss) stats->tx_max_ht_nss = ht_nss;

    stats->tx_vht_mcs_samples[n] = tx_vht_mcs;
    if (tx_vht_mcs > stats->tx_max_vht_mcs) stats->tx_max_vht_mcs = tx_vht_mcs;

    stats->tx_vht_nss_samples[n] = tx_vht_nss;
    if (tx_vht_nss > stats->tx_max_vht_nss) stats->tx_max_vht_nss = tx_vht_nss;

    stats->tx_short_gi_samples[n] = tx_short_gi;
    if (tx_short_gi) stats->ever_tx_short_gi = 1;

    stats->tx_width_samples[n] = tx_width;
    if (tx_width > stats->tx_max_width) stats->tx_max_width = tx_width;

    stats->tx_sample_index = n;
  }
  if (si[NL80211_STA_INFO_TX_BYTES]) {
    uint32_t last_tx_bytes = stats->tx_bytes;
    stats->tx_bytes = nla_get_u32(si[NL80211_STA_INFO_TX_BYTES]);
    stats->tx_bytes64 += (stats->tx_bytes - last_tx_bytes);
  }
  if (si[NL80211_STA_INFO_TX_PACKETS]) {
    uint32_t last_tx_packets = stats->tx_packets;
    stats->tx_packets = nla_get_u32(si[NL80211_STA_INFO_TX_PACKETS]);
    stats->tx_packets64 += (stats->tx_packets - last_tx_packets);
  }
  if (si[NL80211_STA_INFO_TX_RETRIES]) {
    uint32_t last_tx_retries = stats->tx_retries;
    stats->tx_retries = nla_get_u32(si[NL80211_STA_INFO_TX_RETRIES]);
    stats->tx_retries64 += (stats->tx_retries - last_tx_retries);
  }
  if (si[NL80211_STA_INFO_TX_FAILED]) {
    uint32_t last_tx_failed = stats->tx_failed;
    stats->tx_failed = nla_get_u32(si[NL80211_STA_INFO_TX_FAILED]);
    stats->tx_failed64 += (stats->tx_failed - last_tx_failed);
  }
  if (si[NL80211_STA_INFO_CONNECTED_TIME]) {
    stats->connected_secs = nla_get_u32(si[NL80211_STA_INFO_CONNECTED_TIME]);
  }
  if (si[NL80211_STA_INFO_SIGNAL]) {
    stats->signal = (int8_t)nla_get_u8(si[NL80211_STA_INFO_SIGNAL]);
  }
  if (si[NL80211_STA_INFO_SIGNAL_AVG]) {
    stats->signal_avg = (int8_t)nla_get_u8(si[NL80211_STA_INFO_SIGNAL_AVG]);
  }

  if (si[NL80211_STA_INFO_STA_FLAGS]) {
    struct nl80211_sta_flag_update *sta_flags;
    sta_flags = (struct nl80211_sta_flag_update *)nla_data(
        si[NL80211_STA_INFO_STA_FLAGS]);

    #define BIT(x) ((sta_flags->mask & (1ULL<<(x))) ? 1 : 0)
    stats->authorized = BIT(NL80211_STA_FLAG_AUTHORIZED);
    stats->authenticated = BIT(NL80211_STA_FLAG_AUTHENTICATED);
    stats->preamble = BIT(NL80211_STA_FLAG_SHORT_PREAMBLE);
    stats->wmm_wme = BIT(NL80211_STA_FLAG_WME);
    stats->mfp = BIT(NL80211_STA_FLAG_MFP);
    stats->tdls_peer = BIT(NL80211_STA_FLAG_TDLS_PEER);
    stats->preamble_length = BIT(NL80211_STA_FLAG_SHORT_PREAMBLE);
    #undef BIT
  }

#ifdef NL80211_RECENT_FIELDS
  if (si[NL80211_STA_INFO_RX_DROP_MISC]) {
    stats->rx_drop64 = nla_get_u64(si[NL80211_STA_INFO_RX_DROP_MISC]);
  }
  if (si[NL80211_STA_INFO_EXPECTED_THROUGHPUT]) {
    stats->expected_mbps =
      nla_get_u32(si[NL80211_STA_INFO_EXPECTED_THROUGHPUT]);
  }
#endif

  return NL_OK;
}  /* ParseWifiStats */


static int InterfaceListCallback(struct nl_msg *msg, void *arg)
{
  struct nlattr *il[NL80211_ATTR_MAX + 1];
  struct genlmsghdr *gh = (struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
  wifi_interface_t *wif = &interfaces[ninterfaces];
  const char *name;

  nla_parse(il, NL80211_ATTR_MAX, genlmsg_attrdata(gh, 0),
      genlmsg_attrlen(gh, 0), NULL);

  if (!il[NL80211_ATTR_IFNAME]) {
    return NL_SKIP;
  }

  name = nla_get_string(il[NL80211_ATTR_IFNAME]);
  snprintf(wif->ifname, sizeof(wif->ifname), "%s", name);
  wif->ifindex = GetIfIndex(name);
  ninterfaces++;

  if (il[NL80211_ATTR_STA_INFO]) {
    ParseWifiStats(il[NL80211_ATTR_STA_INFO], &wif->s);
  }

  return NL_OK;
}


static int BssInfoCallback(struct nl_msg *msg, void *arg)
{
  struct nlattr *nl[NL80211_ATTR_MAX + 1];
  struct nlattr *bi[NL80211_BSS_MAX + 1];
  struct genlmsghdr *gh = (struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
  wifi_interface_t *wif = NULL;
  uint32_t ifindex;
  int i;
  static struct nla_policy bss_policy[NL80211_BSS_MAX + 1];

  memset(&bss_policy, 0, sizeof(bss_policy));
  bss_policy[NL80211_BSS_TSF].type = NLA_U64;
  bss_policy[NL80211_BSS_FREQUENCY].type = NLA_U32;
  bss_policy[NL80211_BSS_BSSID].type = NLA_UNSPEC;
  bss_policy[NL80211_BSS_BEACON_INTERVAL].type = NLA_U16;
  bss_policy[NL80211_BSS_CAPABILITY].type = NLA_U16;
  bss_policy[NL80211_BSS_INFORMATION_ELEMENTS].type = NLA_UNSPEC;
  bss_policy[NL80211_BSS_SIGNAL_MBM].type = NLA_U32;
  bss_policy[NL80211_BSS_SIGNAL_UNSPEC].type = NLA_U8;
  bss_policy[NL80211_BSS_STATUS].type = NLA_U32;

  nla_parse(nl, NL80211_ATTR_MAX, genlmsg_attrdata(gh, 0),
      genlmsg_attrlen(gh, 0), NULL);

  if (!nl[NL80211_ATTR_IFINDEX]) {
    return NL_SKIP;
  }

  ifindex = nla_get_u32(nl[NL80211_ATTR_IFINDEX]);
  for (i = 0; i < ninterfaces; ++i) {
    if (interfaces[i].ifindex == (int)ifindex) {
      wif = &interfaces[i];
      break;
    }
  }
  if (wif == NULL) {
    return NL_SKIP;
  }

  if (nla_parse_nested(bi, NL80211_BSS_MAX, nl[NL80211_ATTR_BSS],
        bss_policy)) {
    return NL_SKIP;
  }

  if (!bi[NL80211_BSS_BSSID] || !bi[NL80211_BSS_STATUS]) {
    return NL_SKIP;
  }

  wif->is_client = 0;
  if (bi[NL80211_BSS_STATUS]) {
    uint32_t status = nla_get_u32(bi[NL80211_BSS_STATUS]);
    wif->is_client = (status == NL80211_BSS_STATUS_ASSOCIATED) ? 1 : 0;
  }
  if (bi[NL80211_BSS_BSSID]) {
    memcpy(wif->bssid, nla_data(bi[NL80211_BSS_BSSID]), sizeof(wif->bssid));
  }
  if (bi[NL80211_BSS_FREQUENCY]) {
    wif->freq = nla_get_u32(bi[NL80211_BSS_FREQUENCY]);
  }

  return NL_OK;
}


static int InterfaceInfoCallback(struct nl_msg *msg, void *arg)
{
  struct nlattr *si[NL80211_ATTR_MAX + 1];
  struct genlmsghdr *gh = (struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
  wifi_interface_t *wif = NULL;
  uint32_t ifindex;
  int i;

  nla_parse(si, NL80211_ATTR_MAX, genlmsg_attrdata(gh, 0),
      genlmsg_attrlen(gh, 0), NULL);

  if (!si[NL80211_ATTR_IFINDEX]) {
    return NL_SKIP;
  }

  ifindex = nla_get_u32(si[NL80211_ATTR_IFINDEX]);
  for (i = 0; i < ninterfaces; ++i) {
    if (interfaces[i].ifindex == (int)ifindex) {
      wif = &interfaces[i];
      break;
    }
  }

  if (wif == NULL) {
    return NL_SKIP;
  }

  if (si[NL80211_ATTR_STA_INFO]) {
    ParseWifiStats(si[NL80211_ATTR_STA_INFO], &wif->s);
  }

  return NL_OK;
}


static void HandleNLCommand(struct nl_sock *nlsk, int nl80211_id,
                            int n, const uint8_t *bssid,
                            int cb(struct nl_msg *, void *),
                            int cmd, int flag)
{
  struct nl_msg *msg;
  int ifindex = n >= 0 ? interfaces[n].ifindex : -1;
  const char *ifname = n >= 0 ? interfaces[n].ifname : NULL;

  if (nl_socket_modify_cb(nlsk, NL_CB_VALID, NL_CB_CUSTOM,
                          cb, (void *)ifname)) {
    fprintf(stderr, "nl_socket_modify_cb failed\n");
    exit(1);
  }

  if ((msg = nlmsg_alloc()) == NULL) {
    fprintf(stderr, "nlmsg_alloc failed\n");
    exit(1);
  }
  if (genlmsg_put(msg, 0, 0, nl80211_id, 0, flag,
                  cmd, 0) == NULL) {
    fprintf(stderr, "genlmsg_put failed\n");
    exit(1);
  }

  if (ifindex >= 0 && nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifindex)) {
    fprintf(stderr, "NL80211_CMD_GET_STATION put IFINDEX failed\n");
    exit(1);
  }

  if (bssid && nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, bssid)) {
    fprintf(stderr, "NL80211_CMD_GET_STATION put MAC_ADDR failed\n");
    exit(1);
  }

  if (nl_send_auto(nlsk, msg) < 0) {
    fprintf(stderr, "nl_send_auto failed\n");
    exit(1);
  }
  nlmsg_free(msg);
}


void RequestInterfaceList(struct nl_sock *nlsk, int nl80211_id)
{
  HandleNLCommand(nlsk, nl80211_id, -1, NULL, InterfaceListCallback,
                  NL80211_CMD_GET_INTERFACE, NLM_F_DUMP);
}


void RequestInterfaceInfo(struct nl_sock *nlsk, int nl80211_id, int n)
{
  int done = 0;
  wifi_interface_t *wif = &interfaces[n];

  HandleNLCommand(nlsk, nl80211_id, n, NULL, BssInfoCallback,
                  NL80211_CMD_GET_SCAN, NLM_F_DUMP);
  ProcessNetlinkMessages(nlsk, &done);

  if (wif->is_client) {
    done = 0;
    HandleNLCommand(nlsk, nl80211_id, n, wif->bssid, InterfaceInfoCallback,
                    NL80211_CMD_GET_STATION, 0);
    ProcessNetlinkMessages(nlsk, &done);
  }
}


int NlFinish(struct nl_msg *msg, void *arg)
{
  int *ret = (int *)arg;
  *ret = 1;
  return NL_OK;
}


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



static client_state_t *FindClientState(const uint8_t mac[6])
{
  ClientMapType::iterator it;
  client_state_t *s;
  char macstr[MAC_STR_LEN];

  snprintf(macstr, sizeof(macstr), "%02x:%02x:%02x:%02x:%02x:%02x",
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  /* Find any existing state for this STA, or allocate new. */
  if ((it = clients.find(macstr)) == clients.end()) {
    s = (client_state_t *)malloc(sizeof(*s));
    memset(s, 0, sizeof(*s));
    memcpy(s->macstr, macstr, sizeof(s->macstr));
    s->first_seen = monotime();
    clients[std::string(macstr)] = s;
  } else {
    s = it->second;
  }

  return s;
}


static int StationDumpCallback(struct nl_msg *msg, void *arg)
{
  const char *ifname = (const char *)arg;
  struct genlmsghdr *gh = (struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
  struct nlattr *tb[NL80211_ATTR_MAX + 1] = {0};
  uint8_t *mac;
  client_state_t *state;

  if (nla_parse(tb, NL80211_ATTR_MAX,
                genlmsg_attrdata(gh, 0), genlmsg_attrlen(gh, 0), NULL)) {
    fprintf(stderr, "nla_parse failed.\n");
    return NL_SKIP;
  }

  if (!tb[NL80211_ATTR_STA_INFO]) {
    return NL_SKIP;
  }

  if (!tb[NL80211_ATTR_MAC]) {
    fprintf(stderr, "No NL80211_ATTR_MAC\n");
    return NL_SKIP;
  }

  mac = (uint8_t *)nla_data(tb[NL80211_ATTR_MAC]);
  state = FindClientState(mac);

  if (strcasecmp(state->ifname, ifname) != 0) {
    /* Client moved from one interface to another */
    ClearClientStateCounters(state);
  }

  state->last_seen = monotime();
  snprintf(state->ifname, sizeof(state->ifname), "%s", ifname);

  if (ParseWifiStats(tb[NL80211_ATTR_STA_INFO], &state->s) != NL_OK) {
    return NL_SKIP;
  }

  return NL_OK;
}  /* StationDumpCallback */


void RequestAssociatedDevices(struct nl_sock *nlsk,
    int nl80211_id, int n)
{
  HandleNLCommand(nlsk, nl80211_id, n, NULL, StationDumpCallback,
                  NL80211_CMD_GET_STATION, NLM_F_DUMP);
}  /* RequestAssociatedDevices */


static void ClearClientCounters(client_state_t *state)
{
  /* Kernel cleared its counters when client re-joined the WLAN,
   * clear out previous state as well. */
  state->s.rx_bytes = 0;
  state->s.rx_packets = 0;
  state->s.tx_bytes = 0;
  state->s.tx_packets = 0;
  state->s.tx_retries = 0;
  state->s.tx_failed = 0;
}


static int AgeOutClient(client_state_t *state)
{
  time_t mono_now = monotime();

  if ((mono_now - state->last_seen) > MAX_CLIENT_AGE_SECS) {
    char filename[PATH_MAX];
    snprintf(filename, sizeof(filename), "%s/%s", STATIONS_DIR, state->macstr);
    unlink(filename);
    return 1;
  }

  if (state->s.connected_secs < 60) {
    /* If the client recently dropped off and came back, clear any counters
     * we've been maintaining. */
    ClearClientCounters(state);
  }

  return 0;
}


static void ConsolidateSamples(wifi_stats_t *stats)
{
  int i;
  uint8_t rx_ht_mcs=0, rx_vht_mcs=0, rx_width=0, rx_ht_nss=0;
  uint8_t rx_vht_nss=0, rx_short_gi=0;
  uint8_t tx_ht_mcs=0, tx_vht_mcs=0, tx_width=0, tx_ht_nss=0;
  uint8_t tx_vht_nss=0, tx_short_gi=0;
  double max_signal = -1000.0;
  double min_signal = 0.0;
  double sum_signal = 0.0;

  for (i = 0; i < MAX_SAMPLE_INDEX; ++i) {
    if (stats->rx_ht_mcs_samples[i] > rx_ht_mcs) {
      rx_ht_mcs = stats->rx_ht_mcs_samples[i];
    }
    if (stats->rx_vht_mcs_samples[i] > rx_vht_mcs) {
      rx_vht_mcs = stats->rx_vht_mcs_samples[i];
    }
    if (stats->rx_width_samples[i] > rx_width) {
      rx_width = stats->rx_width_samples[i];
    }
    if (stats->rx_ht_nss_samples[i] > rx_ht_nss) {
      rx_ht_nss = stats->rx_ht_nss_samples[i];
    }
    if (stats->rx_vht_nss_samples[i] > rx_vht_nss) {
      rx_vht_nss = stats->rx_vht_nss_samples[i];
    }
    if (stats->rx_short_gi_samples[i] > rx_short_gi) {
      rx_short_gi = stats->rx_short_gi_samples[i];
    }

    if (stats->signal_samples[i] > max_signal) {
      max_signal = stats->signal_samples[i];
    }
    if (stats->signal_samples[i] < min_signal) {
      min_signal = stats->signal_samples[i];
    }
    sum_signal += stats->signal_samples[i];

    if (stats->tx_ht_mcs_samples[i] > tx_ht_mcs) {
      tx_ht_mcs = stats->tx_ht_mcs_samples[i];
    }
    if (stats->tx_vht_mcs_samples[i] > tx_vht_mcs) {
      tx_vht_mcs = stats->tx_vht_mcs_samples[i];
    }
    if (stats->tx_width_samples[i] > tx_width) {
      tx_width = stats->tx_width_samples[i];
    }
    if (stats->tx_ht_nss_samples[i] > tx_ht_nss) {
      tx_ht_nss = stats->tx_ht_nss_samples[i];
    }
    if (stats->tx_vht_nss_samples[i] > tx_vht_nss) {
      tx_vht_nss = stats->tx_vht_nss_samples[i];
    }
    if (stats->tx_short_gi_samples[i] > tx_short_gi) {
      tx_short_gi = stats->tx_short_gi_samples[i];
    }
  }

  stats->rx_ht_mcs = rx_ht_mcs;
  stats->rx_vht_mcs = rx_vht_mcs;
  stats->rx_width = rx_width;
  stats->rx_ht_nss = rx_ht_nss;
  stats->rx_vht_nss = rx_vht_nss;
  stats->rx_short_gi = rx_short_gi;

  stats->max_signal = max_signal;
  stats->min_signal = min_signal;
  stats->avg_signal = sum_signal / (double)MAX_SAMPLE_INDEX;

  stats->tx_ht_mcs = tx_ht_mcs;
  stats->tx_vht_mcs = tx_vht_mcs;
  stats->tx_width = tx_width;
  stats->tx_ht_nss = tx_ht_nss;
  stats->tx_vht_nss = tx_vht_nss;
  stats->tx_short_gi = tx_short_gi;
}


static void ClientStateToJson(const client_state_t *state)
{
  char tmpfile[PATH_MAX];
  char filename[PATH_MAX];
  time_t mono_now = monotime();
  FILE *f;

  snprintf(tmpfile, sizeof(tmpfile), "%s/%s.new", STATIONS_DIR, state->macstr);
  snprintf(filename, sizeof(filename), "%s/%s", STATIONS_DIR, state->macstr);

  if ((f = fopen(tmpfile, "w+")) == NULL) {
    char errbuf[80];
    snprintf(errbuf, sizeof(errbuf), "fopen %s", tmpfile);
    perror(errbuf);
    return;
  }

  fprintf(f, "{\n");

  fprintf(f, "  \"addr\": \"%s\",\n", state->macstr);
  fprintf(f, "  \"inactive since\": %.3f,\n", state->s.inactive_since);
  fprintf(f, "  \"inactive msec\": %u,\n", state->s.inactive_msec);

  fprintf(f, "  \"active\": %s,\n",
      ((mono_now - state->last_seen) < 600) ? "true" : "false");

  fprintf(f, "  \"rx bitrate\": %u.%u,\n",
      (state->s.rx_bitrate / 10), (state->s.rx_bitrate % 10));
  fprintf(f, "  \"rx bytes\": %u,\n", state->s.rx_bytes);
  fprintf(f, "  \"rx packets\": %u,\n", state->s.rx_packets);

  fprintf(f, "  \"tx bitrate\": %u.%u,\n",
      (state->s.tx_bitrate / 10), (state->s.tx_bitrate % 10));
  fprintf(f, "  \"tx bytes\": %u,\n", state->s.tx_bytes);
  fprintf(f, "  \"tx packets\": %u,\n", state->s.tx_packets);
  fprintf(f, "  \"tx retries\": %u,\n", state->s.tx_retries);
  fprintf(f, "  \"tx failed\": %u,\n", state->s.tx_failed);

  fprintf(f, "  \"rx mcs\": %u,\n", state->s.rx_ht_mcs);
  fprintf(f, "  \"rx max mcs\": %u,\n", state->s.rx_max_ht_mcs);
  fprintf(f, "  \"rx vht mcs\": %u,\n", state->s.rx_vht_mcs);
  fprintf(f, "  \"rx max vht mcs\": %u,\n", state->s.rx_max_vht_mcs);
  fprintf(f, "  \"rx width\": %u,\n", state->s.rx_width);
  fprintf(f, "  \"rx max width\": %u,\n", state->s.rx_max_width);
  fprintf(f, "  \"rx ht_nss\": %u,\n", state->s.rx_ht_nss);
  fprintf(f, "  \"rx max ht_nss\": %u,\n", state->s.rx_max_ht_nss);
  fprintf(f, "  \"rx vht_nss\": %u,\n", state->s.rx_vht_nss);
  fprintf(f, "  \"rx max vht_nss\": %u,\n", state->s.rx_max_vht_nss);

  #define BOOL(x) (x ? "true" : "false")
  fprintf(f, "  \"rx SHORT_GI\": %s,\n", BOOL(state->s.rx_short_gi));
  fprintf(f, "  \"rx SHORT_GI seen\": %s,\n", BOOL(state->s.ever_rx_short_gi));
  #undef BOOL

  fprintf(f, "  \"signal\": %hhd,\n", state->s.signal);
  fprintf(f, "  \"signal_avg\": %hhd,\n", state->s.signal_avg);

  #define BOOL(x) (x ? "yes" : "no")
  fprintf(f, "  \"authorized\": \"%s\",\n", BOOL(state->s.authorized));
  fprintf(f, "  \"authenticated\": \"%s\",\n", BOOL(state->s.authenticated));
  fprintf(f, "  \"preamble\": \"%s\",\n", BOOL(state->s.preamble));
  fprintf(f, "  \"wmm_wme\": \"%s\",\n", BOOL(state->s.wmm_wme));
  fprintf(f, "  \"mfp\": \"%s\",\n", BOOL(state->s.mfp));
  fprintf(f, "  \"tdls_peer\": \"%s\",\n", BOOL(state->s.tdls_peer));
  #undef BOOL

  fprintf(f, "  \"preamble length\": \"%s\",\n",
      (state->s.preamble_length ? "short" : "long"));

  fprintf(f, "  \"rx bytes64\": %" PRIu64 ",\n", state->s.rx_bytes64);
  fprintf(f, "  \"rx drop64\": %" PRIu64 ",\n", state->s.rx_drop64);
  fprintf(f, "  \"tx bytes64\": %" PRIu64 ",\n", state->s.tx_bytes64);
  fprintf(f, "  \"tx retries64\": %" PRIu64 ",\n", state->s.tx_retries64);
  fprintf(f, "  \"expected Mbps\": %u.%03u,\n",
          (state->s.expected_mbps / 1000), (state->s.expected_mbps % 1000));

  fprintf(f, "  \"ifname\": \"%s\"\n", state->ifname);
  fprintf(f, "}\n");

  fclose(f);
  if (rename(tmpfile, filename)) {
    char errstr[160];
    snprintf(errstr, sizeof(errstr), "%s: rename %s to %s",
        __FUNCTION__, tmpfile, filename);
    perror(errstr);
  }
}


static void ClientStateToLog(const client_state_t *state, time_t mono_now)
{
  if (!state->s.authorized || !state->s.authenticated) {
    /* Don't log about non-associated clients */
    return;
  }

  if ((mono_now - state->first_seen) < 120) {
    /* Allow data to accumulate before beginning to log it. */
    return;
  }

  printf(
      "%s %s %ld %" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
      " %c,%hhd,%hhd,%u,%u,%u,%u,%u,%d"
      " %u,%u,%u,%u,%u,%d"
      " %u,%u,%u,%u,%u,%d"
      " %u,%u,%u,%u,%u,%d"
      " rssi:%0.2f/%0.2f/%0.2f"
      "\n",
      state->macstr, state->ifname,
      ((mono_now - state->last_seen) + (state->s.inactive_msec / 1000)),

      /* L2 traffic stats */
      state->s.rx_bytes64, state->s.rx_drop64, state->s.tx_bytes64,
      state->s.tx_retries64, state->s.tx_failed64,

      /* L1 information */
      (state->s.preamble_length ? 'S' : 'L'),
      state->s.signal, state->s.signal_avg,
      state->s.rx_ht_mcs, state->s.rx_ht_nss,
      state->s.rx_vht_mcs, state->s.rx_vht_nss,
      state->s.rx_width, state->s.rx_short_gi,

      /* information about the maximum we've ever seen from this client. */
      state->s.rx_max_ht_mcs, state->s.rx_max_ht_nss,
      state->s.rx_max_vht_mcs, state->s.rx_max_vht_nss,
      state->s.rx_max_width, state->s.ever_rx_short_gi,

      state->s.tx_ht_mcs, state->s.tx_ht_nss,
      state->s.tx_vht_mcs, state->s.tx_vht_nss,
      state->s.tx_width, state->s.tx_short_gi,

      /* information about the maximum we've ever seen from this client. */
      state->s.tx_max_ht_mcs, state->s.tx_max_ht_nss,
      state->s.tx_max_vht_mcs, state->s.tx_max_vht_nss,
      state->s.tx_max_width, state->s.ever_tx_short_gi,

      state->s.min_signal, state->s.avg_signal, state->s.max_signal);
}


void ConsolidateAssociatedDevices()
{
  ClientMapType::iterator it = clients.begin();

  while (it != clients.end()) {
    client_state_t *state = it->second;
    ConsolidateSamples(&state->s);
    if (AgeOutClient(state)) {
      clients.erase(it++);
    } else {
      ++it;
    }
  }
}


/* Walk through all Wifi clients, printing their info to JSON files. */
void UpdateAssociatedDevices()
{
  ClientMapType::iterator it = clients.begin();

  while (it != clients.end()) {
    client_state_t *state = it->second;
    ClientStateToJson(state);
    ++it;
  }
}


void LogAssociatedDevices()
{
  ClientMapType::iterator it = clients.begin();
  time_t mono_now = monotime();

  while (it != clients.end()) {
    client_state_t *state = it->second;
    ClientStateToLog(state, mono_now);
    ++it;
  }
}


void LogInterfaces()
{
  int i;
  for (i = 0; i < ninterfaces; ++i) {
    wifi_interface_t *wif = &interfaces[i];

    if (!wif->is_client) {
      continue;
    }

    ConsolidateSamples(&wif->s);

    printf(
        "C %s %d %" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
        " %c,%hhd,%hhd,%u,%u,%u,%u,%u,%d"
        " %u,%u,%u,%u,%u,%d"
        " %u,%u,%u,%u,%u,%d"
        " %u,%u,%u,%u,%u,%d"
        " %0.2f %0.2f %0.2f"
        "\n",
        wif->ifname, wif->freq,

        /* L2 traffic stats */
        wif->s.rx_bytes64, wif->s.rx_drop64, wif->s.tx_bytes64,
        wif->s.tx_retries64, wif->s.tx_failed64,

        /* L1 information */
        (wif->s.preamble_length ? 'S' : 'L'),
        wif->s.signal, wif->s.signal_avg,
        wif->s.rx_ht_mcs, wif->s.rx_ht_nss,
        wif->s.rx_vht_mcs, wif->s.rx_vht_nss,
        wif->s.rx_width, wif->s.rx_short_gi,

        /* information about the maximum we've ever received. */
        wif->s.rx_max_ht_mcs, wif->s.rx_max_ht_nss,
        wif->s.rx_max_vht_mcs, wif->s.rx_max_vht_nss,
        wif->s.rx_max_width, wif->s.ever_rx_short_gi,

        wif->s.tx_ht_mcs, wif->s.tx_ht_nss,
        wif->s.tx_vht_mcs, wif->s.tx_vht_nss,
        wif->s.tx_width, wif->s.tx_short_gi,

        /* information about the maximum we've ever achieved. */
        wif->s.tx_max_ht_mcs, wif->s.tx_max_ht_nss,
        wif->s.tx_max_vht_mcs, wif->s.tx_max_vht_nss,
        wif->s.tx_max_width, wif->s.ever_tx_short_gi,
        wif->s.min_signal, wif->s.avg_signal, wif->s.max_signal);
  }
}


static int ieee80211_frequency_to_channel(int freq)
{
  /* see 802.11-2007 17.3.8.3.2 and Annex J */
  if (freq == 2484)
    return 14;
  else if (freq < 2484)
    return (freq - 2407) / 5;
  else if (freq >= 4910 && freq <= 4980)
    return (freq - 4000) / 5;
  else if (freq <= 45000) /* DMG band lower limit */
    return (freq - 5000) / 5;
  else if (freq >= 58320 && freq <= 64800)
    return (freq - 56160) / 2160;
  else
    return 0;
}


static void print_ssid_escaped(FILE *f, int len, const uint8_t *data)
{
  int i;

  for (i = 0; i < len; i++) {
    switch(data[i]) {
      case '\\': fprintf(f, "\\\\"); break;
      case '"': fprintf(f, "\\\""); break;
      case '\b': fprintf(f, "\\b"); break;
      case '\f': fprintf(f, "\\f"); break;
      case '\n': fprintf(f, "\\n"); break;
      case '\r': fprintf(f, "\\r"); break;
      case '\t': fprintf(f, "\\t"); break;
      default:
        if ((data[i] <= 0x1f) || !isprint(data[i])) {
          fprintf(f, "\\u00%02x", data[i]);
        } else {
          fprintf(f, "%c", data[i]);
        }
        break;
    }
  }
}


static int WlanInfoCallback(struct nl_msg *msg, void *arg)
{
  struct genlmsghdr *gnlh = (struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
  struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];

  nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL);

  if (tb_msg[NL80211_ATTR_MAC]) {
    unsigned char *mac_addr = (unsigned char *)nla_data(tb_msg[NL80211_ATTR_MAC]);
    fprintf(wifi_info_handle,
            "  \"BSSID\": \"%02x:%02x:%02x:%02x:%02x:%02x\",\n",
            mac_addr[0], mac_addr[1], mac_addr[2],
            mac_addr[3], mac_addr[4], mac_addr[5]);
  }
  if (tb_msg[NL80211_ATTR_SSID]) {
    fprintf(wifi_info_handle, "  \"SSID\": \"");
    print_ssid_escaped(wifi_info_handle, nla_len(tb_msg[NL80211_ATTR_SSID]),
                       (const uint8_t *)nla_data(tb_msg[NL80211_ATTR_SSID]));
    fprintf(wifi_info_handle, "\",\n");
  }
  if (tb_msg[NL80211_ATTR_WIPHY_FREQ]) {
    uint32_t freq = nla_get_u32(tb_msg[NL80211_ATTR_WIPHY_FREQ]);
    fprintf(wifi_info_handle, "  \"Channel\": %d,\n",
            ieee80211_frequency_to_channel(freq));
  }

  return NL_SKIP;
}


void RequestWifiInfo(struct nl_sock *nlsk, int nl80211_id, int n)
{
  HandleNLCommand(nlsk, nl80211_id, n, NULL, WlanInfoCallback,
                  NL80211_CMD_GET_INTERFACE, 0);
}


static int RegdomainCallback(struct nl_msg *msg, void *arg)
{
  struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
  struct genlmsghdr *gnlh = (struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
  char *reg;

  nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL);

  if (!tb_msg[NL80211_ATTR_REG_ALPHA2]) {
    return NL_SKIP;
  }

  if (!tb_msg[NL80211_ATTR_REG_RULES]) {
    return NL_SKIP;
  }

  reg = (char *)nla_data(tb_msg[NL80211_ATTR_REG_ALPHA2]);
  fprintf(wifi_info_handle, "  \"RegDomain\": \"%c%c\",\n", reg[0], reg[1]);

  return NL_SKIP;
}


void RequestRegdomain(struct nl_sock *nlsk, int nl80211_id)
{
  HandleNLCommand(nlsk, nl80211_id, -1, NULL, RegdomainCallback,
                  NL80211_CMD_GET_REG, 0);
}


void UpdateWifiShow(struct nl_sock *nlsk, int nl80211_id, int n)
{
  char tmpfile[PATH_MAX];
  char filename[PATH_MAX];
  char autofile[PATH_MAX];
  const char *ifname = interfaces[n].ifname;
  int done = 0;
  struct stat buffer;
  FILE *fptr;

  if (!ifname || !ifname[0]) {
    return;
  }

  snprintf(tmpfile, sizeof(tmpfile), "%s/%s.new", WIFIINFO_DIR, ifname);
  snprintf(filename, sizeof(filename), "%s/%s", WIFIINFO_DIR, ifname);

  if ((wifi_info_handle = fopen(tmpfile, "w+")) == NULL) {
    perror("fopen");
    return;
  }

  fprintf(wifi_info_handle, "{\n");
  done = 0;
  RequestWifiInfo(nlsk, nl80211_id, n);
  ProcessNetlinkMessages(nlsk, &done);

  done = 0;
  RequestRegdomain(nlsk, nl80211_id);
  ProcessNetlinkMessages(nlsk, &done);

  snprintf(autofile, sizeof(autofile), "/tmp/autochan.%s", ifname);
  if (stat(autofile, &buffer) == 0) {
    fprintf(wifi_info_handle, "  \"AutoChannel\": true,\n");
  } else {
    fprintf(wifi_info_handle, "  \"AutoChannel\": false,\n");
  }
  snprintf(autofile, sizeof(autofile), "/tmp/autotype.%s", ifname);
  if ((fptr = fopen(autofile, "r")) == NULL) {
    fprintf(wifi_info_handle, "  \"AutoType\": \"LOW\"\n");
  } else {
    char buf[24];
    if (fgets(buf, sizeof(buf), fptr) != NULL) {
      fprintf(wifi_info_handle, "  \"AutoType\": \"%s\"\n", buf);
    }
    fclose(fptr);
    fptr = NULL;
  }
  fprintf(wifi_info_handle, "}\n");

  fclose(wifi_info_handle);
  wifi_info_handle = NULL;
  if (rename(tmpfile, filename)) {
    char errbuf[256];
    snprintf(errbuf, sizeof(errbuf), "%s: rename %s to %s : errno=%d",
        __FUNCTION__, tmpfile, filename, errno);
    perror(errbuf);
  }
}

#ifndef UNIT_TESTS
static void TouchUpdateFile()
{
  char filename[PATH_MAX];
  int fd;

  snprintf(filename, sizeof(filename), "%s/updated.new", STATIONS_DIR);
  if ((fd = open(filename, O_CREAT | O_WRONLY, 0666)) < 0) {
    perror("TouchUpdatedFile open");
    exit(1);
  }

  if (write(fd, "updated", 7) < 7) {
    perror("TouchUpdatedFile write");
    exit(1);
  }

  close(fd);
} /* TouchUpdateFile */


int main(int argc, char **argv)
{
  int done = 0;
  int nl80211_id = -1;
  struct nl_sock *nlsk = NULL;
  struct rlimit rlim;

  memset(&rlim, 0, sizeof(rlim));
  if (getrlimit(RLIMIT_AS, &rlim)) {
    perror("getrlimit RLIMIT_AS failed");
    exit(1);
  }
  rlim.rlim_cur = 6 * 1024 * 1024;
  if (setrlimit(RLIMIT_AS, &rlim)) {
    perror("getrlimit RLIMIT_AS failed");
    exit(1);
  }

  setlinebuf(stdout);

  nlsk = InitNetlinkSocket();
  if (nl_socket_modify_cb(nlsk, NL_CB_FINISH, NL_CB_CUSTOM, NlFinish, &done)) {
    fprintf(stderr, "nl_socket_modify_cb failed\n");
    exit(1);
  }
  if ((nl80211_id = genl_ctrl_resolve(nlsk, "nl80211")) < 0) {
    fprintf(stderr, "genl_ctrl_resolve failed\n");
    exit(1);
  }

  while (1) {
    int i, j;

    /* Check if new interfaces have appeared */
    ninterfaces = 0;
    memset(interfaces, 0, sizeof(interfaces));
    RequestInterfaceList(nlsk, nl80211_id);
    ProcessNetlinkMessages(nlsk, &done);
    for (i = 0; i < ninterfaces; ++i) {
      UpdateWifiShow(nlsk, nl80211_id, i);
    }

    /* Accumulate MAX_SAMPLE_INDEX samples between calls to
     * LogAssociatedDevices() */
    for (i = 0; i < MAX_SAMPLE_INDEX; ++i) {
      sleep(2);
      for (j = 0; j < ninterfaces; ++j) {
        done = 0;
        RequestAssociatedDevices(nlsk, nl80211_id, j);
        ProcessNetlinkMessages(nlsk, &done);
        ConsolidateAssociatedDevices();
        UpdateAssociatedDevices();

        done = 0;
        RequestInterfaceInfo(nlsk, nl80211_id, j);
        ProcessNetlinkMessages(nlsk, &done);
      }
      TouchUpdateFile();
    }
    LogAssociatedDevices();
    LogInterfaces();
  }

  exit(0);
}
#endif  /* UNIT_TESTS */
