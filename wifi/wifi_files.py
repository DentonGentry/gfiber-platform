#!/usr/bin/python

"""Script that produces a file in /tmp/stations for each connected wifi client.
"""

import copy
import json
import os
import re
import sys
import time
import iw
import utils

filepath = '/tmp/stations'
prev_content = {}


def create_content_dict(content_list):
  """Parses iw info and creates dictionary of content with MAC address as a key.

  Args:
    content_list: The list of content from which to create the dictionary.

  Returns:
    content_dict: The content dictionary of parsed information from content_list
  """
  content_dict = {}
  for content in content_list:
    entry_dict = {}
    for line in content.splitlines()[1:]:
      line = line.strip()
      key, value = line.split(':', 1)
      value = value.strip()
      if key in ['rx bytes', 'rx packets', 'tx bytes', 'tx packets',
                 'tx retries', 'tx failed', 'signal', 'signal avg',
                 'tx bitrate', 'rx bitrate', 'inactive time']:
        try:
          num_value = float(value.split()[0])
          entry_dict[key] = num_value
        except ValueError:
          entry_dict[key] = value
      else:
        entry_dict[key] = value
    station_mac = re.search(r'([0-9A-F]{2}[:-]){5}([0-9A-F]{2})',
                            content, re.IGNORECASE)
    if station_mac:
      content_dict[station_mac.group().lower()] = entry_dict
  return content_dict


def create_files_from_content_dict(content_dict):
  """Parses dictionary of iw info and creates files organized by MAC address.

  Args:
    content_dict: The dict of content from which to create files.
  """
  global prev_content
  if not os.path.exists(filepath):
    os.makedirs(filepath)
  for mac_addr, content in content_dict.items():
    inactive_time = content.pop('inactive time', 0)
    inactive_since = time.time() - inactive_time / 1000
    if mac_addr in prev_content:
      prev_inactive_since = prev_content[mac_addr].get('inactive since', 0)
    else:
      prev_inactive_since = 0
    if abs(inactive_since - prev_inactive_since) > 2:  # over 2 second jitter
      content['inactive since'] = inactive_since
    else:  # if 2 seconds haven't passed, use the old value
      content['inactive since'] = prev_inactive_since
    if content != prev_content.get(mac_addr):
      utils.atomic_write(os.path.join(filepath, mac_addr), json.dumps(content))
  for dirfile in os.listdir(filepath):
    if dirfile not in content_dict:
      os.remove(os.path.join(filepath, dirfile))
  prev_content = copy.deepcopy(content_dict)


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
  if not os.path.exists(filepath):
    os.makedirs(filepath)
  try:
    iw.RUNNABLE_IW()
  except OSError:
    print 'No wifi functionality on this device'
    sys.exit(0)
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
    content_dict = create_content_dict(content_list)
    create_files_from_content_dict(content_dict)
    time.sleep(1)

if __name__ == '__main__':
  main()
