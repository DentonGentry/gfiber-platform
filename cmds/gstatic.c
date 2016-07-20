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

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* Request a simple URL from all the known gstatic.com IP/IPv6 addresses. */

#define HOSTNAME "gstatic.com"
#define PORT "80"
#define TIMEOUT_MS 3000

#define HTTP_REQUEST \
    "GET /generate_204 HTTP/1.0\r\n" \
    "User-Agent: gfiber-cpe-gstatic\r\n" \
    "\r\n"

#define BUFLEN 128
#define MAX_ADDRS 128

void perror_die(const char *msg)
{
  perror(msg);
  exit(1);
}

void socket_set_blocking(int fd, bool blocking)
{
  int flags;
  int rc;

  flags = fcntl(fd, F_GETFL, 0);
  if (blocking)
    flags &= ~O_NONBLOCK;
  else
    flags |= O_NONBLOCK;
  rc = fcntl(fd, F_SETFL, flags);
  if (rc < 0)
    perror_die("fcntl");
}

struct timespec timespec_diff(struct timespec *start, struct timespec *end)
{
  struct timespec diff;
  if (end->tv_nsec > start->tv_nsec) {
    diff.tv_sec = end->tv_sec - start->tv_sec - 1;
    diff.tv_nsec = 1000000000 + end->tv_nsec - start->tv_nsec;
  } else {
    diff.tv_sec = end->tv_sec - start->tv_sec;
    diff.tv_nsec = end->tv_nsec - start->tv_nsec;
  }
  return diff;
}

ssize_t xwrite(int fd, const char *buf, size_t count)
{
  size_t total;
  ssize_t rc;

  total = 0;
  while (total < count) {
    rc = write(fd, buf + total, count - total);
    if (rc < 0) {
      perror("write");
      return -1;
    } else if (rc == 0) {
      fprintf(stderr, "write: EOF\n");
      return total;
    } else
      total += rc;
  }

  return total;
}

const char *inet_ntop46(const struct addrinfo *addr, char *buf, size_t size)
{
  switch (addr->ai_family) {
  case AF_INET:
    inet_ntop(AF_INET, &(((struct sockaddr_in *)(addr->ai_addr))->sin_addr),
        buf, size);
    break;
  case AF_INET6:
    inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)(addr->ai_addr))->sin6_addr),
        buf, size);
    break;
  default:
    assert(!"Unknown Address Family");
  }

  return buf;
}

int connect_timeout(int fd, struct addrinfo *addr, int timeout_ms)
{
  struct timeval timeout;
  fd_set writeset;
  socklen_t len;
  int error;
  int rc;

  socket_set_blocking(fd, false);

  rc = connect(fd, addr->ai_addr, addr->ai_addrlen);
  if (rc < 0 && errno != EINPROGRESS) {
    perror("connect");
    return -1;
  }

  if (rc != 0) {
    FD_ZERO(&writeset);
    FD_SET(fd, &writeset);
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    rc = select(fd + 1, NULL, &writeset, NULL, &timeout);
    if (rc < 0) {
      perror("connect-select");
      return -1;
    } else if (rc == 0) {
      /* timeout */
      return -1;
    }

    len = sizeof(error);
    rc = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
    if (rc < 0)
      perror_die("getsockopt");
    else if (error != 0)
      return -1;
  }

  socket_set_blocking(fd, true);
  return fd;
}

ssize_t read_timeout(int fd, void *buf, size_t count, int timeout_ms)
{
  fd_set readset;
  struct timeval timeout;
  int rc;

  FD_ZERO(&readset);
  FD_SET(fd, &readset);
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_usec = (timeout_ms % 1000) * 1000;

  rc = select(fd + 1, &readset, NULL, NULL, &timeout);
  if (rc < 0) {
    perror("select");
    return -1;
  } else if (rc == 0) {
    fprintf(stderr, "select: timed out\n");
    return -1;
  }

  return read(fd, buf, count);
}

int do_http_request(int fd, struct addrinfo *addr)
{
  char http_response[BUFLEN];
  char ip[BUFLEN];
  struct timespec start, end;
  struct timespec diff;
  float elapsed_ms;
  int rc;

  inet_ntop46(addr, ip, sizeof(ip));
  clock_gettime(CLOCK_MONOTONIC, &start);

  rc = connect_timeout(fd, addr, TIMEOUT_MS);
  if (rc < 0)
    goto err;

  rc = xwrite(fd, HTTP_REQUEST, sizeof(HTTP_REQUEST));
  if (rc < (ssize_t)sizeof(HTTP_REQUEST))
    goto err;

  rc = read_timeout(fd, http_response, sizeof(http_response), TIMEOUT_MS);
  if (rc < 0)
    goto err;

  clock_gettime(CLOCK_MONOTONIC, &end);
  diff = timespec_diff(&start, &end);
  elapsed_ms = (diff.tv_sec * 1000) + (diff.tv_nsec / 1000000.0f);

  printf("%s %.1fms\n", ip, elapsed_ms);
  return 0;

err:
  printf("%s ERR\n", ip);
  return 1;
}

int main(int argc, const char **argv)
{
  struct addrinfo *res, *result;
  struct addrinfo hints;
  int bad;
  int rc;

  // In case we get stuck in one of the blocking syscalls (write, read, etc)
  alarm(60);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  rc = getaddrinfo(HOSTNAME, PORT, &hints, &result);
  if (rc) {
    if (rc == EAI_SYSTEM)
      fprintf(stderr, "%s: DNS-ERR (%s)\n", HOSTNAME, strerror(errno));
    else
      fprintf(stderr, "%s: DNS-ERR (%s)\n", HOSTNAME, gai_strerror(rc));
    exit(1);
  }

  bad = 0;
  for (res = result; res != NULL; res = res->ai_next) {
    int fd;

    fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
      perror("socket");
      bad = 1;
      continue;
    }

    rc = do_http_request(fd, res);
    if (rc != 0)
      bad = 1;

    close(fd);
  }

  freeaddrinfo(result);
  return bad;
}
