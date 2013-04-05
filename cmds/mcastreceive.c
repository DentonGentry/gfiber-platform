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

#define MAX_LEN  1024   /* maximum receive string size */
#define MIN_PORT 1024   /* minimum port allowed */
#define MAX_PORT 65535  /* maximum port allowed */

void printhelp(char *argv[]) {
  fprintf(stderr,
          "Usage: %s <IP> <Port> <packets-per-dot> [npackets] [timeout-secs]\n"
          "    packets-per-dot: print a dot every time this number of packets is received\n"
          "                     - disable with 0\n"
          "    npackets: exit with status 0 after this many packets have been received\n"
          "    timeout-secs: exit with error status after this many seconds have elapsed --\n"
          "                  exit with status 2 if 0 packets received \n"
          "                  exit with status 1 if some packets received\n"
          "\n"
          "Examples:\n"
          "  %s 225.0.0.100 2000 100\n"
          "       run forever monitoring this multicast stream \n"
          "       printing a dot for every 100 packets\n"
          "  %s 225.0.0.100 2000 0 100 60\n"
          "       test this multicast stream\n"
          "       returning an error if 100 packets are not received in 60 seconds\n",
          argv[0],argv[0],argv[0]);
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

  /* for select() */
  fd_set socks;
  int readSocks=0;
  struct timeval timeout;

  /* validate number of arguments */
  if (argc < 4) {
    printhelp(argv);
    exit(4);
  }

  mc_addr_str = argv[1];      /* arg 1: multicast ip address */
  mc_port = atoi(argv[2]);    /* arg 2: multicast port number */
  packets_per_dot = atoi(argv[3]);
  if ( packets_per_dot < 0 ) {
    fprintf(stderr,"invalid value for packets_per_dot");
    printhelp(argv);
    exit(4);
  }

  if ( argc > 4 )
    npacketsLimit=atoi(argv[4]);
  if ( npacketsLimit < 0 ) {
    fprintf(stderr,"invalid value for npackets");
    printhelp(argv);
    exit(4);
  }


  if ( argc > 5 )
    timeoutSecs=atoi(argv[5]);

  if ( timeoutSecs < 0 ) {
    fprintf(stderr,"invalid value for timeoutSecs");
    printhelp(argv);
    exit(4);
  }

  /* validate the port range */
  if ((mc_port < MIN_PORT) || (mc_port > MAX_PORT)) {
    fprintf(stderr, "Invalid port number argument %d.\n",
            mc_port);
    fprintf(stderr, "Valid range is between %d and %d.\n",
            MIN_PORT, MAX_PORT);
    exit(4);
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

  for (;;) {          /* loop forever */
    FD_ZERO(&socks);
    FD_SET(sock,&socks);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    readSocks=select(sock+1,&socks,NULL,NULL,&timeout);

    if ( readSocks > 0 ) {
      /* read a packet */

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
