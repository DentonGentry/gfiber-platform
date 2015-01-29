/*
 * Copyright 2015 Google Inc. All rights reserved.
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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/net.h>

#define MAX_INTERFACES 10

static int dvb_add_netif(int netfd, int pid, int ule) {
  struct dvb_net_if p = {0};
  p.pid = pid;
  p.feedtype = ule ? DVB_NET_FEEDTYPE_ULE : DVB_NET_FEEDTYPE_MPE;
  return ioctl(netfd, NET_ADD_IF, &p);
}

static int dvb_remove_netif(int netfd, int if_num) {
  return ioctl(netfd, NET_REMOVE_IF, if_num);
}

static int dvb_get_netif(int netfd, int if_num, int* pid, int* feedtype) {
  int err;
  struct dvb_net_if p = {0};
  p.if_num = if_num;

  if (!pid || !feedtype) {
    return -EINVAL;
  }

  err = ioctl(netfd, NET_GET_IF, &p);
  if (err >= 0) {
    *pid = p.pid;
    *feedtype = p.feedtype;
  }

  return err;
}

static int dvb_open(int adapter, int device, const char* type) {
  char s[100];
  if (snprintf(s, sizeof(s), "/dev/dvb/adapter%d/%s%d",
               adapter, type, device) < 0) {
    return -EINVAL;
  }
  return open(s, O_RDWR);
}

static void usage(const char* prog) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "  %s [options] -p <PID> [-u]\n", prog);
  fprintf(stderr, "               Add network interfaces\n");
  fprintf(stderr, "  %s [options] -r <Number>\n", prog);
  fprintf(stderr, "               Remove network interfaces\n");
  fprintf(stderr, "  %s [options] -l\n", prog);
  fprintf(stderr, "               List network interfaces\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "    -a Adapter Adapter device (default 0)\n");
  fprintf(stderr, "    -d Network Network device (default 0)\n");
  fprintf(stderr, "    -p PID     Program ID (0 - 0x2000)\n");
  fprintf(stderr, "    -s Size    Demux buffer size\n");
  fprintf(stderr, "    -u         Use ULE instead of MPE\n");
  fprintf(stderr, "    -r Number  Network interface number\n");
  exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
  int err = 0;
  int opt;
  int adapter = 0;
  int net = 0;
  int pid = -1;
  int if_num = 0;
  int list = 0;
  int remove = 0;
  int ule = 0;
  int netfd = 0;

  while ((opt = getopt(argc, argv, "a:d:p:r:luUh")) != -1) {
    switch (opt) {
      case 'a':
        adapter = atoi(optarg);
        break;
      case 'd':
        net = atoi(optarg);
        break;
      case 'l':
        list = 1;
        break;
      case 'p':
        pid = atoi(optarg);
        break;
      case 'r':
        remove = 1;
        if_num = atoi(optarg);
        break;
      case 'U':
      case 'u':
        ule = 1;
        break;
      default:
        usage(argv[0]);
    }
  }

  if ((!list && !remove) && (pid < 0 || pid > 0x2000)) {
    usage(argv[0]);
  }

  netfd = dvb_open(adapter, net, "net");
  if (netfd < 0) {
    return 1;
  }

  if (list) {
    int i;
    for (i = 0; i < MAX_INTERFACES; ++i) {
      const char* encap = "MPE";
      err = dvb_get_netif(netfd, i, &pid, &ule);
      if (err < 0) {
        continue;
      }
      if (ule == DVB_NET_FEEDTYPE_ULE) {
        encap = "ULE";
      }
      printf("dvb%d_%d PID %d encapsulation %s\n", net, i, pid, encap);
    }
    if (i != 0) {
      err = 0;
    }
  } else if (remove) {
    err = dvb_remove_netif(netfd, if_num);
    if (err < 0) {
      perror("NET_REMOVE_IF");
    }
  } else {
    err = dvb_add_netif(netfd, pid, ule);
    if (err < 0) {
      perror("NET_ADD_IF");
    }
  }

  close(netfd);

  return err;
}
