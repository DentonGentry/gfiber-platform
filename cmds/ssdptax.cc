/*
 * Copyright 2014 Google Inc. All rights reserved.
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

/*
 * ssdptax (SSDP Taxonomy)
 *
 * A client implementing the API described in
 * http://miniupnp.free.fr/minissdpd.html
 *
 * Requests the list of all known SSDP nodes, requests
 * device info from them, and tries to figure out what
 * they are.
 */

#include <arpa/inet.h>
#include <asm/types.h>
#include <ctype.h>
#include <curl/curl.h>
#include <getopt.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <iostream>
#include <set>

#include "l2utils.h"

/* Encode length by using 7bit per Byte :
 * Most significant bit of each byte specifies that the
 * following byte is part of the code */
#define DECODELENGTH(n, p) { \
  n = 0; \
  do { n = (n << 7) | (*p & 0x7f); } \
  while (*(p++)&0x80); \
}

#define CODELENGTH(n, p) { \
  if(n>=0x10000000) *(p++) = (n >> 28) | 0x80; \
  if(n>=0x200000) *(p++) = (n >> 21) | 0x80; \
  if(n>=0x4000) *(p++) = (n >> 14) | 0x80; \
  if(n>=0x80) *(p++) = (n >> 7) | 0x80; \
  *(p++) = n & 0x7f; \
}

#define SOCK_PATH "/var/run/minissdpd.sock"


typedef struct ssdp_info {
  ssdp_info(): srv_type(), url(), friendlyName(), ipaddr(),
    manufacturer(), model(), failed(0) {}
  ssdp_info(const ssdp_info& s): srv_type(s.srv_type), url(s.url),
    friendlyName(s.friendlyName), ipaddr(s.ipaddr),
    manufacturer(s.manufacturer), model(s.model), failed(s.failed) {}
  std::string srv_type;
  std::string url;
  std::string friendlyName;
  std::string ipaddr;
  std::string manufacturer;
  std::string model;

  std::string buffer;
  int failed;
} ssdp_info_t;


static void strncpy_limited(char *dst, size_t dstlen,
    const char *src, size_t srclen)
{
  size_t i;
  size_t lim = (srclen >= (dstlen - 1)) ? (dstlen - 2) : srclen;

  for (i = 0; i < lim; ++i) {
    unsigned char s = src[i];
    if (isspace(s) || s == ';') {
      dst[i] = ' ';  // deliberately convert newline to space
    } else if (isprint(s)) {
      dst[i] = s;
    } else {
      dst[i] = '_';
    }
  }
  dst[lim] = '\0';
}


/*
 * Send a request to minissdpd. Returns a std::string containing
 * minissdpd's response.
 */
std::string request_from_ssdpd(const char *sock_path,
    int reqtype, const char *device)
{
  int s = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un addr;
  char *buffer;
  ssize_t len;
  size_t siz = 256 * 1024;
  char *p;
  int device_len = (int)strlen(device);
  fd_set readfds;
  struct timeval tv;
  std::string rc;

  if (s < 0) {
    perror("socket AF_UNIX failed");
    exit(1);
  }
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path));
  if(connect(s, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) < 0) {
    perror("connect to minisspd failed");
    exit(1);
  }

  if ((buffer = (char *)malloc(siz)) == NULL) {
    fprintf(stderr, "malloc(%zu) failed\n", siz);
    exit(1);
  }
  memset(buffer, 0, siz);

  buffer[0] = reqtype;
  p = buffer + 1;
  CODELENGTH(device_len, p);
  memcpy(p, device, device_len);
  p += device_len;
  if (write(s, buffer, p - buffer) < 0) {
    perror("write to minissdpd failed");
    exit(1);
  }

  FD_ZERO(&readfds);
  FD_SET(s, &readfds);
  memset(&tv, 0, sizeof(tv));
  tv.tv_sec = 2;

  if (select(s + 1, &readfds, NULL, NULL, &tv) < 1) {
    fprintf(stderr, "select failed\n");
    exit(1);
  }

  if ((len = read(s, buffer, siz)) < 0) {
    perror("read from minissdpd failed");
    exit(1);
  }

  close(s);
  rc = std::string(buffer, len);
  free(buffer);
  return(rc);
}


