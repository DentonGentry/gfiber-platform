#!/usr/bin/python

"""Utility functions for calling iw and parsing its output."""

import re
import subprocess


GFIBER_VENDOR_IE_OUI = 'f4:f5:e8'
GFIBER_OUIS = ['00:1a:11', 'f4:f5:e8', 'f8:8f:ca']
VENDOR_IE_FEATURE_ID_AUTOPROVISIONING = '01'
DEFAULT_GFIBERSETUP_SSID = 'GFiberSetupAutomation'


_BSSID_RE = r'BSS (?P<BSSID>([0-9a-f]{2}:?){6})\(on .*\)'
_SSID_RE = r'SSID: (?P<SSID>.*)'
_RSSI_RE = r'signal: (?P<RSSI>.*) dBm'
_VENDOR_IE_RE = (r'Vendor specific: OUI (?P<OUI>([0-9a-f]{2}:?){3}), '
                 'data:(?P<data>( [0-9a-f]{2})+)')


def _scan(band, **kwargs):
  try:
    return subprocess.check_output(('wifi', 'scan', '-b', band), **kwargs)
  except subprocess.CalledProcessError:
    return ''


class BssInfo(object):
  """Contains info about a BSS, parsed from 'iw scan'."""

  def __init__(self, bssid='', ssid='', rssi=-100, security=None,
               vendor_ies=None):
    self.bssid = bssid
    self.ssid = ssid
    self.rssi = rssi
    self.vendor_ies = vendor_ies or []
    self.security = security or []

  def __attrs(self):
    return (self.bssid, self.ssid, tuple(sorted(self.vendor_ies)),
            tuple(sorted(self.security)), self.rssi)

  def __eq__(self, other):
    # pylint: disable=protected-access
    return isinstance(other, BssInfo) and self.__attrs() == other.__attrs()

  def __ne__(self, other):
    return not self.__eq__(other)

  def __hash__(self):
    return hash(self.__attrs())

  def __repr__(self):
    return '<BssInfo: SSID=%s BSSID=%s Security=%s Vendor IEs=%s>' % (
        self.ssid, self.bssid, ','.join(self.security),
        ','.join('|'.join(ie) for ie in self.vendor_ies))


# TODO(rofrankel): waveguide also scans. Can we find a way to avoid two programs
# scanning in parallel?
def scan_parsed(band, **kwargs):
  """Return the parsed results of 'iw scan'."""
  result = []
  bss_info = None
  for line in _scan(band, **kwargs).splitlines():
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
    match = re.match(_RSSI_RE, line)
    if match:
      bss_info.rssi = float(match.group('RSSI'))
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


def find_bssids(band, include_secure):
  """Return information about interesting access points.

  Args:
    band:  The band on which to scan.
    include_secure:  Whether to exclude secure networks.

  Returns:
    A list of (BSSID, priority) tuples, prioritizing BSSIDs with the GFiber
    provisioning vendor IE > GFiber APs > other APs, and by RSSI within each
    group.
  """
  parsed = scan_parsed(band)
  bssids = set()

  for bss_info in parsed:
    if bss_info.security and not include_secure:
      continue

    for oui, data in bss_info.vendor_ies:
      if oui == GFIBER_VENDOR_IE_OUI:
        octets = data.split()
        if octets[0] == '03' and not bss_info.ssid:
          bss_info.ssid = ''.join(octets[1:]).decode('hex')
          continue

    # Some of our devices (e.g. Frenzy) can't see vendor IEs.  If we find a
    # hidden network no vendor IEs or SSID, guess 'GFiberSetupAutomation'.
    if not bss_info.ssid and not bss_info.vendor_ies:
      bss_info.ssid = DEFAULT_GFIBERSETUP_SSID

    bssids.add(bss_info)

  return [(bss_info, _bssid_priority(bss_info)) for bss_info in bssids]


def _bssid_priority(bss_info):
  result = 4 if bss_info.bssid[:8] in GFIBER_OUIS else 2
  for oui, data in bss_info.vendor_ies:
    if (oui == GFIBER_VENDOR_IE_OUI and
        data.startswith(VENDOR_IE_FEATURE_ID_AUTOPROVISIONING)):
      result = 5

  return result + (100 + (max(bss_info.rssi, -100))) / 100.0
