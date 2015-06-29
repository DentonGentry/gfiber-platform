#!/usr/bin/python

"""Script that produces a file in /tmp/stations for each connected wifi client.
"""

import os
import re
import time
import iw
import utils

filepath = '/tmp/stations'


def create_files_from_wifi_info(content_list):
  """Parses iw info and creates files organized by station name/MAC address.

  Args:
    content_list: The list of content from which to create files.
  """
  contents_dict = dict()
  for entry in content_list:
    station_mac = re.search(r'([0-9A-F]{2}[:-]){5}([0-9A-F]{2})',
                            entry, re.IGNORECASE)
    if station_mac:
      contents_dict[station_mac.group().lower()] = entry
  if not os.path.exists(filepath):
    os.makedirs(filepath)
  for mac_addr in contents_dict:
    utils.atomic_write(os.path.join(filepath, mac_addr),
                       contents_dict.get(mac_addr))
  for dirfile in os.listdir(filepath):
    if dirfile not in contents_dict:
      os.remove(os.path.join(filepath, dirfile))


def parse_interface(content_list, info_string):
  """Add the new information from given string to list.

  After
    parse_interface(['Station 01:02:03:04:05:06'], 'Station 01:02:03:04:05:07')
  content_list will be
    ['Station 01:02:03:04:05:06', 'Station 01:02:03:04:05:07']

  Args:
    content_list: The old list of content.
    info_string: The string of new information to be added.
  """
  if info_string.startswith('Station'):
    for info in info_string.split('\nStation'):
      if info:
        if not info.endswith('\n'):
          info += '\n'
        if not info.startswith('Station'):
          content_list.append('Station' + info)
        else:
          content_list.append(info)


def main():
  while True:
    content_list = []
    for band in ['2.4', '5']:
      for suffix in ['', '_portal']:
        interface = iw.find_interface_from_band(
            band, iw.INTERFACE_TYPE.ap, suffix)
        if interface is not None:
          info_string = iw.station_dump(interface)
          if info_string is None:
            info_string = ''
          parse_interface(content_list, info_string)
    create_files_from_wifi_info(content_list)
    time.sleep(1)

if __name__ == '__main__':
  main()
