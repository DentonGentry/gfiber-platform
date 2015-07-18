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

/*
 * A tool to advertise an Eddystone-UID beacon.
 * https://github.com/google/eddystone
 */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

uint8_t nibble(char c)
{
  switch(c) {
    case '0' ... '9':
      return c - '0';
      break;
    case 'a' ... 'f':
      return c - 'a' + 10;
      break;
    case 'A' ... 'F':
      return c - 'A' + 10;
      break;
  }

  return 0;
}

void hex_to_uint8(const char *str, uint8_t *bin, int len)
{
  int i;
  uint8_t n;

  for (i = 0; i < len; ++i) {
    n = nibble(str[(2 * i)]) << 4;
    n |= nibble(str[(2 * i) + 1]);
    bin[i] = n;
  }
}

void populate_beacon(const uint8_t *nid, const uint8_t *instance,
    int8_t txpower, le_set_advertising_data_cp *advertising_data)
{
  uint8_t *adata = advertising_data->data;

  adata[0] = 0x02;  // length of flags
  adata[1] = 0x01;  // flags type
  adata[2] = 0x06;  // Flags

  adata[3] = 0x03;  // length
  adata[4] = 0x03;  // data type (list of UUIDs)
  adata[5] = 0xaa;  // Eddystone 16 bit UUID
  adata[6] = 0xfe;  // Eddystone 16 bit UUID
  adata[7] = 0x17;  // Service Data length
  adata[8] = 0x16;  // Service Data type
  adata[9] = 0xaa;  // Eddystone 16 bit UUID
  adata[10] = 0xfe;  // Eddystone 16 bit UUID
  adata[11] = 0x00;  // Eddystone-UID type
  adata[12] = txpower;

  memcpy(adata + 13, nid, 10);  // 10 byte namespace id
  memcpy(adata + 23, instance, 6);  // 6 byte instance
  adata[29] = 0;
  adata[30] = 0;

  advertising_data->length = 31;
}

void set_adv_data(int s, const uint8_t *nid, const uint8_t *instance,
    uint8_t txpower)
{
  le_set_advertising_data_cp adata;
  struct hci_request hcirq;
  uint8_t status;

  memset(&adata, 0, sizeof(adata));
  populate_beacon(nid, instance, txpower, &adata);

  memset(&hcirq, 0, sizeof(hcirq));
  hcirq.ogf = OGF_LE_CTL;
  hcirq.ocf = OCF_LE_SET_ADVERTISING_DATA;
  hcirq.cparam = &adata;
  hcirq.clen = LE_SET_ADVERTISING_DATA_CP_SIZE;
  hcirq.rparam = &status;
  hcirq.rlen = sizeof(status);

  if (hci_send_req(s, &hcirq, 1000)) {
    perror("hci_send_req OCF_LE_SET_ADVERTISING_DATA");
    hci_close_dev(s);
    exit(1);
  }

  if (status) {
    fprintf(stderr, "OCF_LE_SET_ADVERTISING_DATA status %d\n", status);
    hci_close_dev(s);
    exit(1);
  }
}

void set_adv_params(int s, int interval_ms)
{
  le_set_advertising_parameters_cp aparams;
  struct hci_request hcirq;
  uint8_t status;

  memset(&aparams, 0, sizeof(aparams));
  aparams.min_interval = htobs(interval_ms);
  aparams.max_interval = htobs(interval_ms);
  aparams.advtype = 3;  // advertising non-connectable
  aparams.chan_map = 7;  // all three advertising channels

  memset(&hcirq, 0, sizeof(hcirq));
  hcirq.ogf = OGF_LE_CTL;
  hcirq.ocf = OCF_LE_SET_ADVERTISING_PARAMETERS;
  hcirq.cparam = &aparams;
  hcirq.clen = LE_SET_ADVERTISING_PARAMETERS_CP_SIZE;
  hcirq.rparam = &status;
  hcirq.rlen = 1;

  if (hci_send_req(s, &hcirq, 1000)) {
    perror("hci_send_req OCF_LE_SET_ADVERTISING_PARAMETERS");
    hci_close_dev(s);
    exit(1);
  }

  if (status) {
    fprintf(stderr, "OCF_LE_SET_ADVERTISING_PARAMETERS status %d\n", status);
    hci_close_dev(s);
    exit(1);
  }
}

void set_adv_enable(int s, uint8_t enable)
{
  le_set_advertise_enable_cp aenable;
  struct hci_request hcirq;
  uint8_t status;

  memset(&aenable, 0, sizeof(aenable));
  aenable.enable = enable;

  memset(&hcirq, 0, sizeof(hcirq));
  hcirq.ogf = OGF_LE_CTL;
  hcirq.ocf = OCF_LE_SET_ADVERTISE_ENABLE;
  hcirq.cparam = &aenable;
  hcirq.clen = LE_SET_ADVERTISE_ENABLE_CP_SIZE;
  hcirq.rparam = &status;
  hcirq.rlen = 1;

  if (hci_send_req(s, &hcirq, 1000)) {
    perror("hci_send_req OCF_LE_SET_ADVERTISE_ENABLE");
    hci_close_dev(s);
    exit(1);
  }

  if (status) {
    fprintf(stderr, "OCF_LE_SET_ADVERTISE_ENABLE status %d\n", status);
    hci_close_dev(s);
    exit(1);
  }
}

int advertise_beacon(int enable, const uint8_t *nid, const uint8_t *instance,
    uint8_t txpower)
{
  int dev_id = hci_get_route(NULL);
  int s = -1;

  if ((s = hci_open_dev(dev_id)) < 0) {
    perror("hci_open_dev");
    return(1);
  }

  if (enable) {
    set_adv_data(s, nid, instance, txpower);
    set_adv_params(s, 200);
    set_adv_enable(s, 1);
  } else {
    set_adv_enable(s, 0);
  }

  hci_close_dev(s);
  return(0);
}

void usage(const char *progname)
{
  fprintf(stderr, "usage: %s [-d | -n nid -i instance -t txpower]\n",
      progname);
  fprintf(stderr, "\t-d: disable BTLE advertisement.\n");
  fprintf(stderr, "\t-n namespace: 10 byte hex like 00112233445566778899\n");
  fprintf(stderr, "\t-i instance: 6 byte hex like aabbccddeeff\n");
  fprintf(stderr, "\t-t txpower: Power level to expect at 0 meters\n");
  exit(1);
}

int main(int argc, char **argv)
{
  int c;
  const char *nidstr = NULL;
  const char *instancestr = NULL;
  int do_disable = 0;
  int txpower_set = 0;
  int8_t txpower = -1;
  uint8_t nid[10];
  uint8_t instance[6];

  while ((c = getopt(argc, argv, "di:n:t:")) != -1) {
    switch(c) {
      case 'd':
        do_disable = 1;
        break;
      case 'i':
        instancestr = optarg;
        break;
      case 'n':
        nidstr = optarg;
        break;
      case 't':
        txpower_set = 1;
        txpower = atoi(optarg);
        break;
      default:
        usage(argv[0]);
        break;
    }
  }

  if (do_disable) {
    advertise_beacon(0, NULL, NULL, 0);
    return 0;
  }

  if ((nidstr == NULL) || (instancestr == NULL) || !txpower_set ||
      (strlen(nidstr) != 20) || (strlen(instancestr) != 12)) {
    usage(argv[0]);
  }

  hex_to_uint8(nidstr, nid, sizeof(nid));
  hex_to_uint8(instancestr, instance, sizeof(instance));
  advertise_beacon(1, nid, instance, txpower);

  return 0;
}
