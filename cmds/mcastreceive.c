// Copyright 2008 Google Inc. All Rights Reserved.
// Author: Niel Markwick (nielm@google.com)
// Put into google3 by brucefan@google.com

#include <sys/types.h>  /* for type definitions */
#include <sys/socket.h> /* for socket API calls */
#include <sys/select.h>
#include <netinet/in.h> /* for address structs */
#include <arpa/inet.h>  /* for sockaddr_in */
#include <stdio.h>      /* for printf() and fprintf() */
#include <stdlib.h>     /* for atoi() */
#include <string.h>     /* for strlen() */
#include <unistd.h>     /* for close() */
#include <time.h>
#include <fcntl.h>

#define MAX_LEN  2048   /* maximum receive string size */
#define MIN_PORT 1024   /* minimum port allowed */
#define MAX_PORT 65535  /* maximum port allowed */

#define RTP_VERSION          2  /* version as defined in RFC-3550 */
#define RTP_HDR_SIZE        12
#define TS_PACKET_SIZE     188

typedef enum {
  PACKET_HDR_FORMAT_UNKNOWN = 0,  // not RTP and not plain TS, probably
                                  // corrupted
  PACKET_HDR_FORMAT_NONE,         // plain TS, no extra payload header
  PACKET_HDR_FORMAT_RTP           // RTP header
} packet_hdr_format_t;

const char *get_hdr_format_str(packet_hdr_format_t hdr) {
  switch(hdr) {
    case PACKET_HDR_FORMAT_RTP:
      return "RTP";
    case PACKET_HDR_FORMAT_NONE:
      return "Plain-TS";
    case PACKET_HDR_FORMAT_UNKNOWN:
    default:
      return "Unknown";
  }
}

void printhelp(char *argv[]) {
  fprintf(stderr,
          "Usage: \n"
          "%s <IP> <Port> [-d <paks-per-dot>] [-n <npaks>] [-t <timeout>]\n"
          "               [-c <ts-file>] [-u <udp-file>]\n"
          "    paks-per-dot: print a dot every time this number of packets\n"
          "             is received - disable with 0\n"
          "    npaks: exit with status 0 after this many packets have been\n"
          "             received\n"
          "    timeout: exit with error status after this many seconds have\n"
          "             elapsed --\n"
          "             exit with status 2 if 0 packets received \n"
          "             exit with status 1 if some packets received\n"
          "    ts-file: save received TS packets (i.e., UDP or RTP payload)\n"
          "             into this file\n"
          "    udp-file: save UDP payload into this file, with each packet\n"
          "             prefixed by its length: <len1> + <UDP-payload1>,\n"
          "             <len2> + <udp-payload2>, ..\n"
          "             This allows to identify individual UDP packet\n"
          "             boundaries.\n"
          "\n"
          " Note: Presence of RTP headers is handled automatically and the\n"
          "       headers are removed for the ts-file output but retained in\n"
          "       the udp-file."
          "\n"
          "Examples:\n"
          "  %s 225.0.0.100 2000 -d 100\n"
          "       run forever monitoring this multicast stream \n"
          "       printing a dot for every 100 packets\n"
          "  %s 225.0.0.100 2000 -d 0 -n 100 -t 60 -c cap.ts\n"
          "       test this multicast stream, printing no dots,\n"
          "       returning an error if 100 packets are not received in\n"
          "       60 seconds and storing the captured data in cap.ts\n",
          argv[0],argv[0],argv[0]);
}

static inline packet_hdr_format_t get_packet_hdr_format(uint8_t first_byte,
                                                        int size) {
  int has_rtp = (first_byte >> 6) == RTP_VERSION &&
    ((size - RTP_HDR_SIZE) % TS_PACKET_SIZE) == 0;
  int is_plain_ts = first_byte == 0x47 && (size % TS_PACKET_SIZE) == 0;

  if (has_rtp)
    return PACKET_HDR_FORMAT_RTP;
  else if (is_plain_ts)
    return PACKET_HDR_FORMAT_NONE;
  return PACKET_HDR_FORMAT_UNKNOWN;
}


