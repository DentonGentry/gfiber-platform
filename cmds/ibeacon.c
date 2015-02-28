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
 * A tool to advertise an iBeacon for a particular UUID. Information in
 * http://www.theregister.co.uk/2013/11/29/feature_diy_apple_ibeacons/
 * was very helpful in constructing this, as was
 * https://github.com/carsonmcdonald/bluez-ibeacon
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <uuid/uuid.h>

void populate_ibeacon(uuid_t uuid, int major, int minor,
    uint8_t tx_power, le_set_advertising_data_cp *advertising_data)
{
  uint16_t tmp;
  uint8_t *adata = advertising_data->data;
  const uint8_t *u = uuid;

  adata[0] = 0x02;  // length of flags
  adata[1] = 0x01;  // flags type
  adata[2] = 0x1a;  // Flags: 000011010

  adata[3] = 0x1a;  // length
  adata[4] = 0xff;  // vendor specific
  adata[5] = 0x4c;  // Apple, Inc
  adata[6] = 0x00;  // Apple, Inc
  adata[7] = 0x02;  // iBeacon
  adata[8] = 0x15;  // length: 16 byte UUID, 2 bytes major&minor, 1 byte RSSI

  memcpy(adata + 9, u, 16);  // UUID

  tmp = htobs(major);
  adata[25] = (tmp >> 8) & 0x00ff;
  adata[26] = tmp & 0x00ff;

  tmp = htobs(minor);
  adata[27] = (tmp >> 8) & 0x00ff;
  adata[28] = tmp & 0x00ff;

  adata[29] = tx_power;

  advertising_data->length = 30;
}

void set_adv_data(int s, const char *uuidstr, int major, int minor,
        uint8_t tx_power)
{
  uuid_t uuid;
  le_set_advertising_data_cp adata;
  struct hci_request hcirq;
  uint8_t status;

  if (uuid_parse(uuidstr, uuid)) {
    fprintf(stderr, "uuid_parse \"%s\" failed\n", uuidstr);
    exit(1);
  }

  memset(&adata, 0, sizeof(adata));
  populate_ibeacon(uuid, major, minor, tx_power, &adata);

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

int advertise_ibeacon(int enable, const char *uuidstr,
    int major, int minor, uint8_t tx_power)
{
  int dev_id = hci_get_route(NULL);
  int s = -1;

  if ((s = hci_open_dev(dev_id)) < 0) {
    perror("hci_open_dev");
    return(1);
  }

  if (enable) {
    set_adv_data(s, uuidstr, major, minor, tx_power);
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
  fprintf(stderr, "usage: %s [-d | -m major -n minor -t txpow -u uuid]\n",
      progname);
  fprintf(stderr, "\t-d: disable BTLE advertisement.\n");
  fprintf(stderr, "\t-m major: major number to advertise.\n");
  fprintf(stderr, "\t-n minor: minor number to advertise.\n");
  fprintf(stderr, "\t-t txpow: transmit power.\n");
  fprintf(stderr, "\t-u uuid: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\n");
  exit(1);
}

int main(int argc, char **argv)
{
  int c;
  const char *uuidstr = NULL;
  int major = -1;
  int minor = -1;
  uint8_t tx_power = 0xff;
  int do_disable = 0;

  while ((c = getopt(argc, argv, "dm:n:t:u:")) != -1) {
    switch(c) {
      case 'd':
        do_disable = 1;
        break;
      case 'm':
        major = atoi(optarg);
        break;
      case 'n':
        minor = atoi(optarg);
        break;
      case 't':
        tx_power = atoi(optarg);
        break;
      case 'u':
        uuidstr = optarg;
        break;
      default:
        usage(argv[0]);
        break;
    }
  }

  if (do_disable) {
    advertise_ibeacon(0, NULL, 0, 0, 0);
    return 0;
  }

  if ((uuidstr == NULL) || (major < 0) || (major > 65535) ||
      (minor < 0) || (minor > 65535)) {
    usage(argv[0]);
  }

  advertise_ibeacon(1, uuidstr, major, minor, tx_power);

  return 0;
}
