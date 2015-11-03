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
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <linux/if_ether.h>
#include <linux/nl80211.h>
#include <math.h>
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
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>


#define STATIONS_DIR "/tmp/stations"
#define WIFISHOW_DIR "/tmp/wifi/wifiinfo"


typedef struct client_state {
  double inactive_since;
} client_state_t;


/* List of wifi interfaces in the system. */
#define NINTERFACES 16
int ifindexes[NINTERFACES];
const char *interfaces[NINTERFACES];
int ninterfaces = 0;

static FILE *wifi_show_handle = NULL;

int GetIfIndex(const char *ifname)
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


static int InterfaceListCallback(struct nl_msg *msg, void *arg)
{
  struct nlattr *il[NL80211_ATTR_MAX + 1];
  struct genlmsghdr *gh = nlmsg_data(nlmsg_hdr(msg));

  nla_parse(il, NL80211_ATTR_MAX, genlmsg_attrdata(gh, 0),
      genlmsg_attrlen(gh, 0), NULL);

  if (il[NL80211_ATTR_IFNAME]) {
    const char *name = nla_get_string(il[NL80211_ATTR_IFNAME]);
    interfaces[ninterfaces] = strdup(name);
    ifindexes[ninterfaces] = GetIfIndex(name);
    ninterfaces++;
  }

  return NL_OK;
}


void HandleNLCommand(struct nl_sock *nlsk, int nl80211_id, int n,
                     int cb(struct nl_msg *, void *),
                     int cmd, int flag)
{
  struct nl_msg *msg;
  int ifindex = n >= 0 ? ifindexes[n] : -1;
  const char *ifname = n>=0 ? interfaces[n] : NULL;

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

  if (nl_send_auto(nlsk, msg) < 0) {
    fprintf(stderr, "nl_send_auto failed\n");
    exit(1);
  }
  nlmsg_free(msg);
}


void RequestInterfaceList(struct nl_sock *nlsk, int nl80211_id)
{
  HandleNLCommand(nlsk, nl80211_id, -1, InterfaceListCallback,
                  NL80211_CMD_GET_INTERFACE, NLM_F_DUMP);
}  /* RequestInterfaceList */


uint32_t GetBitrate(struct nlattr *attr)
{
  int rate = 0;
  struct nlattr *ri[NL80211_RATE_INFO_MAX + 1];
  static struct nla_policy rate_policy[NL80211_RATE_INFO_MAX + 1] = {
    [NL80211_RATE_INFO_BITRATE] = { .type = NLA_U16 },
  };

  if (nla_parse_nested(ri, NL80211_RATE_INFO_MAX, attr, rate_policy)) {
    fprintf(stderr, "nla_parse_nested NL80211_RATE_INFO_MAX failed");
    return 0;
  }

  if (ri[NL80211_RATE_INFO_BITRATE]) {
    rate = nla_get_u16(ri[NL80211_RATE_INFO_BITRATE]);
  }

  return rate;
}

