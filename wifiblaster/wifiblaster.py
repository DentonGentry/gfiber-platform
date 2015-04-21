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

import contextlib
import multiprocessing
import os
import re
import subprocess
import sys
import time
import options


try:
  import monotime  # pylint: disable=unused-import,g-import-not-at-top
except ImportError:
  pass
try:
  _gettime = time.monotonic
except AttributeError:
  _gettime = time.time


_OPTSPEC = """
wifiblaster [options...] [clients...]
--
i,interface=  Name of access point interface [wlan0]
d,duration=   Packet blast duration in seconds [.1]
f,fraction=   Number of samples per duration [10]
s,size=       Packet size in bytes [64]
"""


class Iw(object):
  """Interface to iw."""
  # TODO(mikemu): Use an nl80211 library instead.

  class IwException(Exception):
    pass

  def __init__(self, interface):
    """Initializes Iw on a given interface."""
    self._interface = interface

  def _DevStationDump(self):
    """Returns the output of 'iw dev <interface> station dump'."""
    return subprocess.check_output(
        ['iw', 'dev', self._interface, 'station', 'dump'])

  def _DevInfo(self):
    """Returns the output of 'iw dev <interface> info'."""
    return subprocess.check_output(
        ['iw', 'dev', self._interface, 'info'])

  def GetClients(self):
    """Returns the associated clients."""
    clients = set()
    for line in self._DevStationDump().splitlines():
      m = re.search(r'Station (([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2})', line)
      if m:
        clients.add(m.group(1).lower())
    return clients

  def GetPhy(self):
    """Returns the PHY name."""
    for line in self._DevInfo().splitlines():
      m = re.search(r'wiphy (\d+)', line)
      if m:
        return 'phy%s' % m.group(1)
    raise Iw.IwException('could not get PHY name')


class Mac80211Stats(object):
  """Interface to mac80211 statistics in debugfs."""

  def __init__(self, phy):
    """Initializes Mac80211Stats on a given PHY."""
    self._basedir = os.path.join(
        '/sys/kernel/debug/ieee80211', phy, 'statistics')

  def _ReadCounter(self, counter):
    """Returns a counter read from a file."""
    with open(os.path.join(self._basedir, counter)) as f:
      return int(f.read())

  def GetTransmittedFrameCount(self):
    """Returns the number of successfully transmitted MSDUs."""
    return self._ReadCounter('transmitted_frame_count')


class Pktgen(object):
  """Interface to pktgen."""

  class PktgenException(Exception):
    pass

  def __init__(self, interface):
    """Initializes Pktgen on a given interface."""
    self._interface = interface
    self._control_file = '/proc/net/pktgen/pgctrl'
    self._thread_file = '/proc/net/pktgen/kpktgend_1'
    self._device_file = '/proc/net/pktgen/%s' % interface

  def _ReadFile(self, filename):
    """Returns the contents of a file."""
    with open(filename) as f:
      return f.read()

  def _WriteFile(self, filename, s):
    """Writes a string and a newline to a file."""
    with open(filename, 'w') as f:
      f.write(s + '\n')

  @contextlib.contextmanager
  def PacketBlast(self, client, count, duration, size):
    """Runs a packet blast."""
    # Reset pktgen.
    self._WriteFile(self._control_file, 'reset')
    self._WriteFile(self._thread_file, 'add_device %s' % self._interface)

    # Work around a bug on GFRG200 where transmits hang on queues other than BE.
    self._WriteFile(self._device_file, 'queue_map_min 2')
    self._WriteFile(self._device_file, 'queue_map_max 2')

    # Set parameters.
    self._WriteFile(self._device_file, 'dst_mac %s' % client)
    self._WriteFile(self._device_file, 'count %d' % count)
    self._WriteFile(self._device_file, 'duration %d' % (1000000 * duration))
    self._WriteFile(self._device_file, 'pkt_size %d' % size)

    # Start packet blast.
    p = multiprocessing.Process(target=self._WriteFile,
                                args=[self._control_file, 'start'])
    p.start()

    # Wait for pktgen startup delay. pktgen prints 'Starting' after the packet
    # blast has started.
    while (p.is_alive() and not
           re.search(r'Result: Starting', self._ReadFile(self._device_file))):
      pass

    # Run with-statement body.
    try:
      yield

    # Stop packet blast.
    finally:
      p.terminate()
      p.join()
      if not re.search(r'Result: OK', self._ReadFile(self._device_file)):
        raise Pktgen.PktgenException('packet blast failed')


def _PacketBlast(mac80211stats, pktgen, client, duration, fraction, size):
  """Blasts packets at a client and returns a string representing the result."""
  # Attempt to start an aggregation session.
  with pktgen.PacketBlast(client, 1, 0, size):
    pass

  # Start packet blast and sample transmitted frame count.
  with pktgen.PacketBlast(client, 0, duration, size):
    samples = [mac80211stats.GetTransmittedFrameCount()]
    start = _gettime()
    dt = duration / fraction
    for t in [start + dt * (i + 1) for i in xrange(fraction)]:
      time.sleep(t - _gettime())
      samples.append(mac80211stats.GetTransmittedFrameCount())

  # Compute throughputs from samples.
  samples = [8 * size * (after - before) / dt
             for (after, before) in zip(samples[1:], samples[:-1])]

  # Format result.
  return 'version=1 mac=%s throughput=%d samples=%s' % (
      client,
      sum(samples) / len(samples),
      ','.join(['%d' % sample for sample in samples]))


class Status(object):
  SUCCESS = 0
  UNKNOWN = -1
  NOT_SUPPORTED = -2


def main():
  # Parse and validate arguments.
  o = options.Options(_OPTSPEC)
  (opt, _, clients) = o.parse(sys.argv[1:])
  opt.duration = float(opt.duration)
  opt.fraction = int(opt.fraction)
  opt.size = int(opt.size)
  if opt.duration <= 0:
    o.fatal('duration must be positive')
  if opt.fraction <= 0:
    o.fatal('fraction must be a positive integer')
  if opt.size <= 0:
    o.fatal('size must be a positive integer')

  # Initialize iw, mac80211stats, and pktgen.
  iw = Iw(opt.interface)
  mac80211stats = Mac80211Stats(iw.GetPhy())
  pktgen = Pktgen(opt.interface)

  # Get connected clients.
  connected_clients = iw.GetClients()

  # If no clients are specified, test all connected clients. Otherwise,
  # normalize and validate clients.
  if not clients:
    clients = sorted(connected_clients)
  else:
    clients = [client.lower() for client in clients]
    for client in clients:
      if client not in connected_clients:
        raise ValueError('invalid client: %s' % client)

  # Blast packets at each client.
  for client in clients:
    try:
      print _PacketBlast(mac80211stats, pktgen, client, opt.duration,
                         opt.fraction, opt.size)
    except IOError:
      print 'version=1 mac=%s throughput=%d' % (client, Status.NOT_SUPPORTED)
    except:
      print 'version=1 mac=%s throughput=%d' % (client, Status.UNKNOWN)
      raise


if __name__ == '__main__':
  main()