int main(int argc, char *argv[]) {

  int sock;                     /* socket descriptor */
  int flag_on = 1;              /* socket option flag */
  int recv_buf_size = 1024*1024;/* maximum socket receive buffer in bytes */
  struct sockaddr_in mc_addr;   /* socket address structure */
  char recvBuff[MAX_LEN+1];     /* buffer to receive string */
  int recv_len;                 /* length of string received */
  struct ip_mreq mc_req;        /* multicast request structure */
  char* mc_addr_str;            /* multicast IP address */
  unsigned int mc_port;       /* multicast port */
  struct sockaddr_in from_addr; /* packet source */
  unsigned int from_len;        /* source addr length */
  int packets_per_dot=1;
  int num_packets=0;
  int npacketsLimit=0;
  int timeoutSecs=0;
  const char* ts_filename = NULL, *udp_filename = NULL;
  int fd_ts = -1, fd_udp = -1;

  /* for select() */
  fd_set socks;
  int readSocks=0;
  struct timeval timeout;

  int opt;
  while ((opt = getopt(argc, argv, "?hd:n:t:c:u:")) != -1) {
    switch (opt) {
    case '?':
    case 'h':
      printhelp(argv);
      exit(0);
    case 'd':
      packets_per_dot = atoi(optarg);
      if ( packets_per_dot < 0 ) {
        fprintf(stderr,"invalid value for packets_per_dot");
        printhelp(argv);
        exit(4);
      }
      break;
    case 'n':
      npacketsLimit=atoi(optarg);
      if ( npacketsLimit < 0 ) {
        fprintf(stderr,"invalid value for npackets");
        printhelp(argv);
        exit(4);
      }
      break;
    case 't':
      timeoutSecs=atoi(optarg);
      if ( timeoutSecs < 0 ) {
        fprintf(stderr,"invalid value for timeoutSecs");
        printhelp(argv);
        exit(4);
      }
      break;
    case 'c':
      ts_filename = optarg;
      break;
    case 'u':
      udp_filename = optarg;
      break;
    }
  }

  if ( optind + 2 != argc ) {
    fprintf(stderr, "Missing either <IP> or <Port>\n");
    printhelp(argv);
    exit(4);
  }

  mc_addr_str = argv[optind];        /* multicast ip address */
  mc_port = atoi(argv[optind + 1]);  /* multicast port number */
  /* validate the port range */
  if ((mc_port < MIN_PORT) || (mc_port > MAX_PORT)) {
    fprintf(stderr, "Invalid port number argument %d.\n", mc_port);
    fprintf(stderr, "Valid range is between %d and %d.\n", MIN_PORT, MAX_PORT);
    exit(4);
  }

  printf("Running with these configs:\nmcast-addr:%s:%d paks-per-dot:%d "
         "npaklimit:%d timeout:%d ts_filename:%s updFilename:%s\n", mc_addr_str,
         mc_port, packets_per_dot, npacketsLimit, timeoutSecs, ts_filename,
         udp_filename);

  if (ts_filename) {
    fd_ts = open(ts_filename, O_WRONLY | O_EXCL | O_CREAT, 0666);
    if (fd_ts < 0) {
      perror("Error opening <ts_filename>");
    }
  }
  if (udp_filename) {
    fd_udp = open(udp_filename, O_WRONLY | O_EXCL | O_CREAT, 0666);
    if (fd_udp < 0) {
      perror("Error opening <udp_filename>");
    }
  }

  /* create socket to join multicast group on */
  if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    perror("socket() failed");
    exit(4);
  }

  /* set reuse port to on to allow multiple binds per host */
  if ((setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag_on,
                  sizeof(flag_on))) < 0) {
    perror("setsockopt() failed");
    exit(4);
  }

  if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &recv_buf_size,
                  sizeof(recv_buf_size)) != 0 ) {
    perror("setsockopt() failed");
    exit(4);
  }

  /* construct a multicast address structure */
  memset(&mc_addr, 0, sizeof(mc_addr));
  mc_addr.sin_family      = AF_INET;
  mc_addr.sin_addr.s_addr = inet_addr(mc_addr_str);
  mc_addr.sin_port        = htons(mc_port);

  /* bind to multicast address to socket */
  if ((bind(sock, (struct sockaddr *) &mc_addr,
            sizeof(mc_addr))) < 0) {
    perror("bind() failed");
    exit(4);
  }

  /* construct an IGMP join request structure */
  mc_req.imr_multiaddr.s_addr = inet_addr(mc_addr_str);
  mc_req.imr_interface.s_addr = htonl(INADDR_ANY);

  /* send an ADD MEMBERSHIP message via setsockopt */
  if ((setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                  (void*) &mc_req, sizeof(mc_req))) < 0) {
    perror("setsockopt() failed");
    exit(4);
  }
  /* clear the receive buffers & structs */
  memset(recvBuff, 0, sizeof(recvBuff));
  from_len = sizeof(from_addr);
  memset(&from_addr, 0, from_len);

  time_t startTime=time(NULL);
  packet_hdr_format_t last_hdr_fmt = PACKET_HDR_FORMAT_UNKNOWN, cur_hdr_fmt;

  for (;;) {          /* loop forever */
    FD_ZERO(&socks);
    FD_SET(sock,&socks);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    readSocks=select(sock+1,&socks,NULL,NULL,&timeout);

    if ( readSocks > 0 ) {

      /* read one UDP packet */
      if ((recv_len = recvfrom(sock, recvBuff, MAX_LEN, 0,
                               (struct sockaddr*)&from_addr, &from_len)) < 0) {
        perror("recvfrom() failed");
        exit(4);
      }

      num_packets++;
      if (packets_per_dot>0 && ( num_packets % packets_per_dot == 0 ) ) {
        putchar('.');
        if ( (num_packets/packets_per_dot)%80==0)
          putchar('\n');
        fflush(stdout);
      }
      if (npacketsLimit > 0 && num_packets >= npacketsLimit) {
        printf("exiting: %d packets received\n",num_packets);
        exit(0);
      }

      // handle RTP header
      cur_hdr_fmt = get_packet_hdr_format((uint8_t)recvBuff[0], recv_len);
      if (last_hdr_fmt != cur_hdr_fmt) {
        printf("Payload format changed:%s -> %s\n",
               get_hdr_format_str(last_hdr_fmt),
               get_hdr_format_str(cur_hdr_fmt));
        last_hdr_fmt = cur_hdr_fmt;
      }
      char *ts_payload_ptr = recvBuff;
      int ts_payload_len = recv_len;
      if (cur_hdr_fmt == PACKET_HDR_FORMAT_RTP) {
        // strip RTP header
        ts_payload_ptr += RTP_HDR_SIZE;
        ts_payload_len -= RTP_HDR_SIZE;
      }

      if (fd_ts >= 0) {
        /* write TS packets */
        int l = write(fd_ts, ts_payload_ptr, ts_payload_len);
        if (l < ts_payload_len) {
          fprintf(stderr, "Warning-Wrote only %d/%d ts-bytes, stop writing "
                  "ts-file!\n", l, ts_payload_len);
          close(fd_ts);
          fd_ts = -1;
        }
      }
      if (fd_udp >= 0) {
        /* write payload length */
        int l = write(fd_udp, &recv_len, sizeof(recv_len));
        if (l < (int)sizeof(recv_len)) {
          fprintf(stderr, "Warning-Wrote only %d/%d bytes, stop writing "
                  "udp-file!\n", l, (int)sizeof(recv_len));
          close(fd_udp);
          fd_udp = -1;
        } else {
          /* write payload */
          l = write(fd_udp, recvBuff, recv_len);
          if (l < recv_len) {
            fprintf(stderr, "Warning-Wrote only %d/%d udp-bytes, stop writing "
                    "udp-file!\n", l, recv_len);
            close(fd_udp);
            fd_udp = -1;
          }
        }
      }
    } else if ( readSocks == 0 ) {
      /* timeout */
      if (timeoutSecs>0 && time(NULL) >= startTime+timeoutSecs ){
        printf("timeout: %d packets received\n",num_packets);
        if ( num_packets > 0){
          exit(1);
        } else {
          exit(2);
        }
      }

    } else if (readSocks < 0) {
      perror("select");
      exit(4);
    }
  }
}
