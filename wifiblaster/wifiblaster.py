#!/usr/bin/python
# Copyright 2015 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Wifi packet blaster."""

__author__ = 'mikemu@google.com (Mike Mu)'

import collections
import re
import subprocess
import sys
import options


_OPTSPEC = """
wifiblaster [options...] [clients...]
--
i,interface=  Name of access point interface [wlan0]
c,count=      Number of packets to blast [1000]
s,size=       Packet size [64]
"""

IwStat = collections.namedtuple('IwStat', ['tx_packets',
                                           'tx_retries',
                                           'tx_failed'])


class Iw(object):
  """Interface to iw."""
  # TODO(mikemu): Use an nl80211 library instead.

  def __init__(self, interface):
    """Initializes Iw on a given interface."""
    self._interface = interface

  def _StationDump(self):
    """Returns the output of 'iw dev interface station dump'."""
    return subprocess.check_output(
        ['iw', 'dev', self._interface, 'station', 'dump'])

  def GetClients(self):
    """Returns a dict mapping clients to IwStats at the time of the call."""
    clients = {}
    tx_packets = tx_retries = tx_failed = 0
    for line in reversed(self._StationDump().splitlines()):
      line = line.strip()
      m = re.match(r'tx packets:\s+(\d+)', line)
      if m:
        tx_packets = long(m.group(1))
        continue
      m = re.match(r'tx retries:\s+(\d+)', line)
      if m:
        tx_retries = long(m.group(1))
        continue
      m = re.match(r'tx failed:\s+(\d+)', line)
      if m:
        tx_failed = long(m.group(1))
        continue
      m = re.match(r'Station (([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2})', line)
      if m:
        clients[m.group(1).lower()] = IwStat(tx_packets=tx_packets,
                                             tx_retries=tx_retries,
                                             tx_failed=tx_failed)
        tx_packets = tx_retries = tx_failed = 0
        continue
    return clients


class Pktgen(object):
  """Interface to pktgen."""

  def __init__(self, interface):
    """Initializes Pktgen on a given interface."""
    self._interface = interface
    self._control_file = '/proc/net/pktgen/pgctrl'
    self._thread_file = '/proc/net/pktgen/kpktgend_0'
    self._device_file = '/proc/net/pktgen/%s' % interface

  def _ReloadModule(self):
    """Reloads the pktgen kernel module."""
    subprocess.check_call(['modprobe', '-r', 'pktgen'])
    subprocess.check_call(['modprobe', 'pktgen'])

  def _ReadFile(self, filename):
    """Returns the contents of a file."""
    with open(filename) as f:
      return f.read()

  def _WriteFile(self, filename, s):
    """Writes a string and a newline to a file."""
    with open(filename, 'w') as f:
      f.write(s + '\n')

  def Initialize(self):
    """Resets and initializes pktgen."""
    self._ReloadModule()
    self._WriteFile(self._thread_file, 'add_device %s' % self._interface)
    # Work around a bug on GFRG200 where transmits hang on queues other than BE.
    self._WriteFile(self._device_file, 'queue_map_min 2')
    self._WriteFile(self._device_file, 'queue_map_max 2')

  def SetClient(self, client):
    """Sets the client to blast packets to."""
    self._WriteFile(self._device_file, 'dst_mac %s' % client)

  def SetCount(self, count):
    """Sets the number of packets to blast."""
    self._WriteFile(self._device_file, 'count %d' % count)

  def SetSize(self, size):
    """Set the packet size."""
    self._WriteFile(self._device_file, 'pkt_size %d' % size)

  def PacketBlast(self):
    """Starts a blocking packet blast with the current configuration."""
    self._WriteFile(self._control_file, 'start')
    result = self._ReadFile(self._device_file)
    if re.search(r'Result: OK', result):
      m = re.search(r'(\d+)\(c\d+\+d\d+\) usec', result)
      return float(m.group(1)) / 1000000
    raise Exception('Packet blast failed')


def _PacketBlast(iw, pktgen, client, count, size):
  """Blasts packets at a client and returns a string representing the result."""
  # Configure pktgen.
  pktgen.SetClient(client)
  pktgen.SetCount(count)
  pktgen.SetSize(size)

  try:
    # Get statistics before packet blast.
    before = iw.GetClients()[client]
    # Start packet blast.
    elapsed = pktgen.PacketBlast()
    # Get statistics after packet blast.
    after = iw.GetClients()[client]
  except KeyError:
    return 'mac=%s not connected' % client

  # Compute throughput.
  throughput = 8 * size * (after.tx_packets - before.tx_packets) / elapsed

  # Format result.
  return ('mac=%s tx_packets=%d tx_retries=%d tx_failed=%d elapsed=%g '
          'throughput=%d') % (
      client,
      after.tx_packets - before.tx_packets,
      after.tx_retries - before.tx_retries,
      after.tx_failed - before.tx_failed,
      elapsed,
      throughput)


def main():
  # Parse and validate arguments.
  (opt, _, clients) = options.Options(_OPTSPEC).parse(sys.argv[1:])
  if int(opt.count) <= 0:
    raise Exception('count must be a positive integer')
  if int(opt.size) <= 0:
    raise Exception('size must be a positive integer')

  # Initialize iw and pktgen.
  iw = Iw(opt.interface)
  pktgen = Pktgen(opt.interface)
  pktgen.Initialize()

  # Get connected clients.
  connected_clients = iw.GetClients()

  # If no clients are specified, test all connected clients. Otherwise,
  # normalize and validate clients.
  if not clients:
    clients = sorted(connected_clients.keys())
  else:
    clients = [client.lower() for client in clients]
    for client in clients:
      if client not in connected_clients:
        raise Exception('Invalid client: %s' % client)

  # Blast packets at each client.
  for client in clients:
    print _PacketBlast(iw, pktgen, client, opt.count, opt.size)


if __name__ == '__main__':
  main()