/*
 * Combine the manufacturer and model. If the manufacturer name
 * is already present in the model string, don't duplicate it.
 */
const std::string unfriendly_name(const std::string &manufacturer,
    const std::string &model)
{
  if (strcasestr(model.c_str(), manufacturer.c_str()) != NULL) {
    return model;
  }

  return manufacturer + " " + model;
}


std::string format_response(const ssdp_info_t *info, L2Map *l2map)
{
  std::string mac;
  std::string ipaddr;
  std::string result;

  if (info->failed) {
    /*
     * We could not fetch information from this client. That often means that
     * the device was powered off recently. minissdpd still remembers that
     * it is there, but we cannot contact it.
     *
     * Don't print anything for these, as we'd end up calling them "Unknown"
     * and that is misleading. We only report information about devices which
     * are active right now.
     */
    return result;
  }

  mac = get_l2addr_for_ip(info->ipaddr);
  if (info->friendlyName.length() > 0) {
    result = "ssdp " + mac + " " + info->friendlyName + ";" +
      unfriendly_name(info->manufacturer, info->model);
  } else {
    result = "ssdp " + mac + " Unknown;" + info->srv_type;
  }

  return result;
}


void parse_minissdpd_response(std::string &response,
    std::string &url, std::string &srv_type)
{
  size_t slen;
  const char *p;
  const char *end = response.c_str() + response.length();
  char urlbuf[256];
  char srv_type_buf[256];

  memset(urlbuf, 0, sizeof(urlbuf));
  memset(srv_type_buf, 0, sizeof(srv_type_buf));

  p = response.c_str();
  DECODELENGTH(slen, p);
  if ((p + slen) > end) {
    fprintf(stderr, "Unable to parse SSDP response\n");
    return;
  }
  strncpy_limited(urlbuf, sizeof(urlbuf), p, slen);
  p += slen;

  DECODELENGTH(slen, p);
  if ((p + slen) > end) {
    fprintf(stderr, "Unable to parse SSDP response\n");
    return;
  }
  strncpy_limited(srv_type_buf, sizeof(srv_type_buf), p, slen);
  p += slen;

  DECODELENGTH(slen, p);
  if ((p + slen) > end) {
    fprintf(stderr, "Unable to parse SSDP response\n");
    return;
  }
  /* Skip over the UUID without processing it. */
  p += slen;

  url = urlbuf;
  srv_type = srv_type_buf;

  response.erase(0, (p - response.c_str()));
}


const char *findXmlField(const char *ptr, const char *label, ssize_t *len)
{
  char openlabel[64], closelabel[64];
  const char *start, *end;

  snprintf(openlabel, sizeof(openlabel), "<%s>", label);
  snprintf(closelabel, sizeof(closelabel), "</%s>", label);

  start = strcasestr(ptr, openlabel) + strlen(openlabel);
  end = strcasestr(ptr, closelabel);

  if (start < end) {
    *len = end - start;
    return start;
  }

  return NULL;
}


/*
 * Expected value in buffer is an XML blob of
 * http://upnp.org/specs/basic/UPnP-basic-Basic-v1-Device.pdf
 *
 * Like this (a Samsung TV):
 *  <?xml version="1.0"?>
 *  <root xmlns='urn:schemas-upnp-org:device-1-0' ...
 *   <device>
 *    <deviceType>urn:dial-multiscreen-org:device:dialreceiver:1</deviceType>
 *    <friendlyName>[TV]Samsung LED60</friendlyName>
 *    <manufacturer>Samsung Electronics</manufacturer>
 *    <manufacturerURL>http://www.samsung.com/sec</manufacturerURL>
 *    <modelDescription>Samsung TV NS</modelDescription>
 *    <modelName>UN60F6300</modelName>
 *    <modelNumber>1.0</modelNumber>
 *    <modelURL>http://www.samsung.com/sec</modelURL>
 * ... etc, etc ...
 */
