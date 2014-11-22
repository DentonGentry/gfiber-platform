#ifndef _H_LOGUPLOAD_CLIENT_KVEXTRACT_H_
#define _H_LOGUPLOAD_CLIENT_KVEXTRACT_H_

#include <sys/socket.h>
#include <ifaddrs.h>

#define MAX_KV_LENGTH 128

#define KV_OK 0
#define KV_FAIL -1
#define KV_NOTHING 1

struct kvpair {
  char key[MAX_KV_LENGTH];
  char value[MAX_KV_LENGTH];
  struct kvpair* next_pair;
};

struct kvextractparams {
  const char** interfaces_to_check;
  int num_interfaces;
  struct ifaddrs* ifaddr;
  const char* platform_path;
  const char* serial_path;
  int (*name_info_resolver)(const struct sockaddr* sa, socklen_t salen,
      char* host, size_t hostlen, char* serv, size_t servlen, int flags);
  int (*interface_to_mac)(const char* iface, char* buf, int len);
};

// Returns KV_OK on success, KV_FAIL on failure and KV_NOTHING if there was
// nothing in the file.
// pair will point to a malloc'd kvpair if the return value is KV_OK.
// The passed in key is set as the key and the data from the file is the
// value with trailing whitespace removed.
int get_pair_from_file(const char* filepath, const char* key,
    struct kvpair** pair);

// Returns a dynamically allocated kvpair which needs to be freed
// with free_kv_pairs after using it. Returns NULL if there's a problem
// with extracting the pair information.
// This will get pairs for the device model, serial and information
// about the network config such as MAC and IPV4/6 addresses.
struct kvpair* extract_kv_pairs(struct kvextractparams* params);
void free_kv_pairs(struct kvpair* pairs);

#endif  // _H_LOGUPLOAD_CLIENT_KVEXTRACT_H_
