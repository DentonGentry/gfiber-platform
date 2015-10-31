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

/* Unit tests for netusage.c */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>


struct timespec test_clock_gettime_value = {0, 0};
int test_clock_gettime(clockid_t clk_id, struct timespec *tp)
{
  *tp = test_clock_gettime_value;
  return 0;
}


void sendreq(int s, const char *ifname)
{
  return;
}


int recvresp_count = 0;
void recvresp(int s, uint32_t *tx_bytes, uint32_t *rx_bytes,
    uint32_t *tx_pkts, uint32_t *rx_pkts, uint32_t *rx_multipkts)
{
  /*
   * Set up the conditions for an overflow in rx_uni_pkts. On the first
   * read of the hardware there were 5 total packets, but multipackets
   * incremented to 6 immediately before we read it.
   *
   * On the second read, rx_packets will have also incremented to 6.
   */
  if (recvresp_count == 0) {
    recvresp_count = 1;
    *tx_bytes = 1000;
    *rx_bytes = 2000;
    *tx_pkts = 3000;
    *rx_pkts = 5000;
    *rx_multipkts = 6000;
  } else {
    recvresp_count = 0;
    *tx_bytes = 1000;
    *rx_bytes = 2000;
    *tx_pkts = 3000;
    *rx_pkts = 6000;
    *rx_multipkts = 6000;
  }
}


#define UNIT_TESTS
#define CLOCK_GETTIME test_clock_gettime
#include "netusage.c"


int almost_equal(double val, double expected)
{
  return fabs(val - expected) < 0.0001;
}


void test_counters()
{
  double tx_kbps, rx_kbps, tx_pps, rx_uni_pps, rx_multi_pps;
  struct saved_counters old;

  test_clock_gettime_value.tv_sec = 1;
  test_clock_gettime_value.tv_nsec = 0;
  memset(&old, 0, sizeof(old));

  accumulate_stats(0, 1.0, "foo0", &tx_kbps, &rx_kbps, &tx_pps,
      &rx_uni_pps, &rx_multi_pps, &old);

  assert(almost_equal(tx_kbps, 1.0 * 8));
  assert(almost_equal(rx_kbps, 2.0 * 8));
  assert(almost_equal(tx_pps, 3000.0));
  assert(almost_equal(rx_uni_pps, 0.0));
  assert(almost_equal(rx_multi_pps, 6000.0));
}


void test_mono_usecs()
{
  uint64_t usecs;

  test_clock_gettime_value.tv_sec = 1;
  test_clock_gettime_value.tv_nsec = 3000;
  usecs = mono_usecs();
  assert(usecs == 1000003);
}


int main(int argc, char** argv)
{
  test_mono_usecs();
  test_counters();
  exit(0);
}
