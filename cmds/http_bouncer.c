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

#include <curl/curl.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

const char *pki_hosts[] = {"pki.google.com", "clients1.google.com"};

#define HTTP_REDIRECT \
    "HTTP/1.0 302 Found\r\n" \
    "Location: %s\r\n\r\n"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(*arr))
#endif
#define BUF_SIZE 4096

struct http_request {
  char host[BUF_SIZE];
  char path[BUF_SIZE];
};

void perror_die(const char *msg)
{
  perror(msg);
  exit(1);
}

void usage_and_die(const char *argv0)
{
  fprintf(stderr, "Usage: %s -p port -u url\n", argv0);
  exit(1);
}

bool is_strprefix(const char *str, const char *prefix)
{
  return (strncmp(str, prefix, strlen(prefix)) == 0);
}

bool is_strsuffix(const char *str, const char *suffix)
{
  const char *pos;
  size_t str_len;
  size_t suffix_len;

  str_len = strlen(str);
  suffix_len = strlen(suffix);

  if (suffix_len > str_len)
    return false;

  pos = str + str_len - suffix_len;
  return (strcmp(pos, suffix) == 0);
}

bool in_array(const char **array, size_t count, const char *key)
{
  size_t i;
  for (i = 0; i < count; i++) {
    if (strcmp(array[i], key) == 0)
      return true;
  }
  return false;
}

bool is_crl_request(struct http_request *req)
{
  return (in_array(pki_hosts, ARRAY_SIZE(pki_hosts), req->host) &&
      (is_strprefix(req->path, "/ocsp/") || is_strsuffix(req->path, ".crl")));
}

int stream_get_header(char *buf, size_t bufsz, FILE *fp)
{
  char *line;
  size_t end;

  line = fgets(buf, bufsz, fp);
  if (line == NULL)
    return -1;

  end = strcspn(buf, "\r\n");
  buf[end] = '\0';

  if (buf[0] == '\0')
    return -1;

  return 0;
}

int extract_request_path(char *path, size_t pathsz, char *header)
{
  char *token;
  char *delim = " \t";

  token = strtok(header, delim);
  if (token == NULL)
    return -1;

  token = strtok(NULL, delim);
  if (token == NULL)
    return -1;

  snprintf(path, pathsz, "%s", token);
  return 0;
}

int extract_header_val(char *val, size_t valsz, const char *header)
{
  char *space = strchr(header, ' ');
  if (space == NULL)
    return -1;

  space++;
  snprintf(val, valsz, "%s", space);
  return 0;
}

int stream_get_header_val(char *val, size_t valsz, const char *headername,
    FILE *fp)
{
  char buf[BUF_SIZE];

  while (stream_get_header(buf, sizeof(buf), fp) == 0) {
    if (strncasecmp(buf, headername, strlen(headername)) == 0)
      return extract_header_val(val, valsz, buf);
  }

  return -1;
}

int stream_parse_request(struct http_request *req, FILE *fp)
{
  char buf[BUF_SIZE];
  int rc;

  rc = stream_get_header(buf, sizeof(buf), fp);
  if (rc < 0)
    return -1;

  rc = extract_request_path(req->path, sizeof(req->path), buf);
  if (rc < 0)
    return -1;

  rc = stream_get_header_val(req->host, sizeof(req->host), "Host", fp);
  if (rc < 0)
    return -1;

  return 0;
}

int stream_send_proxy_request(struct http_request *req, FILE *fp)
{
  char url[BUF_SIZE];
  CURL *curl;
  CURLcode res;

  curl = curl_easy_init();
  if (curl == NULL)
    return -1;

  snprintf(url, sizeof(url), "http://%s%s", req->host, req->path);

  res = CURLE_OK;
  res |= curl_easy_setopt(curl, CURLOPT_URL, url);
  res |= curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
  res |= curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
  res |= curl_easy_setopt(curl, CURLOPT_HEADERDATA, fp);
  res |= curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  if (res != CURLE_OK) {
    fprintf(stderr, "curl_easy_setopt() failed: %s\n",
        curl_easy_strerror(res));
    return -1;
  }

  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
        curl_easy_strerror(res));
    return -1;
  }

  return 0;
}

int handle_client(int fd, const char *redirect_url)
{
  struct http_request req;
  FILE *read_fp;
  FILE *write_fp;
  int rc;

  read_fp = NULL;
  write_fp = NULL;

  read_fp = fdopen(fd, "r");
  if (read_fp == NULL)
    goto err;

  write_fp = fdopen(fd, "w");
  if (write_fp == NULL)
    goto err;

  /* if the request is invalid we will send them the redirect response anyway */
  if (!stream_parse_request(&req, read_fp) && is_crl_request(&req)) {
    rc = stream_send_proxy_request(&req, write_fp);
    if (rc)
      goto err;
  } else {
    fprintf(write_fp, HTTP_REDIRECT, redirect_url);
  }
  fflush(write_fp);

  fclose(read_fp);
  fclose(write_fp);
  return 0;

err:
  if (read_fp) fclose(read_fp);
  if (write_fp) fclose(write_fp);
  return -1;
}

int init_socket(int port)
{
  struct sockaddr_in6 addr;
  int rc;
  int fd;
  int opt;

  fd = socket(AF_INET6, SOCK_STREAM, 0);
  if (fd < 0)
    perror_die("socket");

  opt = false;
  rc = setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
  if (rc < 0)
    perror_die("setsockopt");

  opt = true;
  rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  if (rc < 0)
    perror_die("setsockopt");

  memset(&addr, '\0', sizeof(addr));
  addr.sin6_family = AF_INET6;
  addr.sin6_addr = in6addr_any;
  addr.sin6_port = htons(port);

  rc = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
  if (rc < 0)
    perror_die("bind");

  rc = listen(fd, 1);
  if (rc < 0)
    perror_die("listen");

  return fd;
}

int main(int argc, char **argv)
{
  int serverfd;
  int clientfd;
  int rc;
  int c;

  int port;
  const char *redirect_url;

  struct option long_options[] = {
    {"port", required_argument, 0, 'p'},
    {"url",  required_argument, 0, 'u'},
    {0,      0,                 0, 0},
  };

  port = -1;
  redirect_url = NULL;

  while ((c = getopt_long(argc, argv, "p:u:", long_options, NULL)) != -1) {
    switch (c) {
    case 'p':
      port = atoi(optarg);
      if (port < 1) {
        errno = EINVAL;
        perror_die("port");
      }
      break;
    case 'u':
      redirect_url = optarg;
      break;
    default:
      usage_and_die(argv[0]);
    }
  }

  if (optind < argc || port < 0 || redirect_url == NULL)
    usage_and_die(argv[0]);

  signal(SIGCHLD, SIG_IGN);
  serverfd = init_socket(port);

  while (1) {
    clientfd = accept(serverfd, NULL, NULL);
    if (clientfd < 0)
      perror_die("accept");

    rc = fork();
    if (rc < 0)
      perror_die("fork");
    else if (rc == 0)
      return handle_client(clientfd, redirect_url);

    close(clientfd);
  }

  return 0;
}
