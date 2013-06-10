#!/usr/bin/python
# Copyright 2013 Google Inc. All Rights Reserved.
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

"""Continuous TV-related statistics monitor.

Like iostat/vmstat, but for TV.
"""

__author__ = 'dgentry@google.com (Denton Gentry)'

import json
import re
import socket
import struct
import sys
import time
import options


optspec = """
tvstat [options...] <interval>
--
"""


# unit tests can override these with fake versions
PLATFORM = '/etc/platform'
PROC_NET_DEV = '/proc/net/dev'
PROC_NET_UDP = '/proc/net/udp'
DC_JSON = '/tmp/cwmp/monitoring/dejittering/tr_135_total_decoderstats%d.json'
DJ_JSON = '/tmp/cwmp/monitoring/dejittering/tr_135_total_djstats%d.json'
TS_JSON = '/tmp/cwmp/monitoring/ts/tr_135_total_tsstats%d.json'


# Field numbers in /proc/net/dev
RX_ERRS = 3
RX_DROP = 4
RX_FIFO = 5
RX_FRAME = 6


def GetIfErrors(dev):
  """Return (rxerr, rxdrop, rxfifo, rxframe) for network interface named dev."""
  for line in open(PROC_NET_DEV):
    f = line.split()
    dev_colon = dev + ':'
    if f[0] == dev_colon:
      return (int(f[RX_ERRS]), int(f[RX_DROP]),
              int(f[RX_FIFO]), int(f[RX_FRAME]))
  return (0, 0, 0, 0)


def UnpackAlanCoxIP(packed):
  """Convert hex IP addresses to strings.

  /proc/net/igmp and /proc/net/udp both contain IP addresses printed as
  a hex string, _without_ calling ntohl() first.

  Example from /proc/net/udp on a little endian machine:
  sl  local_address rem_address   st tx_queue rx_queue ...
  464: 010002E1:07D0 00000000:0000 07 00000000:00000000 ...

  On a big-endian machine:
  sl  local_address rem_address   st tx_queue rx_queue ...
  464: E1020001:07D0 00000000:0000 07 00000000:00000000 ...

  Args:
    packed: the hex thingy.
  Returns:
    A conventional dotted quad IP address encoding.
  """
  return socket.inet_ntop(socket.AF_INET, struct.pack('=L', int(packed, 16)))


def ParseProcNetUDP():
  """Parse /proc/net/udp, returns dict of drops for all entries.

    sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt
    464: 010002E1:07D0 00000000:0000 07 00000000:00000000 00:00000000 00000000

    uid  timeout inode ref pointer drops
    0        0 6187 2 b1f64e00 0

  Returns:
    a dict indexed by ip:port of the number of dropped packets in UDP.
  """
  udp = dict()
  with open(PROC_NET_UDP) as f:
    for line in f:
      try:
        line = ' '.join(line.split())
        fields = re.split('[ :]', line)
        ip = UnpackAlanCoxIP(fields[2])
        port = int(fields[3], 16)
        key = '%s:%d' % (ip, port)
        current = udp.get(key, 0)
        udp[key] = current + int(fields[17])
      except (ValueError, IndexError):
        # comment line, or something
        continue
  return udp


def GetUDPDrops():
  """Return UDP Drops for all streams."""
  udp = ParseProcNetUDP()
  drops = 0
  for i in range(1, 9):
    try:
      d = json.load(open(TS_JSON % i))
      mc = d['STBService'][0]['MainStream'][0]['MulticastStats']
      ip = mc['MulticastGroup']
      drops += int(udp[ip])
    except (IOError, KeyError, IndexError):
      continue
  return drops


def GetTsErrors():
  """Return Errors from sagesrv JSON files.

  Returns:
    (PacketDiscontinuityCount, DropBytes, DropPackets, PacketErrorCount)
    for all existing MainStreams.
  """
  errs = {
      'PacketDiscontinuityCounter': 0, 'DropBytes': 0,
      'DropPackets': 0, 'PacketErrorCount': 0,
  }
  for i in range(1, 9):
    try:
      d = json.load(open(TS_JSON % i))
      mpeg = d['STBService'][0]['MainStream'][0]['MPEG2TSStats']
    except IOError:
      continue
    pdc = int(mpeg.get('PacketDiscontinuityCounter', 0))
    errs['PacketDiscontinuityCounter'] += pdc
    errs['DropBytes'] += int(mpeg.get('DropBytes', 0))
    errs['DropPackets'] += int(mpeg.get('DropPackets', 0))
    errs['PacketErrorCount'] += int(mpeg.get('PacketErrorCount', 0))
  return errs


