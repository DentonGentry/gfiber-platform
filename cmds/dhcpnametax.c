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

#include <getopt.h>
#include <inttypes.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hostnamelookup.h"

void usage(const char *progname)
{
  fprintf(stderr, "usage: %s -d dhcpsig -h hostname -l label\n", progname);
  fprintf(stderr, "\t-d: DHCP options signature\n");
  fprintf(stderr, "\t-h: hostname of the station\n");
  fprintf(stderr, "\t-l: label for this station (typically the MAC addr)\n");
  exit(1);
}


struct hostname_strings *check_directv(const char *hostname)
{
  regex_t r_directv;
  regmatch_t match[2];

  if (regcomp(&r_directv, "DIRECTV-([^-]+)-", REG_EXTENDED | REG_ICASE)) {
    fprintf(stderr, "%s: regcomp failed!\n", __FUNCTION__);
    exit(1);
  }

  if (regexec(&r_directv, hostname, 2, match, 0) == 0) {
    struct hostname_strings *h = (struct hostname_strings *)malloc(sizeof *h);
    int len = match[1].rm_eo - match[1].rm_so;

    h->genus = "DirecTV";
    h->species = strndup(hostname + match[1].rm_so, len);
    return h;
  }

  return NULL;
}


int main(int argc, char **argv)
{
  struct option long_options[] = {
    {"dhcpsig",  required_argument, 0, 'd'},
    {"hostname", required_argument, 0, 'h'},
    {"label",    required_argument, 0, 'l'},
    {0,          0,                 0, 0},
  };
  int c;
  const char *dhcpsig = NULL;
  const char *hostname = NULL;
  const char *label = NULL;
  char concatenated[256];
  const struct hostname_strings *sn = NULL;

  setlinebuf(stdout);
  alarm(30);
  while ((c = getopt_long(argc, argv, "d:h:l:", long_options, NULL)) != -1) {
    switch (c) {
    case 'd':
      dhcpsig = optarg;
      break;
    case 'l':
      label = optarg;
      break;
    case 'h':
      hostname = optarg;
      break;
    default:
      usage(argv[0]);
      break;
    }
  }

  if (optind < argc ||
      dhcpsig == NULL || hostname == NULL || label == NULL) {
    usage(argv[0]);
  }

  snprintf(concatenated, sizeof(concatenated), "%s%%%s", hostname, dhcpsig);
  if ((sn = hostname_lookup(concatenated, strlen(concatenated))) == NULL) {
    if (strcmp(dhcpsig, "1,3,6,12,15,28,40,41,42") == 0) {
      // DIRECTV-HR24-XXXXXXXX
      sn = check_directv(hostname);
    } else if (strcmp(dhcpsig, "1,3,6,12,15,28,42") == 0) {
      // DIRECTV-HR24-XXXXXXXX
      if ((sn = check_directv(hostname)) == NULL) {
        // Trane thermostat XL824-XXXXXXXX
        sn = hostname_lookup(hostname, 6);
      }
    } else if (strcmp(dhcpsig, "1,28,2,3,15,6,12") == 0) {
      // TIVO-###
      sn = hostname_lookup(hostname, 8);
    } else if (strcmp(dhcpsig, "1,3,6,15,12") == 0) {
      // Roku NP-##
      sn = hostname_lookup(hostname, 5);
    } else if (strcmp(dhcpsig, "3,1,252,42,15,6,12") == 0) {
      // Nest 0#A
      sn = hostname_lookup(hostname, 3);
    } else if (strcmp(dhcpsig, "1,28,2,3,15,6,119,12,44,47,26,121,42") == 0) {
      // SleepIQ
      sn = hostname_lookup(hostname, 11);
    }
  }

  if (sn != NULL) {
    printf("name %s %s;%s\n", label, sn->genus, sn->species);
  }
  exit(0);
}
