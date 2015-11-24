#!/usr/bin/python

"""Utility functions for calling iw and parsing its output."""

import re
import subprocess


def _scan(interface, **kwargs):
  try:
    return subprocess.check_output(('iw', 'dev', interface, 'scan'), **kwargs)
  except subprocess.CalledProcessError:
    return ''


_BSSID_RE = r'BSS (?P<BSSID>([0-9a-f]{2}:?){6})\(on .*\)'
_SSID_RE = r'SSID: (?P<SSID>.*)'
_VENDOR_IE_RE = (r'Vendor specific: OUI (?P<OUI>([0-9a-f]{2}:?){3}), '
                 'data:(?P<data>( [0-9a-f]{2})+)')


class BssInfo(object):
  """Contains info about a BSS, parsed from 'iw scan'."""

  def __init__(self, bssid='', ssid='', security=None, vendor_ies=None):
    self.bssid = bssid
    self.ssid = ssid
    self.vendor_ies = vendor_ies or []
    self.security = security or []

  def __attrs(self):
    return (self.bssid, self.ssid, tuple(sorted(self.vendor_ies)),
            tuple(sorted(self.security)))

  def __eq__(self, other):
    # pylint: disable=protected-access
    return self.__attrs() == other.__attrs()

  def __hash__(self):
    return hash(self.__attrs())

  def __repr__(self):
    return '<BssInfo: SSID=%s BSSID=%s Security=%s Vendor IEs=%s>' % (
        self.ssid, self.bssid, ','.join(self.security),
        ','.join('|'.join(ie) for ie in self.vendor_ies))


# TODO(rofrankel): waveguide also scans. Can we find a way to avoid two programs
# scanning in parallel?
def scan_parsed(interface, **kwargs):
  """Return the parsed results of 'iw scan'."""
  result = []
  bss_info = None
  for line in _scan(interface, **kwargs).splitlines():
    line = line.strip()
    match = re.match(_BSSID_RE, line)
    if match:
      if bss_info:
        result.append(bss_info)
      bss_info = BssInfo(bssid=match.group('BSSID'))
      continue
    match = re.match(_SSID_RE, line)
    if match:
      bss_info.ssid = match.group('SSID')
      continue
    match = re.match(_VENDOR_IE_RE, line)
    if match:
      bss_info.vendor_ies.append((match.group('OUI'),
                                  match.group('data').strip()))
      continue
    if line.startswith('RSN:'):
      bss_info.security.append('WPA2')
    elif line.startswith('WPA:'):
      bss_info.security.append('WPA')
    elif line.startswith('Privacy:'):
      bss_info.security.append('WEP')

  if bss_info:
    result.append(bss_info)

  return result


def find_bssids(interface, vendor_ie_function, include_secure):
  """Return information about interesting access points.

  Args:
    interface:  The wireless interface with which to scan.
    vendor_ie_function:  A function that takes a vendor IE and returns a bool.
    include_secure:  Whether to exclude secure networks.

  Returns:
    Two lists of tuples of the form (SSID, BSSID info dict).  The first list has
    BSSIDs which have a vendor IE accepted by vendor_ie_function, and the second
    list has those which don't.
  """
  parsed = scan_parsed(interface)
  result_with_ie = set()
  result_without_ie = set()

  for bss_info in parsed:
    if bss_info.security and not include_secure:
      continue
    for oui, data in bss_info.vendor_ies:
      if vendor_ie_function(oui, data):
        result_with_ie.add(bss_info)
        break
    else:
      result_without_ie.add(bss_info)

  return result_with_ie, result_without_ie
