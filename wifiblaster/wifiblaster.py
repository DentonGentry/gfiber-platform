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
import errno
import multiprocessing
import os
import re
import subprocess
import sys
import time
import traceback
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
s,size=       Packet size in bytes [1470]
"""


class Error(Exception):
  """Exception superclass representing a nominal test failure."""
  pass


class NotActiveError(Error):
  """Client is not active."""
  pass


class NotAssociatedError(Error):
  """Client is not associated."""
  pass


class NotSupportedError(Error):
  """Packet blasts are not supported."""
  pass


class PktgenError(Error):
  """Pktgen failure."""
  pass


class Iw(object):
  """Interface to iw."""
  # TODO(mikemu): Use an nl80211 library instead.

  def __init__(self, interface):
    """Initializes Iw on a given interface."""
    self._interface = interface

  def _DevInfo(self):
    """Returns the output of 'iw dev <interface> info'."""
    return subprocess.check_output(
        ['iw', 'dev', self._interface, 'info'])

  def _DevStationDump(self):
    """Returns the output of 'iw dev <interface> station dump'."""
    return subprocess.check_output(
        ['iw', 'dev', self._interface, 'station', 'dump'])

  def _DevStationGet(self, client):
    """Returns the output of 'iw dev <interface> station get <client>'."""
    try:
      return subprocess.check_output(
          ['iw', 'dev', self._interface, 'station', 'get', client])
    except subprocess.CalledProcessError as e:
      if e.returncode == 254:
        raise NotAssociatedError
      raise

  def GetClients(self):
    """Returns the associated clients of an interface."""
    return set([client.lower() for client in re.findall(
        r'Station ((?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2})',
        self._DevStationDump())])

  def GetFrequency(self):
    """Returns the frequency of an interface."""
    return int(re.search(r'channel.*\((\d+) MHz\)', self._DevInfo()).group(1))

  def GetPhy(self):
    """Returns the PHY name of an interface."""
    return 'phy%d' % int(re.search(r'wiphy (\d+)', self._DevInfo()).group(1))

  def GetInactiveTime(self, client):
    """Returns the inactive time of a client."""
    return float(re.search(r'inactive time:\s+(\d+) ms',
                           self._DevStationGet(client)).group(1)) / 1000

  def GetRssi(self, client):
    """Returns the RSSI of a client."""
    return float(re.search(r'signal:\s+([-.\d]+)',
                           self._DevStationGet(client)).group(1))


class Mac80211Stats(object):
  """Interface to mac80211 statistics in debugfs."""

  def __init__(self, phy):
    """Initializes Mac80211Stats on a given PHY."""
    self._basedir = os.path.join(
        '/sys/kernel/debug/ieee80211', phy, 'statistics')

  def _ReadCounter(self, counter):
    """Returns a counter read from a file."""
    try:
      with open(os.path.join(self._basedir, counter)) as f:
        return int(f.read())
    except IOError as e:
      if e.errno == errno.ENOENT:
        raise NotSupportedError
      raise

  def GetTransmittedFrameCount(self):
    """Returns the number of successfully transmitted MSDUs."""
    return self._ReadCounter('dot11TransmittedFrameCount')


class Pktgen(object):
  """Interface to pktgen."""

  def __init__(self, interface):
    """Initializes Pktgen on a given interface."""
    self._interface = interface
    self._control_file = '/proc/net/pktgen/pgctrl'
    self._thread_file = '/proc/net/pktgen/kpktgend_1'
    self._device_file = '/proc/net/pktgen/%s' % interface

  def _ReadFile(self, filename):
    """Returns the contents of a file."""
    try:
      with open(filename) as f:
        return f.read()
    except IOError as e:
      if e.errno == errno.ENOENT:
        raise NotSupportedError
      raise

  def _WriteFile(self, filename, s):
    """Writes a string and a newline to a file."""
    try:
      with open(filename, 'w') as f:
        f.write(s + '\n')
    except IOError as e:
      if e.errno == errno.ENOENT:
        raise NotSupportedError
      raise

  @contextlib.contextmanager
  def PacketBlast(self, client, size):
    """Runs a packet blast."""
    # Reset pktgen.
    self._WriteFile(self._control_file, 'reset')
    self._WriteFile(self._thread_file, 'add_device %s' % self._interface)

    # Work around a bug on GFRG200 where transmits hang on queues other than BE.
    self._WriteFile(self._device_file, 'queue_map_min 2')
    self._WriteFile(self._device_file, 'queue_map_max 2')

    # Disable packet count limit.
    self._WriteFile(self._device_file, 'count 0')

    # Set parameters.
    self._WriteFile(self._device_file, 'dst_mac %s' % client)
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


def _PacketBlast(iw, mac80211stats, pktgen, client, duration, fraction, size):
  """Blasts packets at a client and returns a string representing the result."""
  try:
    # Validate client.
    if client not in iw.GetClients():
      raise NotAssociatedError

    with pktgen.PacketBlast(client, size):
      # Wait for the client's inactive time to become zero, which happens when
      # an ack is received from the client. Assume the client has disassociated
      # after 2s.
      start = _gettime()
      while _gettime() < start + 2:
        if iw.GetInactiveTime(client) == 0:
          break
      else:
        raise NotActiveError

      # Wait for block ack session and max PHY rate negotiation.
      time.sleep(.1)

      # Sample transmitted frame count.
      samples = [mac80211stats.GetTransmittedFrameCount()]
      start = _gettime()
      dt = duration / fraction
      for t in [start + dt * (i + 1) for i in xrange(fraction)]:
        time.sleep(t - _gettime())
        samples.append(mac80211stats.GetTransmittedFrameCount())

    # Compute throughputs from samples.
    samples = [8 * size * (after - before) / dt
               for (after, before) in zip(samples[1:], samples[:-1])]

    # Print result.
    print 'version=2 mac=%s throughput=%d rssi=%g frequency=%d samples=%s' % (
        client,
        sum(samples) / len(samples),
        iw.GetRssi(client),
        iw.GetFrequency(),
        ','.join(['%d' % sample for sample in samples]))

  except Error as e:
    print 'version=2 mac=%s error=%s' % (client, e.__class__.__name__)
    traceback.print_exc()
  except Exception as e:
    print 'version=2 mac=%s error=%s' % (client, e.__class__.__name__)
    raise


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

  # If no clients are specified, test all associated clients.
  if not clients:
    clients = sorted(iw.GetClients())

  # Normalize clients.
  clients = [client.lower() for client in clients]

  # Blast packets at each client.
  for client in clients:
    _PacketBlast(iw, mac80211stats, pktgen, client,
                 opt.duration, opt.fraction, opt.size)


if __name__ == '__main__':
  main()
