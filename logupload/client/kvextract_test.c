#include <stdio.h>
#include <fcntl.h>
#include "kvextract.h"
#include "gtest/gtest.h"
#include "utils.h"

TEST(KVExtract, get_pair_from_file_success) {
  struct kvpair* mypair;
  char tdir[32] = "kvtestXXXXXX";
  EXPECT_TRUE(mkdtemp(tdir) != NULL);
  char tfile[64];
  snprintf(tfile, sizeof(tfile), "%s/%s", tdir, "getfilepairtest");
  write_to_file(tfile, "specialvalue\n");
  EXPECT_EQ(KV_OK, get_pair_from_file(tfile, "specialkey", &mypair));
  remove(tfile);
  rmdir(tdir);
  EXPECT_STREQ("specialkey", mypair->key);
  EXPECT_STREQ("specialvalue", mypair->value);
  free_kv_pairs(mypair);
}

TEST(KVExtract, get_pair_from_file_nothing) {
  struct kvpair* mypair;
  EXPECT_EQ(KV_NOTHING, get_pair_from_file("filedoesnotexist", "foo", &mypair));
}

TEST(KVExtract, get_pair_from_file_fail) {
  EXPECT_EQ(KV_FAIL, get_pair_from_file("foo", "foo", NULL));
}

static const char* ifaces_to_check[2] = { "fake0", "fake1" };
static int num_interfaces = sizeof(ifaces_to_check) /
  sizeof(ifaces_to_check[0]);

static int mynameresolver(const struct sockaddr* sa, socklen_t salen,
    char* host, size_t hostlen, char* serv, size_t servlen, int flags) {
  if (host) {
    strncpy(host, sa->sa_data, hostlen);
  }
  return 0;
}

static int myifaceresolver(const char* iface, char* buf, int len) {
  if (!strcmp(iface, ifaces_to_check[0])) {
    snprintf(buf, len, "11:22:33:44:55");
    return 0;
  } else if (!strcmp(iface, ifaces_to_check[1])) {
    snprintf(buf, len, "AA:BB:CC:DD:EE");
    return 0;
  } else
    return -1;
}

TEST(KVExtract, extract_kv_pairs_success) {
  struct kvpair* result;
  char ignore_iface[32] = "ignore";
  // The data array in sockaddr is 14 bytes long so we need to fit our
  // test addresses in there.
  char invalid_addr[14] = "invalid";
  char valid_ipv4[14] = "192.168.1.4";
  char valid_ipv6[14] = "2620:0:102f";

  struct ifaddrs ifaddr_ignore;
  struct ifaddrs ifaddr_v4;
  struct ifaddrs ifaddr_v6;
  struct ifaddrs ifaddr_v6_ignore;
  memset(&ifaddr_ignore, 0, sizeof(ifaddr_ignore));
  memset(&ifaddr_v4, 0, sizeof(ifaddr_v4));
  memset(&ifaddr_v6, 0, sizeof(ifaddr_v6));
  memset(&ifaddr_v6_ignore, 0, sizeof(ifaddr_v6_ignore));

  struct sockaddr sockaddr_ignore;
  struct sockaddr sockaddr_v4;
  struct sockaddr sockaddr_v6;
  struct sockaddr sockaddr_v6_ignore;
  memset(&sockaddr_ignore, 0, sizeof(sockaddr_ignore));
  memset(&sockaddr_v4, 0, sizeof(sockaddr_v4));
  memset(&sockaddr_v6, 0, sizeof(sockaddr_v6));
  memset(&sockaddr_v6_ignore, 0, sizeof(sockaddr_v6_ignore));

  ifaddr_ignore.ifa_next = &ifaddr_v4;
  ifaddr_ignore.ifa_name = ignore_iface;
  ifaddr_ignore.ifa_addr = &sockaddr_ignore;
  sockaddr_ignore.sa_family = AF_APPLETALK;
  strncpy(sockaddr_ignore.sa_data, invalid_addr,
      sizeof(sockaddr_ignore.sa_data));

  ifaddr_v4.ifa_next = &ifaddr_v6;
  ifaddr_v4.ifa_name = (char*)ifaces_to_check[0];
  ifaddr_v4.ifa_addr = &sockaddr_v4;
  sockaddr_v4.sa_family = AF_INET;
  strncpy(sockaddr_v4.sa_data, valid_ipv4, sizeof(sockaddr_v4.sa_data));

  ifaddr_v6.ifa_next = &ifaddr_v6_ignore;
  ifaddr_v6.ifa_name = (char*)ifaces_to_check[1];
  ifaddr_v6.ifa_addr = &sockaddr_v6;
  sockaddr_v6.sa_family = AF_INET6;
  strncpy(sockaddr_v6.sa_data, valid_ipv6, sizeof(sockaddr_v6.sa_data));

  ifaddr_v6_ignore.ifa_name = ignore_iface;
  ifaddr_v6_ignore.ifa_addr = &sockaddr_v6_ignore;
  sockaddr_v6_ignore.sa_family = AF_INET6;
  strncpy(sockaddr_v6_ignore.sa_data, invalid_addr,sizeof(sockaddr_v6.sa_data));

  char tdir[32] = "kvtestXXXXXX";
  EXPECT_TRUE(mkdtemp(tdir) != NULL);
  char tfileplatform[64];
  snprintf(tfileplatform, sizeof(tfileplatform), "%s/%s", tdir, "platform");
  char tfileserial[64];
  snprintf(tfileserial, sizeof(tfileserial), "%s/%s", tdir, "serial");
  write_to_file(tfileplatform, "fakeplatform");
  write_to_file(tfileserial, "fakeserial");

  struct kvextractparams kvparams;
  memset(&kvparams, 0, sizeof(kvparams));
  kvparams.interfaces_to_check = ifaces_to_check;
  kvparams.num_interfaces = num_interfaces;
  kvparams.ifaddr = &ifaddr_ignore;
  kvparams.platform_path = tfileplatform;
  kvparams.serial_path = tfileserial;
  kvparams.name_info_resolver = mynameresolver;
  kvparams.interface_to_mac = myifaceresolver;

  result = extract_kv_pairs(&kvparams);
  remove(tfileplatform);
  remove(tfileserial);
  rmdir(tdir);
  EXPECT_TRUE(result != NULL);
  EXPECT_STREQ("model", result->key);
  EXPECT_STREQ("fakeplatform", result->value);
  EXPECT_TRUE(result->next_pair != NULL);
  struct kvpair* currpair = result->next_pair;
  EXPECT_STREQ("serial", currpair->key);
  EXPECT_STREQ("fakeserial", currpair->value);
  EXPECT_TRUE(currpair->next_pair != NULL);
  currpair = currpair->next_pair;
  EXPECT_STREQ("ip", currpair->key);
  EXPECT_STREQ(valid_ipv4, currpair->value);
  EXPECT_TRUE(currpair->next_pair != NULL);
  currpair = currpair->next_pair;
  EXPECT_STREQ("ip6", currpair->key);
  EXPECT_STREQ(valid_ipv6, currpair->value);
  EXPECT_TRUE(currpair->next_pair != NULL);
  currpair = currpair->next_pair;
  EXPECT_STREQ("hw", currpair->key);
  EXPECT_STREQ("11:22:33:44:55", currpair->value);
  EXPECT_TRUE(currpair->next_pair != NULL);
  currpair = currpair->next_pair;
  EXPECT_STREQ("hw", currpair->key);
  EXPECT_STREQ("AA:BB:CC:DD:EE", currpair->value);
  EXPECT_TRUE(currpair->next_pair == NULL);
  free_kv_pairs(result);
}