static int StationDumpCallback(struct nl_msg *msg, void *arg)
{
  const char *ifname = (const char *)arg;
  char tmpfile[PATH_MAX];
  char filename[PATH_MAX];
  FILE *f;
  struct genlmsghdr *gh = nlmsg_data(nlmsg_hdr(msg));
  struct nlattr *tb[NL80211_ATTR_MAX + 1] = {0};
  struct nlattr *si[NL80211_STA_INFO_MAX + 1] = {0};
  uint8_t *mac;
  char macstr[18];
  static struct nla_policy stats_policy[NL80211_STA_INFO_MAX + 1] = {
    [NL80211_STA_INFO_INACTIVE_TIME] = { .type = NLA_U32 },
    [NL80211_STA_INFO_RX_BITRATE] = { .type = NLA_NESTED },
    [NL80211_STA_INFO_RX_BYTES] = { .type = NLA_U32 },
    [NL80211_STA_INFO_RX_PACKETS] = { .type = NLA_U32 },
    [NL80211_STA_INFO_TX_BITRATE] = { .type = NLA_NESTED },
    [NL80211_STA_INFO_TX_BYTES] = { .type = NLA_U32 },
    [NL80211_STA_INFO_TX_PACKETS] = { .type = NLA_U32 },
    [NL80211_STA_INFO_TX_RETRIES] = { .type = NLA_U32 },
    [NL80211_STA_INFO_TX_FAILED] = { .type = NLA_U32 },
    [NL80211_STA_INFO_SIGNAL] = { .type = NLA_U8 },
    [NL80211_STA_INFO_SIGNAL_AVG] = { .type = NLA_U8 },
    [NL80211_STA_INFO_STA_FLAGS] = {
      .minlen = sizeof(struct nl80211_sta_flag_update) },

#ifdef NL80211_RECENT_FIELDS
    [NL80211_STA_INFO_RX_BYTES64] = { .type = NLA_U64 },
    [NL80211_STA_INFO_RX_DROP_MISC] = { .type = NLA_U64 },
    [NL80211_STA_INFO_TX_BYTES64] = { .type = NLA_U64 },
    [NL80211_STA_INFO_EXPECTED_THROUGHPUT] = { .type = NLA_U32 },
#endif
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

  mac = (uint8_t *)nla_data(tb[NL80211_ATTR_MAC]);
  snprintf(macstr, sizeof(macstr), "%02x:%02x:%02x:%02x:%02x:%02x",
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  snprintf(tmpfile, sizeof(tmpfile), "%s/%s.new", STATIONS_DIR, macstr);
  snprintf(filename, sizeof(filename), "%s/%s", STATIONS_DIR, macstr);

  if ((f = fopen(tmpfile, "w+")) == NULL) {
    perror("fopen");
    return NL_SKIP;
  }

  fprintf(f, "{\n");

  if (si[NL80211_STA_INFO_INACTIVE_TIME]) {
    ENTRY e, *ep;
    uint32_t inactive = nla_get_u32(si[NL80211_STA_INFO_INACTIVE_TIME]);
    double inactive_since = time(NULL) - ((double)inactive / 1000.0);

    memset(&e, 0, sizeof(e));
    e.key = macstr;
    if ((ep = hsearch(e, FIND)) != NULL) {
      client_state_t *prev = (client_state_t *)ep->data;
      if ((fabs(inactive_since - prev->inactive_since)) > 2.0) {
        prev->inactive_since = inactive_since;
      } else {
        inactive_since = prev->inactive_since;
      }
    } else {
      client_state_t *state = (client_state_t *)malloc(sizeof(client_state_t));

      state->inactive_since = inactive_since;
      memset(&e, 0, sizeof(e));
      e.key = strdup(macstr);
      e.data = (void *)state;
      if (hsearch(e, ENTER) == NULL) {
        fprintf(stderr, "hsearch(ENTER) failed\n");
        exit(1);  // rely on babysitter to restart us.
      }
    }

    fprintf(f, "  \"inactive since\": %.3f,\n", inactive_since);
    fprintf(f, "  \"inactive msec\": %u,\n", inactive);
  }

  if (si[NL80211_STA_INFO_RX_BITRATE]) {
    uint32_t rate = GetBitrate(si[NL80211_STA_INFO_RX_BITRATE]);
    if (rate) {
      fprintf(f, "  \"rx bitrate\": %u.%u,\n", rate / 10, rate % 10);
    }
  }
  if (si[NL80211_STA_INFO_RX_BYTES])
    fprintf(f, "  \"rx bytes\": %u,\n",
           nla_get_u32(si[NL80211_STA_INFO_RX_BYTES]));
  if (si[NL80211_STA_INFO_RX_PACKETS])
    fprintf(f, "  \"rx packets\": %u,\n",
           nla_get_u32(si[NL80211_STA_INFO_RX_PACKETS]));
  if (si[NL80211_STA_INFO_TX_BITRATE]) {
    uint32_t rate = GetBitrate(si[NL80211_STA_INFO_TX_BITRATE]);
    if (rate) {
      fprintf(f, "  \"tx bitrate\": %u.%u,\n", rate / 10, rate % 10);
    }
  }
  if (si[NL80211_STA_INFO_TX_BYTES])
    fprintf(f, "  \"tx bytes\": %u,\n",
           nla_get_u32(si[NL80211_STA_INFO_TX_BYTES]));
  if (si[NL80211_STA_INFO_TX_PACKETS])
    fprintf(f, "  \"tx packets\": %u,\n",
           nla_get_u32(si[NL80211_STA_INFO_TX_PACKETS]));
  if (si[NL80211_STA_INFO_TX_RETRIES])
    fprintf(f, "  \"tx retries\": %u,\n",
           nla_get_u32(si[NL80211_STA_INFO_TX_RETRIES]));
  if (si[NL80211_STA_INFO_TX_FAILED])
    fprintf(f, "  \"tx failed\": %u,\n",
           nla_get_u32(si[NL80211_STA_INFO_TX_FAILED]));

  if (si[NL80211_STA_INFO_SIGNAL]) {
    fprintf(f, "  \"signal\": %hhd,\n",
           (int8_t)nla_get_u8(si[NL80211_STA_INFO_SIGNAL]));
  }

  if (si[NL80211_STA_INFO_SIGNAL_AVG]) {
    fprintf(f, "  \"signal avg\": %hhd,\n",
           (int8_t)nla_get_u8(si[NL80211_STA_INFO_SIGNAL_AVG]));
  }

  if (si[NL80211_STA_INFO_STA_FLAGS]) {
    struct nl80211_sta_flag_update *sta_flags;
    sta_flags = (struct nl80211_sta_flag_update *)nla_data(
        si[NL80211_STA_INFO_STA_FLAGS]);
    #define BIT(x) (1ULL<<(x))
    #define PRINT_BOOL(name, bit) if (sta_flags->mask & BIT(bit)) \
      fprintf(f, "  \"%s\": \"%s\",\n", name, \
             (sta_flags->set & BIT(bit) ? "yes" : "no"));

    PRINT_BOOL("authorized", NL80211_STA_FLAG_AUTHORIZED);
    PRINT_BOOL("authenticated", NL80211_STA_FLAG_AUTHENTICATED);
    PRINT_BOOL("preamble", NL80211_STA_FLAG_SHORT_PREAMBLE);
    PRINT_BOOL("WMM/WME", NL80211_STA_FLAG_WME);
    PRINT_BOOL("MFP", NL80211_STA_FLAG_MFP);
    PRINT_BOOL("TDLS peer", NL80211_STA_FLAG_TDLS_PEER);

    if (sta_flags->mask & BIT(NL80211_STA_FLAG_SHORT_PREAMBLE)) {
      uint32_t bit = BIT(NL80211_STA_FLAG_SHORT_PREAMBLE);
      const char *p = (sta_flags->set & bit ?  "short" : "long");
      fprintf(f, "  \"preamble\": \"%s\",\n", p);
    }
  }

#ifdef NL80211_RECENT_FIELDS
  if (si[NL80211_STA_INFO_RX_BYTES64])
    fprintf(f, "  \"rx bytes64\": %" PRIu64 ",\n",
           nla_get_u64(si[NL80211_STA_INFO_RX_BYTES64]));
  if (si[NL80211_STA_INFO_RX_DROP_MISC])
    fprintf(f, "  \"rx drop64\": %" PRIu64 ",\n",
           nla_get_u64(si[NL80211_STA_INFO_RX_DROP_MISC]));
  if (si[NL80211_STA_INFO_TX_BYTES64])
    fprintf(f, "  \"tx bytes64\": %" PRIu64 ",\n",
           nla_get_u64(si[NL80211_STA_INFO_TX_BYTES64]));
  if (si[NL80211_STA_INFO_EXPECTED_THROUGHPUT]) {
    uint32_t thr = nla_get_u32(si[NL80211_STA_INFO_EXPECTED_THROUGHPUT]);
    fprintf(f, "  \"expected throughput\": \"%u.%uMbps\",\n",
           thr / 1000, thr % 1000);
  }
#endif

  fprintf(f, "  \"ifname\": \"%s\"\n", ifname);
  fprintf(f, "}\n");

  fclose(f);
  if (rename(tmpfile, filename)) {
    perror("rename");
  }
  return NL_OK;
}  /* StationDumpCallback */


void RequestAssociatedDevices(struct nl_sock *nlsk, int nl80211_id, int n)
{
  HandleNLCommand(nlsk, nl80211_id, n, StationDumpCallback,
                  NL80211_CMD_GET_STATION, NLM_F_DUMP);
}  /* RequestAssociatedDevices */

static int NlFinish(struct nl_msg *msg, void *arg)
{
  int *ret = arg;
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


void ProcessNetlinkMessages(struct nl_sock *nlsk, int *done)
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


void TouchUpdateFile()
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


void usage(const char *progname)
{
  printf("usage: %s\n", progname);
  printf("\tWill write files to /tmp/stations for Wifi clients.\n");
  exit(1);
}  /* usage */


/* From iw package, try untouched except indentation */
int ieee80211_frequency_to_channel(int freq)
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


void print_ssid_escaped(FILE* f, const uint8_t len, const uint8_t *data)
{
  int i;

  for (i = 0; i < len; i++) {
    if (isprint(data[i]) && data[i] != ' ' && data[i] != '\\')
      fprintf(f, "%c", data[i]);
    else if (data[i] == ' ' && (i != 0 && i != len -1))
      fprintf(f," ");
    else
      fprintf(f, "\\x%.2x", data[i]);
  }
}


static int WlanInfoCallback(struct nl_msg *msg, void *arg)
{
  struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
  struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];

  nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL);

  if (tb_msg[NL80211_ATTR_MAC]) {
    unsigned char *mac_addr = nla_data(tb_msg[NL80211_ATTR_MAC]);
    fprintf(wifi_show_handle,
            "  \"BSSID\": \"%02x:%02x:%02x:%02x:%02x:%02x\",\n",
            mac_addr[0], mac_addr[1], mac_addr[2],
            mac_addr[3], mac_addr[4], mac_addr[5]);
  }
  if (tb_msg[NL80211_ATTR_SSID]) {
    fprintf(wifi_show_handle, "  \"SSID\": \"");
    print_ssid_escaped(wifi_show_handle, nla_len(tb_msg[NL80211_ATTR_SSID]),
                       nla_data(tb_msg[NL80211_ATTR_SSID]));
    fprintf(wifi_show_handle, "\",\n");
  }
  if (tb_msg[NL80211_ATTR_WIPHY_FREQ]) {
    uint32_t freq = nla_get_u32(tb_msg[NL80211_ATTR_WIPHY_FREQ]);

    fprintf(wifi_show_handle, "  \"Channel\": %d",
            ieee80211_frequency_to_channel(freq));

    fprintf(wifi_show_handle, ",\n");
  }

  return NL_SKIP;
}


void UpdateWifiShowContent(struct nl_sock *nlsk, int nl80211_id, int n)
{
  HandleNLCommand(nlsk, nl80211_id, n, WlanInfoCallback,
                  NL80211_CMD_GET_INTERFACE, 0);
}


static int RegdomainCallback(struct nl_msg *msg, void *arg)
{
  struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
  struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
  char *alpha2;

  nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL);

  if (!tb_msg[NL80211_ATTR_REG_ALPHA2]) {
    return NL_SKIP;
  }

  if (!tb_msg[NL80211_ATTR_REG_RULES]) {
    return NL_SKIP;
  }

  alpha2 = nla_data(tb_msg[NL80211_ATTR_REG_ALPHA2]);
  fprintf(wifi_show_handle, "  \"RegDomain\": \"%c%c\",\n", alpha2[0], alpha2[1]);

  return NL_SKIP;
}


