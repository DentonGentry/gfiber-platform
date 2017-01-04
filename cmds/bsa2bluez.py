#!/usr/bin/python
"""bsa2bluez: Automatic device import from Broadcom BSA to BlueZ."""

import os
import sys
import xml.etree.ElementTree as ET


def create_device_info_file(filename, link_key):
  with open(filename, 'w') as f:
    f.write('\n')
    f.write('[LinkKey]\n')
    f.write('Key=%s\n' % link_key)
    f.write('Type=0\n')
    f.write('PINLength=4\n')
    f.write('\n')
    f.write('[General]\n')
    f.write('Name=GFRM100\n')
    f.write('SupportedTechnologies=BR/EDR;\n')
    f.write('Trusted=true\n')
    f.write('Blocked=false\n')
    # pylint: disable=line-too-long
    f.write('Services=00001000-0000-1000-8000-00805f9b34fb;00001124-0000-1000-8000-00805f9b34fb;00001200-0000-1000-8000-00805f9b34fb;\n')
    f.write('Class=0x00050c\n')
    f.write('\n')
    f.write('[DeviceID]\n')
    f.write('Source=1\n')
    f.write('Vendor=88\n')
    f.write('Product=8192\n')
    f.write('Version=283\n')


def create_device_cache_file(filename):
  with open(filename, 'w') as f:
    f.write('\n')
    f.write('[General]\n')
    f.write('Name=GFRM100\n')
    f.write('\n')
    f.write('[ServiceRecords]\n')
    # pylint: disable=line-too-long
    f.write('0x00000000=35920900000A000000000900013503191000090004350D350619010009000135031900010900053503191002090006350909656E09006A09010009000935083506190100090100090100252D42726F6164636F6D20426C7565746F6F746820576972656C6573732052656D6F74652053445020536572766572090101250E52656D6F746520436F6E74726F6C0902003503090100\n')
    f.write('0x00010000=3601B60900000A000100000900013503191124090004350D350619010009001135031900110900053503191002090006350909656E09006A0901000900093508350619112409010009000D350F350D350619010009001335031900110901002517476F6F676C6554562052656D6F746520436F6E74726F6C09010125214D756C74692D66756E6374696F6E2072656D6F74652077697468206B65797061640901022506476F6F676C6509020009017109020109011109020208400902030821090204280109020528010902063600BB3600B808222600B305010906A10185417508950126FF00050719002AFF008100C0050C0901A101854019002AFF0375109501150026FF038100C005010980A10185121981299315812593750895018140C0050C0901A1018513092015002564750895018142C08521092175089501150026FF008102852205010922A102093B950175101500264F01810206F0FF09227510964F01150026FF00820102C085F2090275089501150026FF00910285F3090375089510150026FF0081020902073508350609040909010009020B09010009020C090C8009020D280009020E280109020F090318090210090000\n')
    f.write('0x00010001=35520900000A000100010900013503191200090004350D350619010009000135031900010900093508350619120009010009020009010009020109005809020209200009020309011B0902042801090205090001\n')


BT_MAC_FILE = '/tmp/btmacaddress'
BSA_DEVICES_FILE = '/user/bsa/bt_devices.xml'
BLUEZ_STORAGE_DIR = '/user/bluez/lib/bluetooth'


def main():
  if not os.path.exists(BT_MAC_FILE):
    sys.exit('error: %s does not exist' % BT_MAC_FILE)

  with open(BT_MAC_FILE, 'r') as f:
    bt_mac = f.read().upper().rstrip('\n')

  if len(bt_mac) != 17:
    sys.exit('error: bt_mac %s is invalid' % bt_mac)

  try:
    bsa_devices = ET.parse(BSA_DEVICES_FILE)
  except ET.ParseError as e:
    print >>sys.stderr, 'error: failed to parse %r with exception %r' % (
        BSA_DEVICES_FILE, e)
    with open(BSA_DEVICES_FILE, 'r') as f:
      buf = f.read()
    print >>sys.stderr, buf[0:4096]
    sys.exit(1)

  adapter_dir = BLUEZ_STORAGE_DIR + '/' + bt_mac
  if not os.path.isdir(adapter_dir):
    print >>sys.stderr, 'create: BlueZ adapter dir %s' % adapter_dir
    os.makedirs(adapter_dir)

  device_cache_dir = adapter_dir + '/cache'
  if not os.path.isdir(device_cache_dir):
    print >>sys.stderr, 'create: BlueZ device cache dir %s' % device_cache_dir
    os.makedirs(device_cache_dir)

  for parent in bsa_devices.getiterator():
    if parent.tag != 'device':
      continue

    bd_addr = ''
    link_key = ''

    for child in parent:
      if child.tag == 'bd_addr':
        bd_addr = child.text
      elif child.tag == 'link_key':
        link_key = child.text

    bd_addr = bd_addr.upper()
    if len(bd_addr) != 17:
      print >>sys.stderr, 'BSA device: GFRM100 has invalid bd_addr %s' % bd_addr
      continue

    link_key = link_key.upper().replace(':', '')
    if len(link_key) != 32:
      print >>sys.stderr, ('BSA device: GFRM100 has invalid link_key %s'
                           % link_key)
      continue

    print >>sys.stderr, ('BSA device: GFRM100 at bd_addr %s link_key %s'
                         % (bd_addr, link_key))

    device_dir = adapter_dir + '/' + bd_addr
    if not os.path.isdir(device_dir):
      print >>sys.stderr, 'create: BlueZ device dir %s' % device_dir
      os.makedirs(device_dir)
    else:
      print >>sys.stderr, 'exists: BlueZ device dir %s' % device_dir

    device_info_file = device_dir + '/info'
    if not os.path.exists(device_info_file):
      print >>sys.stderr, 'create: BlueZ device info file %s' % device_info_file
      create_device_info_file(device_info_file, link_key)
    else:
      print >>sys.stderr, 'exists: BlueZ device info file %s' % device_info_file

    device_cache_file = device_cache_dir + '/' + bd_addr
    if not os.path.exists(device_cache_file):
      print >>sys.stderr, ('create: BlueZ device cache file %s'
                           % device_cache_file)
      create_device_cache_file(device_cache_file)
    else:
      print >>sys.stderr, ('exists: BlueZ device cache file %s'
                           % device_cache_file)

if __name__ == '__main__':
  main()
