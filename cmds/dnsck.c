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

#include <ares.h>
#include <arpa/nameser.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define RESOLV_CONF "/etc/resolv.conf"
#define RESOLV_CONF_EXTERNAL "/tmp/resolv.conf.external"
#define EXTRA_NAMESERVER_FILE "/tmp/dnsck_servers"
#define OPTION_NAMESERVER "nameserver"

#define MAX_SERVERS 128
#define MAX_LINE 256

struct ns_result {
  const char *server;
  int ares_result;
  float msec;
};

void ares_error_die(const char *msg)
{
  fprintf(stderr, "libcares error: %s\n", msg);
  exit(1);
}

struct timespec timespec_diff(struct timespec *start, struct timespec *end)
{
  struct timespec diff;
  if (end->tv_nsec < start->tv_nsec) {
    diff.tv_sec = end->tv_sec - start->tv_sec - 1;
    diff.tv_nsec = 1000000000 + end->tv_nsec - start->tv_nsec;
  } else {
    diff.tv_sec = end->tv_sec - start->tv_sec;
    diff.tv_nsec = end->tv_nsec - start->tv_nsec;
  }
  return diff;
}

void ares_wait(ares_channel channel)
{
  int nfds, rc;
  fd_set readers, writers;
  struct timeval tv;

  while (1) {
    FD_ZERO(&readers);
    FD_ZERO(&writers);
    nfds = ares_fds(channel, &readers, &writers);
    if (nfds == 0)
      break;
    ares_timeout(channel, NULL, &tv);
    rc = select(nfds, &readers, &writers, NULL, &tv);
    if (rc < 0)
      perror("select");
    ares_process(channel, &readers, &writers);
  }
}

void result_callback(void *arg, int _result, int timeouts, unsigned char *abuf,
    int alen)
{
  int *result = arg;
  *result = _result;
}

int resolve(ares_channel channel, const char *server,
    struct ns_result *ns_result)
{
  int rc;
  int result;
  struct timespec start, end;
  struct timespec diff;

  clock_gettime(CLOCK_MONOTONIC, &start);

  rc = ares_set_servers_csv(channel, server);
  if (rc)
    ares_error_die(ares_strerror(rc));
  ares_query(channel, "gstatic.com", ns_c_in, ns_t_a, result_callback, &result);
  ares_wait(channel);

  clock_gettime(CLOCK_MONOTONIC, &end);
  diff = timespec_diff(&start, &end);

  ns_result->server = server;
  ns_result->ares_result = result;
  ns_result->msec = (diff.tv_sec * 1000) + (diff.tv_nsec / 1000000.0f);

  return result;
}

const char *ares_strresult(int result)
{
  switch (result) {
  case ARES_SUCCESS:
    return "OK";
  case ARES_ETIMEOUT:
    return "TIMEOUT";
  case ARES_ENOTFOUND:
    return "DNSERR";
  default:
    return "ERROR";
  }
}

/* from try_config() in ares_init.c */
const char *parse_nameserver_line(char *line)
{
  char *p;
  char *q;

  /* trim line comment */
  p = line;
  while (*p && (*p != '#'))
    p++;
  *p = '\0';

  /* trim trailing whitespace */
  q = p - 1;
  while ((q >= line) && isspace(*q))
    q--;
  *++q = '\0';

  /* skip leading whitespace */
  p = line;
  while (*p && isspace(*p))
    p++;

  if (!*p)
    /* empty line */
    return NULL;

  if (strncmp(p, OPTION_NAMESERVER, strlen(OPTION_NAMESERVER)) != 0)
    /* line is not nameserver */
    return NULL;

  /* skip over option name */
  p += strlen(OPTION_NAMESERVER);

  if (!*p)
    /* no nameserver value */
    return NULL;

  if (!isspace(*p))
    /* whitespace between option name and value is mandatory */
    return NULL;

  /* skip over whitespace */
  while (*p && isspace(*p))
    p++;

  if (!*p)
    /* no nameserver value */
    return NULL;

  /* return pointer to nameserver value */
  return p;
}