void UpdateWifiRegdomain(struct nl_sock *nlsk, int nl80211_id)
{
  HandleNLCommand(nlsk, nl80211_id, -1, RegdomainCallback,
                  NL80211_CMD_GET_REG, 0);
}


void UpdateWifiShow(struct nl_sock *nlsk, int nl80211_id, int n)
{
  char tmpfile[PATH_MAX];
  char filename[PATH_MAX];
  char autofile[PATH_MAX];
  const char *ifname = interfaces[n];
  int done = 0;
  struct stat buffer;
  FILE *fptr;

  if (!ifname || !ifname[0]) {
    return;
  }

  snprintf(tmpfile, sizeof(tmpfile), "%s/%s.new", WIFISHOW_DIR, ifname);
  snprintf(filename, sizeof(filename), "%s/%s", WIFISHOW_DIR, ifname);

  if ((wifi_show_handle = fopen(tmpfile, "w+")) == NULL) {
    perror("fopen");
    return;
  }

  fprintf(wifi_show_handle, "{\n");
  done = 0;
  UpdateWifiShowContent(nlsk, nl80211_id, n);
  ProcessNetlinkMessages(nlsk, &done);

  done = 0;
  UpdateWifiRegdomain(nlsk, nl80211_id);
  ProcessNetlinkMessages(nlsk, &done);

  snprintf(autofile, sizeof(autofile), "/tmp/autochan.%s", ifname);
  if (stat(autofile, &buffer) == 0) {
    fprintf(wifi_show_handle, "  \"AutoChannel\": true,\n");
  } else {
    fprintf(wifi_show_handle, "  \"AutoChannel\": false,\n");
  }
  snprintf(autofile, sizeof(autofile), "/tmp/autotype.%s", ifname);
  if ((fptr = fopen(autofile, "r")) == NULL) {
    fprintf(wifi_show_handle, "  \"AutoType\": \"LOW\"\n");
  } else {
    char buf[24];
    if (fgets(buf, sizeof(buf), fptr) != NULL)
      fprintf(wifi_show_handle, "  \"AutoType\": \"%s\"\n", buf);
  }
  fprintf(wifi_show_handle, "}\n");

  fclose(wifi_show_handle);
  wifi_show_handle = NULL;
  if (rename(tmpfile, filename)) {
    perror("rename");
  }
}

