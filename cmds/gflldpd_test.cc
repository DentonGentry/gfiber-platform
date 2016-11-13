/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2013 Keichi Takahashi keichi.t@me.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <netinet/if_ether.h>
#include <stdio.h>
#include <string.h>

#include <wvtest.h>


#define UNIT_TESTS
#include "gflldpd.c"


WVTEST_MAIN("mac_str_to_bytes") {
  uint8_t expected_mac[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 };
  uint8_t mac[ETH_ALEN];

  mac_str_to_bytes("00:11:22:33:44:55", mac);
  WVPASSEQ(memcmp(mac, expected_mac, ETH_ALEN), 0);
}


WVTEST_MAIN("format_lldp_packet") {
  size_t siz;
  uint8_t expected[] = {
    0x01, 0x80, 0xc2, 0x00, 0x00, 0x0e, 0x00, 0x11,
    0x22, 0x33, 0x44, 0x55, 0x88, 0xcc, 0x02, 0x07,
    0x04, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x04,
    0x07, 0x03, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
    0x06, 0x02, 0x00, 0x78, 0x08, 0x04, 0x65, 0x74,
    0x68, 0x30, 0x0a, 0x0b, 0x47, 0x30, 0x31, 0x32,
    0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x00,
    0x00
  };

  siz = format_lldp_packet("00:11:22:33:44:55", "eth0", "G0123456789");
  WVPASSEQ(siz, sizeof(expected));
  WVPASSEQ(memcmp(sendbuf, expected, siz), 0);
}
