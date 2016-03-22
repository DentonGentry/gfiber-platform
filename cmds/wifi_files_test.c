#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>


time_t now = 1000;
static time_t monotime(void)
{
  return now;
}


#define UNIT_TESTS
const char *stations_dir;
#define STATIONS_DIR stations_dir
#define WIFIINFO_DIR STATIONS_DIR
#include "wifi_files.c"


int exit_code = 0;


#define TEST_ASSERT(x) \
  if (x) printf("! %s:%d \"x\"\tok\n", __FUNCTION__, __LINE__); \
  else { \
    printf("! %s:%d \"x\"\tFAILED\n", __FUNCTION__, __LINE__); \
    exit_code = 1; \
  }


void testPrintSsidEscaped()
{
  FILE *f = tmpfile();
  char buf[32];
  const uint8_t ssid[] = {'b', 0x86, ' ', 'c'};  /* not NUL terminated. */
  const uint8_t expected[] = {'b', '\\', 'u', '0', '0', '8', '6', ' ', 'c'};

  printf("Testing \"%s\" in %s:\n", __FUNCTION__, __FILE__);
  memset(buf, 0, sizeof(buf));
  TEST_ASSERT(f != NULL);
  print_ssid_escaped(f, sizeof(ssid), ssid);
  fflush(f);
  rewind(f);
  TEST_ASSERT(fread(buf, 1, sizeof(buf), f) > 0);
  TEST_ASSERT(memcmp(buf, expected, sizeof(expected)) == 0);
  fclose(f);
  printf("! %s:%d\t%s\tok\n", __FILE__, __LINE__, __FUNCTION__);
}


void testFrequencyToChannel()
{
  printf("Testing \"%s\" in %s:\n", __FUNCTION__, __FILE__);
  TEST_ASSERT(ieee80211_frequency_to_channel(2484) == 14);
  TEST_ASSERT(ieee80211_frequency_to_channel(5745) == 149);
  printf("! %s:%d\t%s\tok\n", __FILE__, __LINE__, __FUNCTION__);
}


static char *expected_json = "{\n"
"  \"addr\": \"00:11:22:33:44:55\",\n"
"  \"inactive since\": 0.000,\n"
"  \"inactive msec\": 0,\n"
"  \"active\": false,\n"
"  \"rx bitrate\": 4.7,\n"
"  \"rx bytes\": 0,\n"
"  \"rx packets\": 0,\n"
"  \"tx bitrate\": 0.0,\n"
"  \"tx bytes\": 0,\n"
"  \"tx packets\": 0,\n"
"  \"tx retries\": 0,\n"
"  \"tx failed\": 0,\n"
"  \"rx mcs\": 0,\n"
"  \"rx max mcs\": 0,\n"
"  \"rx vht mcs\": 0,\n"
"  \"rx max vht mcs\": 0,\n"
"  \"rx width\": 0,\n"
"  \"rx max width\": 0,\n"
"  \"rx ht_nss\": 0,\n"
"  \"rx max ht_nss\": 0,\n"
"  \"rx vht_nss\": 0,\n"
"  \"rx max vht_nss\": 0,\n"
"  \"rx SHORT_GI\": false,\n"
"  \"rx SHORT_GI seen\": false,\n"
"  \"signal\": 0,\n"
"  \"signal_avg\": 0,\n"
"  \"authorized\": \"no\",\n"
"  \"authenticated\": \"yes\",\n"
"  \"preamble\": \"no\",\n"
"  \"wmm_wme\": \"no\",\n"
"  \"mfp\": \"no\",\n"
"  \"tdls_peer\": \"no\",\n"
"  \"preamble length\": \"long\",\n"
"  \"rx bytes64\": 1,\n"
"  \"rx drop64\": 2,\n"
"  \"tx bytes64\": 0,\n"
"  \"tx retries64\": 0,\n"
"  \"expected Mbps\": 7.009,\n"
"  \"ifname\": \"\"\n"
"}";


void testClientStateToJson()
{
  client_state_t state;
  char *buf;
  char filename[PATH_MAX];
  int fd;

  printf("Testing \"%s\" in %s:\n", __FUNCTION__, __FILE__);
  TEST_ASSERT(ieee80211_frequency_to_channel(2484) == 14);
  memset(&state, 0, sizeof(client_state_t));
  snprintf(state.macstr, sizeof(state.macstr), "00:11:22:33:44:55");
  state.rx_bytes64 = 1ULL;
  state.rx_drop64 = 2ULL;
  state.rx_bitrate = 47;
  state.authorized = 0;
  state.authenticated = 1;
  state.preamble_length = 0;
  state.expected_mbps = 7009;
  ClientStateToJson((gpointer)(&state.macstr), (gpointer)&state, NULL);

  #define SIZ 65536
  TEST_ASSERT((buf = malloc(SIZ)) != NULL);
  memset(buf, 0, SIZ);
  snprintf(filename, sizeof(filename), "%s/%s", STATIONS_DIR, state.macstr);
  TEST_ASSERT((fd = open(filename, O_RDONLY)) >= 0);
  TEST_ASSERT(read(fd, buf, SIZ) > 0);
  close(fd);
  TEST_ASSERT(unlink(filename) == 0);
  TEST_ASSERT(strcmp(buf, expected_json));
  free(buf);
  printf("! %s:%d\t%s\tok\n", __FILE__, __LINE__, __FUNCTION__);
}


void testAgeOutClients()
{
  uint8_t mac[] = {0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
  client_state_t *state;

  printf("Testing \"%s\" in %s:\n", __FUNCTION__, __FILE__);
  mac[5] = 0x01;
  state = FindClientState(mac);
  state->last_seen = 1000;

  mac[5] = 0x02;
  state = FindClientState(mac);
  state->last_seen = 10000;
  TEST_ASSERT(g_hash_table_size(clients) == 2);

  now = 1000 + MAX_CLIENT_AGE_SECS + 1;
  ConsolidateAssociatedDevices();
  TEST_ASSERT(g_hash_table_size(clients) == 1);

  now = 10000 + MAX_CLIENT_AGE_SECS + 1;
  ConsolidateAssociatedDevices();
  TEST_ASSERT(g_hash_table_size(clients) == 0);
  printf("! %s:%d\t%s\tok\n", __FILE__, __LINE__, __FUNCTION__);
}


int main(int argc, char** argv)
{
  char filename[PATH_MAX];

  stations_dir = mkdtemp(strdup("/tmp/wifi_files_test_XXXXXX"));
  printf("stations_dir = %s\n", stations_dir);
  clients = g_hash_table_new(g_str_hash, g_str_equal);

  testPrintSsidEscaped();
  testFrequencyToChannel();
  testClientStateToJson();
  testAgeOutClients();

  snprintf(filename, sizeof(filename), "%s/updated.new", STATIONS_DIR);
  unlink(filename);
  TEST_ASSERT(rmdir(stations_dir) == 0);

  exit(exit_code);
}