int main(int argc, char **argv)
{
  int done = 0;
  int nl80211_id = -1;
  struct nl_sock *nlsk = NULL;
  struct rlimit rlim;

  hcreate(512);

  memset(&rlim, 0, sizeof(rlim));
  if (getrlimit(RLIMIT_AS, &rlim)) {
    perror("getrlimit RLIMIT_AS failed");
    exit(1);
  }
  rlim.rlim_cur = 5 * 1024 * 1024;
  if (setrlimit(RLIMIT_AS, &rlim)) {
    perror("getrlimit RLIMIT_AS failed");
    exit(1);
  }

  nlsk = InitNetlinkSocket();
  if (nl_socket_modify_cb(nlsk, NL_CB_FINISH, NL_CB_CUSTOM, NlFinish, &done)) {
    fprintf(stderr, "nl_socket_modify_cb failed\n");
    exit(1);
  }
  if ((nl80211_id = genl_ctrl_resolve(nlsk, "nl80211")) < 0) {
    fprintf(stderr, "genl_ctrl_resolve failed\n");
    exit(1);
  }
  RequestInterfaceList(nlsk, nl80211_id);
  ProcessNetlinkMessages(nlsk, &done);

  while (1) {
    int i;
    for (i = 0; i < ninterfaces; i++) {
      done = 0;
      RequestAssociatedDevices(nlsk, nl80211_id, i);
      ProcessNetlinkMessages(nlsk, &done);

      UpdateWifiShow(nlsk, nl80211_id, i);
    }
    TouchUpdateFile();
    sleep(2);
  }

  exit(0);
}