def GetVideoHardwareErrors():
  """Return Errors from miniclient JSON files for the Video hardware.

  Returns:
    a dict populated with stats from the hardware pipelines.
  """
  errs = {
      'VideoDecodeErrors': 0, 'DecodeOverflows': 0, 'DecodeDrops': 0,
      'DisplayErrors': 0, 'DisplayDrops': 0, 'DisplayUnderflows': 0,
      'VideoWatchdogs': 0, 'AudioDecodeErrors': 0, 'AudioFifoOverflows': 0,
      'AudioFifoUnderflows': 0, 'AudioWatchdogs': 0, 'AVPtsDifference' : 0
  }
  for i in range(1, 9):
    try:
      d = json.load(open(DC_JSON % i))
      decode = d['STBService'][0]['MainStream'][0]['DecoderStats']
    except IOError:
      continue
    errs['VideoDecodeErrors'] += int(decode.get('VideoDecodeErrors', 0))
    errs['DecodeOverflows'] += int(decode.get('DecodeOverflows', 0))
    errs['DecodeDrops'] += int(decode.get('DecodeDrops', 0))
    errs['DisplayErrors'] += int(decode.get('DisplayErrors', 0))
    errs['DisplayDrops'] += int(decode.get('DisplayDrops', 0))
    errs['DisplayUnderflows'] += int(decode.get('DisplayUnderflows', 0))
    errs['VideoWatchdogs'] += int(decode.get('VideoWatchdogs', 0))
    errs['AudioDecodeErrors'] += int(decode.get('AudioDecodeErrors', 0))
    errs['AudioFifoOverflows'] += int(decode.get('AudioFifoOverflows', 0))
    errs['AudioFifoUnderflows'] += int(decode.get('AudioFifoUnderflows', 0))
    errs['AudioWatchdogs'] += int(decode.get('AudioWatchdogs', 0))
    audio_pts = int(decode.get('AudioPts', 0))
    video_pts = int(decode.get('VideoPts', 0))
    if audio_pts and video_pts:
      errs['AVPtsDifference'] = audio_pts - video_pts
  return errs


def GetVideoSoftwareErrors():
  """Return Errors from miniclient JSON files for software errors.

  Returns:
    a dict populated with Overrruns, Underruns, and EmptyBufferTime.
  """
  errs = {
      'Overrruns': 0, 'Underruns': 0, 'EmptyBufferTime': 0
  }
  for i in range(1, 9):
    try:
      d = json.load(open(DJ_JSON % i))
      decode = d['STBService'][0]['MainStream'][0]['DejitteringStats']
    except IOError:
      continue
    errs['Overrruns'] += int(decode.get('Overrruns', 0))
    errs['Underruns'] += int(decode.get('Underruns', 0))
    errs['EmptyBufferTime'] += int(decode.get('EmptyBufferTime', 0))
  return errs



def TvBoxStats(old):
  new = dict()
  new.update(GetVideoHardwareErrors())
  new.update(GetVideoSoftwareErrors())
  print '%6d %5d %5d %5d  %5d %5d %5d %5d %5d %6d %4d %4d' % (
      Delta(new, old, 'VideoDecodeErrors'),
      Delta(new, old, 'DecodeOverflows'),
      Delta(new, old, 'DecodeDrops'),
      Delta(new, old, 'AudioDecodeErrors'),
      Delta(new, old, 'DisplayErrors'),
      Delta(new, old, 'DisplayDrops'),
      Delta(new, old, 'DisplayUnderflows'),
      Delta(new, old, 'AudioFifoOverflows'),
      Delta(new, old, 'AudioFifoUnderflows'),
      Delta(new, old, 'AVPtsDifference'),
      Delta(new, old, 'VideoWatchdogs'),
      Delta(new, old, 'AudioWatchdogs'))
  return new


def StorageBoxStats(old):
  new = dict()
  (new['eth0_rxerr'], new['eth0_rxdrop'],
   new['eth0_rxfifo'], new['eth0_rxframe']) = GetIfErrors('eth0')
  (new['br0_rxerr'], new['br0_rxdrop'],
   new['br0_rxfifo'], new['br0_rxframe']) = GetIfErrors('br0')
  new.update(GetTsErrors())
  new['UdpDrops'] = GetUDPDrops()
  print '%7d %5d %5d %6d %7d %4d %4d %4d %5d %4d %4d %4d %5d' % (
      Delta(new, old, 'PacketDiscontinuityCounter'),
      Delta(new, old, 'DropBytes'),
      Delta(new, old, 'DropPackets'),
      Delta(new, old, 'PacketErrorCount'),
      Delta(new, old, 'UdpDrops'),
      Delta(new, old, 'eth0_rxerr'),
      Delta(new, old, 'eth0_rxdrop'),
      Delta(new, old, 'eth0_rxfifo'),
      Delta(new, old, 'eth0_rxframe'),
      Delta(new, old, 'br0_rxerr'),
      Delta(new, old, 'br0_rxdrop'),
      Delta(new, old, 'br0_rxfifo'),
      Delta(new, old, 'br0_rxframe'))
  return new


def Delta(new, old, field):
  """Return difference new - old for field."""
  if field in old:
    return new[field] - old[field]
  else:
    return new[field]


def IsTVBox():
  platform = open(PLATFORM).read()
  return True if 'GFHD' in platform else False


def main():
  o = options.Options(optspec)
  _, _, extra = o.parse(sys.argv[1:])

  interval = int(extra[0]) if len(extra) else -1
  tvbox = IsTVBox()

  new = dict()
  old = dict()

  if tvbox:
    print '--------decoder---------- ------------------display---------------------'
    print 'verror vover vdrop aerror  verr vdrop vundr aover aundr a2vpts vwch awch'
  else:
    print '---------sagesrv------------------ --------eth0-------- --------br0---------'
    print ' discon dropb dropp pkterr udpdrop  err drop fifo frame  err drop fifo frame'

  while True:
    if tvbox:
      new = TvBoxStats(old)
    else:
      new = StorageBoxStats(old)

    if interval <= 0:
      return 0

    time.sleep(interval)
    old = new

  return 0


if __name__ == '__main__':
  sys.exit(main())
