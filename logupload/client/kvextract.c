#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <netdb.h>

#include "kvextract.h"
#include "utils.h"

void free_kv_pairs(struct kvpair* pairs) {
  while (pairs) {
    struct kvpair* next_pair = pairs->next_pair;
    free(pairs);
    pairs = next_pair;
  }
}

int get_pair_from_file(const char* filepath, const char* key,
    struct kvpair** pair) {
  if (!pair)
    return KV_FAIL;
  *pair = (struct kvpair*) malloc(sizeof(struct kvpair));
  if (!*pair) {
    fprintf(stderr, "failed allocating kvpair buffer\n");
    return KV_FAIL;
  }
  int len;
  memset(*pair, 0, sizeof(struct kvpair));
  len = read_file_as_string(filepath, (*pair)->value, sizeof((*pair)->value));
  if (len < 0) {
    free(*pair);
    return KV_NOTHING;
  }
  rstrip((*pair)->value);
  assert(strlen(key) < MAX_KV_LENGTH);
  strcpy((*pair)->key, key);
  return KV_OK;
}

struct kvpair* extract_kv_pairs(struct kvextractparams* params) {
  int family, i, rv;
  struct ifaddrs* ifa;
  char mac_buf[32];
  struct kvpair* pair_head;
  struct kvpair* pair_tail;
  pair_head = pair_tail = NULL;

  // Get the data for the model of the device
  if (get_pair_from_file(params->platform_path, "model", &pair_tail) != KV_OK) {
    fprintf(stderr, "failed getting kv for %s\n", params->platform_path);
    return NULL;
  }
  pair_head = pair_tail;

  // Now get the serial # of the device
  if (get_pair_from_file(params->serial_path, "serial",
        &(pair_tail->next_pair)) != KV_OK) {
    fprintf(stderr, "failed getting kv for %s\n", params->serial_path);
    free_kv_pairs(pair_head);
    return NULL;
  }
  pair_tail = pair_tail->next_pair;

  // Now get the IP address information
  for (ifa = params->ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL)
      continue;
    family = ifa->ifa_addr->sa_family;
    if (family == AF_INET || family == AF_INET6) {
      // IPV4 or IPV6 address, check if it's for one of the interfaces
      // we care about.
      for (i = 0; i < params->num_interfaces; i++) {
        if (!strcmp(ifa->ifa_name, params->interfaces_to_check[i])) {
          // Matches our interface, add a new kvpair
          pair_tail->next_pair = (struct kvpair*) malloc(sizeof(struct kvpair));
          if (!pair_tail->next_pair) {
            fprintf(stderr, "failed allocating kvpair buffer\n");
            free_kv_pairs(pair_head);
            return NULL;
          }
          pair_tail = pair_tail->next_pair;
          memset(pair_tail, 0, sizeof(struct kvpair));
          rv = params->name_info_resolver(ifa->ifa_addr,
              (family == AF_INET) ? sizeof(struct sockaddr_in) :
              sizeof(struct sockaddr_in6), pair_tail->value,
              sizeof(pair_tail->value), NULL, 0, NI_NUMERICHOST);
          if (rv != 0) {
            fprintf(stderr, "getnameinfo() failed: %s\n", gai_strerror(rv));
            free_kv_pairs(pair_head);
            return NULL;
          }
          strcpy(pair_tail->key, (family == AF_INET) ? "ip" : "ip6");
          break;
        }
      }
    }
  }

  // Now get the MAC address information from the interfaces
  for (i = 0; i < params->num_interfaces; i++) {
    if (!params->interface_to_mac(params->interfaces_to_check[i], mac_buf,
        sizeof(mac_buf))) {
      pair_tail->next_pair = (struct kvpair*) malloc(sizeof(struct kvpair));
      if (!pair_tail->next_pair) {
        fprintf(stderr, "failed allocating memory for kvpair\n");
        free_kv_pairs(pair_head);
        return NULL;
      }
      pair_tail = pair_tail->next_pair;
      memset(pair_tail, 0, sizeof(struct kvpair));
      strcpy(pair_tail->key, "hw");
      strcpy(pair_tail->value, mac_buf);
    }
  }

  // Now add the logtype
  if (params->logtype && strlen(params->logtype) > 0) {
    pair_tail->next_pair = (struct kvpair*) malloc(sizeof(struct kvpair));
    if (!pair_tail->next_pair) {
      fprintf(stderr, "failed allocating memory for kvpair\n");
      free_kv_pairs(pair_head);
      return NULL;
    }
    pair_tail = pair_tail->next_pair;
    memset(pair_tail, 0, sizeof(struct kvpair));
    strcpy(pair_tail->key, "logtype");
    strcpy(pair_tail->value, params->logtype);
  }

  return pair_head;
}