void extract_fields_from_buffer(ssdp_info_t *info)
{
  const char *ptr = info->buffer.c_str();
  const char *p;
  ssize_t len;

  if ((p = findXmlField(ptr, "friendlyName", &len)) == NULL) {
    p = findXmlField(ptr, "modelDescription", &len);
  }
  if (p && len > 0) {
    info->friendlyName = std::string(p, len);
  }

  p = findXmlField(ptr, "manufacturer", &len);
  if (p && len > 0) {
    info->manufacturer = std::string(p, len);
  }

  p = findXmlField(ptr, "modelName", &len);
  if (p && len > 0) {
    info->model = std::string(p, len);
  }
}


size_t callback(const char *ptr, size_t size, size_t nmemb, void *userdata)
{
  ssdp_info_t *info = (ssdp_info_t *)userdata;
  info->buffer.append(ptr, size * nmemb);
  return size * nmemb;
}


/*
 * SSDP returned an endpoint URL, use curl to GET its contents.
 */
void fetch_device_info(const std::string &url, ssdp_info_t *info)
{
  CURL *curl = curl_easy_init();
  char *ip;

  if (!curl) {
    fprintf(stderr, "curl_easy_init failed\n");
    return;
  }
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_PATH_AS_IS, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, info);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "ssdptaxonomy/1.0");
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1L);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, true);
  if (curl_easy_perform(curl) == CURLE_OK) {
    extract_fields_from_buffer(info);
  } else {
    info->failed = 1;
  }
  if (curl_easy_getinfo(curl, CURLINFO_PRIMARY_IP, &ip) == CURLE_OK) {
    info->ipaddr = ip;
  }

  info->buffer.clear();
  curl_easy_cleanup(curl);
}


void usage(char *progname) {
  printf("usage: %s [-t /path/to/fifo]\n", progname);
  printf("\t-t\ttest mode, use a fake path instead of minissdpd.\n");
  exit(1);
}


int main(int argc, char **argv)
{
  std::string buffer;
  typedef std::tr1::unordered_map<std::string, ssdp_info_t*> ResponsesMap;
  ResponsesMap responses;
  L2Map l2map;
  int c, num;
  const char *sock_path = SOCK_PATH;

  setlinebuf(stdout);
  alarm(30);

  if (curl_global_init(CURL_GLOBAL_NOTHING)) {
    fprintf(stderr, "curl_global_init failed\n");
    exit(1);
  }

  while ((c = getopt(argc, argv, "t:")) != -1) {
    switch(c) {
      case 't': sock_path = optarg; break;
      default: usage(argv[0]); break;
    }
  }

  buffer = request_from_ssdpd(sock_path, 3, "ssdp:all");
  num = buffer.c_str()[0];
  buffer.erase(0, 1);
  while ((num-- > 0) && buffer.length() > 0) {
    ssdp_info_t *info = new ssdp_info_t;

    parse_minissdpd_response(buffer, info->url, info->srv_type);
    if (info->url.length() && responses.find(info->url) == responses.end()) {
      fetch_device_info(info->url, info);
      responses[info->url] = info;
    } else {
      delete info;
    }
  }

  get_l2_map(&l2map);

  typedef std::set<std::string> ResultsSet;
  ResultsSet results;
  for (ResponsesMap::const_iterator ii = responses.begin();
      ii != responses.end(); ++ii) {
    std::string r = format_response(ii->second, &l2map);
    if (r.length() > 0) {
      results.insert(r);
    }
  }

  /* Many devices advertise multiple URLs with the same
   * model information in all of them. Suppress duplicate
   * output using the set. */
  for (ResultsSet::const_iterator ii = results.begin();
      ii != results.end(); ++ii) {
    std::cout << *ii << std::endl;
  }

  curl_global_cleanup();
  exit(0);
}
