/*
 * Copyright 2016 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <netlink/msg.h>
#include <netlink/netlink.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "l2utils.h"

void get_l2_map(L2Map *l2map)
{
  int s;
  struct {
    struct nlmsghdr hdr;
    struct ndmsg msg;
  } nlreq;
  struct sockaddr_nl addr;
  struct msghdr msg;
  static uint8_t l2buf[256 * 1024];
  struct iovec iov = {.iov_base = l2buf, .iov_len = sizeof(l2buf)};
  struct nlmsghdr *nh;
  struct ndmsg *ndm;
  struct nlattr *tb[NDA_MAX+1];
  int len;
  int af[] = {AF_INET, AF_INET6};
  unsigned int i;

  for (i = 0; i < (sizeof(af) / sizeof(af[0])); ++i) {
    if ((s = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) < 0) {
      perror("socket AF_NETLINK");
      exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 0;

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
      perror("bind AF_NETLINK");
      exit(1);
    }

    memset(&nlreq, 0, sizeof(nlreq));
    nlreq.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(nlreq.msg));
    nlreq.hdr.nlmsg_type = RTM_GETNEIGH;
    nlreq.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    nlreq.msg.ndm_family = af[i];

    if (send(s, &nlreq, nlreq.hdr.nlmsg_len, 0) < 0) {
      perror("send AF_NETLINK");
      exit(1);
    }

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_controllen = 0;
    msg.msg_control = NULL;
    msg.msg_flags = 0;

    if ((len = recvmsg(s, &msg, 0)) <= 0) {
      perror("recvmsg AL_NETLINK");
      exit(1);
    }

    if (msg.msg_flags & MSG_TRUNC) {
      fprintf(stderr, "recvmsg AL_NETLINK MSG_TRUNC\n");
      exit(1);
    }

    memset(tb, 0, sizeof(tb));
    nh = (struct nlmsghdr *)l2buf;
    while (nlmsg_ok(nh, len)) {
      ndm = (struct ndmsg *)nlmsg_data(nh);
      if (nlmsg_parse(nh, sizeof(*ndm), tb, NDA_MAX, NULL)) {
        fprintf(stderr, "nlmsg_parse failed\n");
        exit(1);
      }

      if (tb[NDA_DST] && tb[NDA_LLADDR] &&
          !(ndm->ndm_state & (NUD_INCOMPLETE | NUD_FAILED)) &&
          (ndm->ndm_family == AF_INET || ndm->ndm_family == AF_INET6)) {
        char mac[18];
        char ipaddr[INET6_ADDRSTRLEN];
        uint8_t *p;

        p = (uint8_t *)nla_data(tb[NDA_LLADDR]);
        snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
            p[0], p[1], p[2], p[3], p[4], p[5]);

        p = (uint8_t *)nla_data(tb[NDA_DST]);
        inet_ntop(ndm->ndm_family, p, ipaddr, sizeof(ipaddr));

        (*l2map)[std::string(ipaddr)] = std::string(mac);
      }

      nh = nlmsg_next(nh, &len);
    }

    close(s);
  }
}