size_t read_resolv_conf(const char *servers[], size_t nservers)
{
  char line[MAX_LINE];
  const char *resolv_conf;
  const char *ip;
  struct stat st;
  size_t count;
  FILE *fp;

  count = 0;

  resolv_conf = RESOLV_CONF;
  if (stat(RESOLV_CONF_EXTERNAL, &st) == 0)
    resolv_conf = RESOLV_CONF_EXTERNAL;

  fp = fopen(resolv_conf, "r");
  if (!fp)
    return 0;

  while (count < nservers) {
    if (fgets(line, sizeof(line), fp) == NULL)
      break;

    if ((ip = parse_nameserver_line(line)) != NULL) {
      servers[count] = strdup(ip);
      count++;
    }
  }

  fclose(fp);
  return count;
}

size_t read_extra_nameservers(const char *servers[], size_t nservers)
{
  char line[MAX_LINE];
  char *tokptr;
  const char *delim = " \n\t,;";
  const char *ip;
  size_t count;
  FILE *fp;

  count = 0;

  fp = fopen(EXTRA_NAMESERVER_FILE, "r");
  if (!fp)
    return 0;

  while (count < nservers) {
    if (fgets(line, sizeof(line), fp) == NULL)
      break;

    tokptr = line;
    while (count < nservers) {
      ip = strtok(tokptr, delim);
      if (!ip)
        break;
      tokptr = NULL;

      servers[count] = strdup(ip);
      count++;
    }
  }

  fclose(fp);
  return count;
}

void resolve_array(ares_channel channel, const char *servers[], size_t nservers)
{
  size_t i;
  struct ns_result ns_result;

  for (i = 0; i < nservers; i++) {
    resolve(channel, servers[i], &ns_result);
    printf("%s(%s),%.1fms ", servers[i], ares_strresult(ns_result.ares_result),
        ns_result.msec);
  }
}

void free_servers(const char *servers[], size_t nservers)
{
  size_t i;

  for (i = 0; i < nservers; i++)
    free((char *)servers[i]);
}

void usage(const char *progname)
{
  fprintf(stderr, "usage: %s [-i interface]\nwhere:\n", progname);
  fprintf(stderr, "\t-i : name of interface to SO_BINDTODEVICE, like br0\n");
  exit(1);
}

int main(int argc, char * const argv[])
{
  ares_channel channel;
  const char *servers[MAX_SERVERS];
  size_t count;
  const char *interface = NULL;
  int rc, c;

  struct ares_options options = {
    .flags = ARES_FLAG_NOCHECKRESP,
    .timeout = 3000, /* milliseconds */
    .tries = 1,
  };

  while ((c = getopt(argc, argv, "i:")) != -1) {
    switch (c) {
    case 'i':
      interface = optarg;
      break;
    default:
      usage(argv[0]);
      break;
    }
  }

  rc = ares_library_init(ARES_LIB_INIT_NONE);
  if (rc)
    ares_error_die(ares_strerror(rc));

  rc = ares_init_options(&channel, &options,
      ARES_OPT_FLAGS | ARES_OPT_TIMEOUTMS | ARES_OPT_TRIES);
  if (rc)
    ares_error_die(ares_strerror(rc));

  if (interface) {
    ares_set_local_dev(channel, interface);
  }

  count = read_resolv_conf(servers, MAX_SERVERS);
  resolve_array(channel, servers, count);
  free_servers(servers, count);

  count = read_extra_nameservers(servers, MAX_SERVERS);
  resolve_array(channel, servers, count);
  free_servers(servers, count);

  if (optind > 0)
    resolve_array(channel, (const char **)&argv[optind], argc - optind);

  printf("\n");
  ares_destroy(channel);
  return 0;
}
