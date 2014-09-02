#!/usr/bin/python
# Copyright 2014 Google Inc. All Rights Reserved.
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

"""Wifi channel selection and roaming daemon."""

from collections import namedtuple
import hmac
import os
import random
import re
import select
import socket
import struct
import subprocess
import sys
import time
import zlib
import options


try:
  import monotime  # pylint: disable=unused-import,g-import-not-at-top
except ImportError:
  pass
try:
  gettime = time.monotonic
except AttributeError:
  gettime = time.time


optspec = """
waveguide [options...]
--
high-power        This high-powered AP takes priority over low-powered ones
fake              Create a fake instance using phy0 / wlan0
initial-scans=    Number of immediate full channel scans at startup [0]
scan-interval=    Seconds between full channel scan cycles (0 to disable) [0]
tx-interval=      Seconds between state transmits (0 to disable) [3]
D,debug           Increase (non-anonymized!) debug output level
"""

PROTO_MAGIC = 'wave'
PROTO_VERSION = 1

# TODO(apenwarr): not sure what's the right multicast address to use.
# MCAST_ADDRESS = '224.0.0.2'  # "all routers" address
MCAST_ADDRESS = '239.255.0.1'  # "administratively scoped" RFC2365 subnet
MCAST_PORT = 4442


debug_level = 0


def Log(s, *args):
  if args:
    print s % args
  else:
    print s
  sys.stdout.flush()


def Debug(s, *args):
  if debug_level:
    Log(s, *args)


freq_to_chan = {}     # a mapping from wifi frequencies (MHz) to channel no.
chan_to_freq = {}     # a mapping from channel no. to wifi frequency (MHz)


class DecodeError(Exception):
  pass


# pylint: disable=invalid-name
class ApFlags(object):
  Can2G = 0x01          # device supports 2.4 GHz channels
  Can5G = 0x02          # device supports 5 GHz channels
  HighPower = 0x10      # high-power device takes precedence over low-power


PRE_FMT = '!4sB'

Header = namedtuple(
    'Header',
    'bss_len,chan_len,assoc_len,arp_len')
HEADER_FMT = '!IIII'

# struct representing an AP's identity.
#
# now: system local time.time().
# uptime_ms: uptime of the waveguide process, in milliseconds.
# consensus_key: an agreed-upon key between all waveguide instances, to be
#   used for anonymizing MAC and IP addresses in log messages.
# mac: the local MAC address of this wifi AP.
# flags: see ApFlags.
Me = namedtuple('Me', 'now,uptime_ms,consensus_key,mac,flags')
Me_FMT = '!QQ16s6sI'

# struct representing observed information about other APs in the vicinity.
#
# is_ours: true if the given BSS is part of our waveguide group.
# mac: the MAC address of the given AP.
# freq: the channel frequency (MHz) of the given AP.
# rssi: the power level received from this AP.
# flags: see ApFlags.
# last_seen: the time since this AP was last scanned.
BSS = namedtuple('BSS', 'is_ours,mac,freq,rssi,flags,last_seen')
BSS_FMT = '!B6sHbII'

# struct representing observed information about traffic on a channel.
#
# freq: the channel frequency (MHz) that we observed.
# noise_dbm: the noise level observed on the channel.
# observed_ms: length of time (ms) that we have observed the channel.
# busy_ms: length of time (ms) that the channel was seen to be busy, where
#    the traffic was *not* related to our own BSSID.
Channel = namedtuple('Channel', 'freq,noise_dbm,observed_ms,busy_ms')
Channel_FMT = '!HbII'

# struct representing stations associated with an AP.
#
# mac: the MAC address of the station.
# rssi: a running average of the signal strength received from the station.
# last_seen: the time of the last packet received from the station.
Assoc = namedtuple('Assoc', 'mac,rssi,last_seen')
Assoc_FMT = '!6sbI'

# struct representing kernel ARP table entries.
#
# ip: the IP address of the node.
# mac: the MAC address corresponding to that IP address.
# last_seen: the time a packet was last received from this node.
ARP = namedtuple('ARP', 'ip,mac,last_seen')
ARP_FMT = '!4s6sI'

# struct representing the complete visible state of a waveguide node.
# (combination of the above structs)
#
# me: a Me() object corresponding to the AP's identity.
# seen_bss: a list of BSS().
# channel_survey: a list of Channel()
# assoc: a list of Assoc()
# arp: a list of ARP().
State = namedtuple('State', 'me,seen_bss,channel_survey,assoc,arp')


# TODO(apenwarr): use "format preserving encryption" here instead.
#   That will guarantee a 1:1 mapping of original to anonymized strings,
#   with no chance of accidental hash collisions.  Hash collisions could
#   be misleading.
def Anonymize(consensus_key, s):
  return hmac.new(consensus_key, s).digest()[:len(s)]


def EncodeIP(ip):
  return socket.inet_aton(ip)


def DecodeIP(ipbin):
  return socket.inet_ntoa(ipbin)


def DecodeMAC(macbin):
  assert len(macbin) == 6
  return ':'.join(['%02x' % ord(i) for i in macbin])


def EncodeMAC(mac):
  s = mac.split(':')
  assert len(s) == 6
  return ''.join([chr(int(i, 16)) for i in s])


def EncodePacket(me,
                 seen_bss_list,
                 channel_survey_list,
                 assoc_list,
                 arp_list):
  """Generate a binary waveguide packet for sending via multicast."""
  now = me.now
  me_out = struct.pack(Me_FMT, me.now, me.uptime_ms,
                       me.consensus_key, me.mac, me.flags)
  bss_out = ''
  for bss in seen_bss_list:
    bss_out += struct.pack(BSS_FMT,
                           bss.is_ours,
                           bss.mac,
                           bss.freq,
                           bss.rssi,
                           bss.flags,
                           now - bss.last_seen)
  chan_out = ''
  for chan in channel_survey_list:
    chan_out += struct.pack(Channel_FMT,
                            chan.freq,
                            chan.noise_dbm,
                            chan.observed_ms,
                            chan.busy_ms)
  assoc_out = ''
  for assoc in assoc_list:
    assoc_out += struct.pack(Assoc_FMT,
                             assoc.mac,
                             assoc.rssi,
                             now - assoc.last_seen)
  arp_out = ''
  for arp in arp_list:
    arp_out += struct.pack(ARP_FMT,
                           arp.ip,
                           arp.mac,
                           now - arp.last_seen)
  header_out = struct.pack(HEADER_FMT,
                           len(bss_out),
                           len(chan_out),
                           len(assoc_out),
                           len(arp_out))
  data = header_out + me_out + bss_out + chan_out + assoc_out + arp_out
  pre = struct.pack(PRE_FMT, PROTO_MAGIC, PROTO_VERSION)
  return pre + zlib.compress(data)


class Eater(object):
  """A simple wrapper for consuming bytes from the front of a string."""

  def __init__(self, data):
    """Create an Eater instance.

    Args:
      data: the byte array that will be consumed by this instance.
    """
    assert isinstance(data, bytes)
    self.data = data
    self.ofs = 0

  def Eat(self, n):
    """Consumes the next n bytes of the string and returns them.

    Args:
      n: the number of bytes to consume.
    Returns:
      n bytes
    Raises:
      DecodeError: if there are not enough bytes left.
    """
    if len(self.data) < self.ofs + n:
      raise DecodeError('short packet: ofs=%d len=%d wanted=%d'
                        % (self.ofs, len(self.data), n))
    b = self.data[self.ofs:self.ofs+n]
    self.ofs += n
    return b

  def Remainder(self):
    """Consumes and returns all the remaining bytes."""
    return self.Eat(len(self.data) - self.ofs)

  def Unpack(self, fmt):
    """Consumes exactly enough bytes to run struct.unpack(fmt) on them.

    Args:
      fmt: a format string compatible with struct.unpack.
    Returns:
      The result of struct.unpack on the bytes using that format string.
    Raises:
      DecodeError: if there are not enough bytes left.
      ...or anything else struct.unpack might raise.
    """
    n = struct.calcsize(fmt)
    result = struct.unpack(fmt, self.Eat(n))
    return result

  def Iter(self, fmt, nbytes):
    """Consume and unpack a series of structs of struct.unpack(fmt).

    Args:
      fmt: a format string compatible with struct.unpack.
      nbytes: the total number of bytes in the array.  Must be a multiple
          of struct.calcsize(fmt).
    Yields:
      A series of struct.unpack(fmt) tuples.
    """
    e = Eater(self.Eat(nbytes))
    while e.ofs < len(e.data):
      yield e.Unpack(fmt)


def DecodePacket(p):
  """Decode a received binary waveguide packet into a State() structure."""
  e = Eater(p)
  magic, ver = e.Unpack(PRE_FMT)
  if magic != PROTO_MAGIC:
    raise DecodeError('expected magic=%r, got %r' % (PROTO_MAGIC, magic))
  if ver != PROTO_VERSION:
    raise DecodeError('expected proto_ver=%r, got %r' % (PROTO_VERSION, ver))

  compressed = e.Remainder()

  e = Eater(zlib.decompress(compressed))

  (bss_len, chan_len, assoc_len, arp_len) = e.Unpack(HEADER_FMT)

  me = Me(*e.Unpack(Me_FMT))
  bss_list = [BSS(*i) for i in e.Iter(BSS_FMT, bss_len)]
  chan_list = [Channel(*i) for i in e.Iter(Channel_FMT, chan_len)]
  assoc_list = [Assoc(*i) for i in e.Iter(Assoc_FMT, assoc_len)]
  arp_list = [ARP(*i) for i in e.Iter(ARP_FMT, arp_len)]

  state = State(me=me,
                seen_bss=bss_list,
                channel_survey=chan_list,
                assoc=assoc_list,
                arp=arp_list)
  return state


class MulticastSocket(object):
  """A simple class for wrapping multicast send/receive activities."""

  def __init__(self, hostport):
    self.host, self.port = hostport

    # A multicast receiver needs to be bound to the right port and have
    # IP_ADD_MEMBERSHIP, but it doesn't care about the remote address.
    self.rsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    self.rsock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    self.rsock.bind(('', self.port))
    mreq = struct.pack('4sl', socket.inet_aton(self.host), socket.INADDR_ANY)
    self.rsock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

    # A multicast transmitter has an arbitrary local address but the remote
    # address needs to be the multicast address:port.
    self.wsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    self.wsock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
    self.wsock.connect((self.host, self.port))
    self.wsock.shutdown(socket.SHUT_RD)

  def Send(self, data):
    return self.wsock.send(data)

  def Recv(self):
    return self.rsock.recvfrom(65536)


# TODO(apenwarr): write an async version of this.
#  ...so that we can run several commands (eg. scan on multiple interfaces)
#  in parallel while still processing packets.  Preparation for that is
#  why we use a callback instead of a simple return value.
def RunProc(callback, *args, **kwargs):
  p = subprocess.Popen(*args,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE,
                       **kwargs)
  stdout, stderr = p.communicate()
  errcode = p.wait()
  callback(errcode, stdout, stderr)


# TODO(apenwarr): run background offchannel sometimes for better survey dump
# TODO(apenwarr): rescan in the background especially when there are no assocs
class WlanManager(object):
  """A class representing one wifi interface on the local host."""

  def __init__(self, phyname, vdevname, high_power):
    self.phyname = phyname
    self.vdevname = vdevname
    self.mac = '\0\0\0\0\0\0'
    self.flags = 0
    self.allowed_freqs = set()
    if high_power:
      self.flags |= ApFlags.HighPower
    self.bss_list = {}
    self.channel_survey_list = {}
    self.assoc_list = {}
    self.arp_list = {}
    self.peer_list = {}
    self.starttime = gettime()
    self.consensus_start = self.starttime
    self.consensus_key = os.urandom(16)  # TODO(apenwarr): rotate occasionally
    self.mcast = MulticastSocket((MCAST_ADDRESS, MCAST_PORT))
    self.next_scan_time = None
    self.scan_idx = -1

  # TODO(apenwarr): when we have async subprocs, add those here
  def GetReadFds(self):
    return [self.mcast.rsock]

  def ReadReady(self):
    """Call this when select.select() returns true on GetReadFds()."""
    data, hostport = self.mcast.Recv()
    p = DecodePacket(data)
    Debug('recv: from %r uptime=%d key=%r',
          hostport, p.me.uptime_ms, p.me.consensus_key[:4])
    # the waveguide that has been running the longest gets to win the contest
    # for what anonymization key to use.  This prevents disruption in case
    # devices come and go.
    # TODO(apenwarr): make sure this doesn't accidentally undo key rotations.
    #   ...once we even do key rotations.
    consensus_start = gettime() - p.me.uptime_ms/1000.0
    if consensus_start < self.consensus_start:
      self.consensus_key = p.me.consensus_key
      self.consensus_start = consensus_start
    if p.me.mac == self.mac:
      Debug('ignoring packet from self')
      return
    if p.me.consensus_key != self.consensus_key:
      Debug('ignoring peer due to key mismatch')
    if p.me.mac not in self.peer_list:
      self.peer_list[p.me.mac] = p
      Log('%r: added a peer: %r',
          self.vdevname, DecodeMAC(Anonymize(self.consensus_key, p.me.mac)))

  def SendUpdate(self):
    """Constructs and sends a waveguide packet on the multicast interface."""
    me = Me(now=time.time(),
            uptime_ms=(gettime() - self.starttime) * 1000,
            consensus_key=self.consensus_key,
            mac=self.mac,
            flags=self.flags)
    p = EncodePacket(me=me,
                     seen_bss_list=self.bss_list.values(),
                     channel_survey_list=self.channel_survey_list.values(),
                     assoc_list=self.assoc_list.values(),
                     arp_list=self.arp_list.values())
    if debug_level >= 2: Debug('sending: %r', me)
    Debug('sent %r: %r bytes', self.vdevname, self.mcast.Send(p))

  def DoScans(self, initial_scans, scan_interval):
    """Calls programs and reads files to obtain the current wifi status."""
    now = gettime()
    if not self.next_scan_time:
      Log('%r: startup (initial_scans=%d).', self.vdevname, initial_scans)
      self._ReadArpTable()
      RunProc(callback=self._PhyResults,
              args=['iw', 'phy', self.phyname, 'info'])
      RunProc(callback=self._DevResults,
              args=['iw', 'dev', self.vdevname, 'info'])
      # channel scan more than once in case we miss hearing a beacon
      for _ in range(initial_scans):
        RunProc(callback=self._ScanResults,
                args=['iw', 'dev', self.vdevname, 'scan',
                      'lowpri', 'ap-force',
                      'passive'])
      self.next_scan_time = now
    elif not self.allowed_freqs:
      Log('%r: no allowed frequencies.', self.vdevname)
    elif scan_interval and now > self.next_scan_time:
      self.scan_idx = (self.scan_idx + 1) % len(self.allowed_freqs)
      scan_freq = list(sorted(self.allowed_freqs))[self.scan_idx]
      Log('%r: scanning %d MHz (%d/%d)',
          self.vdevname, scan_freq, self.scan_idx, len(self.allowed_freqs))
      RunProc(callback=self._ScanResults,
              args=['iw', 'dev', self.vdevname, 'scan',
                    'freq', str(scan_freq),
                    'lowpri', 'ap-force',
                    'passive'])
      chan_interval = scan_interval / len(self.allowed_freqs)
      # Randomly fiddle with the timing to avoid permanent alignment with
      # other nodes also doing scans.  If we're perfectly aligned with
      # another node, they might never see us in their periodic scan.
      chan_interval = random.uniform(chan_interval * 0.5,
                                     chan_interval * 1.5)
      self.next_scan_time += chan_interval

    # These change in the background, not as the result of a scan
    RunProc(callback=self._SurveyResults,
            args=['iw', 'dev', self.vdevname, 'survey', 'dump'])
    RunProc(callback=self._AssocResults,
            args=['iw', 'dev', self.vdevname, 'station', 'dump'])

  def _ReadArpTable(self):
    """Reads the kernel's ARP entries."""
    now = time.time()
    data = open('/proc/net/arp', 'r', 64*1024).read(64*1024)
    lines = data.split('\n')[1:]  # skip header line
    for line in lines:
      g = re.match(r'(\d+\.\d+\.\d+\.\d+)\s+.*\s+'
                   r'(([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2})',
                   line)
      if g:
        ip = EncodeIP(g.group(1))
        mac = EncodeMAC(g.group(2))
        self.arp_list[mac] = ARP(ip=ip, mac=mac, last_seen=now)
        Debug('arp %r', self.arp_list[mac])

  def _PhyResults(self, errcode, stdout, stderr):
    """Callback for 'iw phy xxx info' results."""
    Debug('phy %r err:%r stdout:%r stderr:%r',
          self.phyname, errcode, stdout[:70], stderr)
    if errcode: return
    for line in stdout.split('\n'):
      line = line.strip()
      g = re.match(r'\* (\d+) MHz \[(\d+)\] \((.*)\)', line)
      if g:
        freq = int(g.group(1))
        chan = int(g.group(2))
        disabled = (g.group(3) == 'disabled')
        Debug('phy freq=%d chan=%d disabled=%d', freq, chan, disabled)
        if not disabled:
          if freq / 100 == 24:
            self.flags |= ApFlags.Can2G
          if freq / 1000 == 5:
            self.flags |= ApFlags.Can5G
          self.allowed_freqs.add(freq)
          freq_to_chan[freq] = chan
          chan_to_freq[chan] = freq

  def _DevResults(self, errcode, stdout, stderr):
    """Callback for 'iw dev xxx info' results."""
    Debug('dev err:%r stdout:%r stderr:%r', errcode, stdout[:70], stderr)
    if errcode: return
    for line in stdout.split('\n'):
      line = line.strip()
      g = re.match(r'addr (([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2})', line)
      if g:
        self.mac = EncodeMAC(g.group(1))
        break

  def _ScanResults(self, errcode, stdout, stderr):
    """Callback for 'iw scan' results."""
    Debug('scan err:%r stdout:%r stderr:%r', errcode, stdout[:70], stderr)
    if errcode: return
    now = time.time()
    mac = rssi = flags = last_seen = None
    def AddEntry():
      if mac:
        is_ours = False  # TODO(apenwarr): calc from received waveguide packets
        self.bss_list[mac] = BSS(is_ours=is_ours, freq=freq, mac=mac,
                                 rssi=rssi, flags=flags, last_seen=last_seen)
        Debug('Added: %r', self.bss_list[mac])
    for line in stdout.split('\n'):
      line = line.strip()
      g = re.match(r'BSS (([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2})', line)
      if g:
        AddEntry()
        mac = rssi = flags = last_seen = None
        mac = EncodeMAC(g.group(1))
        rssi = last_seen = None
        flags = 0
      g = re.match(r'freq: (\d+)', line)
      if g:
        freq = int(g.group(1))
      g = re.match(r'signal: ([-\d.]+) dBm', line)
      if g:
        rssi = float(g.group(1))
      g = re.match(r'last seen: (\d+) ms ago', line)
      if g:
        last_seen = now - float(g.group(1))/1000
    AddEntry()

  def _SurveyResults(self, errcode, stdout, stderr):
    """Callback for 'iw survey dump' results."""
    Debug('survey err:%r stdout:%r stderr:%r', errcode, stdout[:70], stderr)
    if errcode: return
    freq = None
    noise = active_ms = busy_ms = rx_ms = tx_ms = 0
    def AddEntry():
      if freq:
        real_busy_ms = busy_ms - rx_ms - tx_ms
        if real_busy_ms < 0: real_busy_ms = 0
        self.channel_survey_list[freq] = Channel(freq=freq,
                                                 noise_dbm=noise,
                                                 observed_ms=active_ms,
                                                 busy_ms=real_busy_ms)
        Debug('Added: %r', self.channel_survey_list[freq])
    for line in stdout.split('\n'):
      line = line.strip()
      g = re.match(r'Survey data from', line)
      if g:
        AddEntry()
        freq = None
        noise = active_ms = busy_ms = rx_ms = tx_ms = 0
      g = re.match(r'frequency:\s+(\d+) MHz', line)
      if g:
        freq = int(g.group(1))
      g = re.match(r'noise:\s+([-.\d]+) dBm', line)
      if g:
        noise = float(g.group(1))
      g = re.match(r'channel active time:\s+([-\d.]+) ms', line)
      if g:
        active_ms = float(g.group(1))
      g = re.match(r'channel busy time:\s+([-\d.]+) ms', line)
      if g:
        busy_ms = float(g.group(1))
      g = re.match(r'channel receive time:\s+([-\d.]+) ms', line)
      if g:
        rx_ms = float(g.group(1))
      g = re.match(r'channel transmit time:\s+([-\d.]+) ms', line)
      if g:
        tx_ms = float(g.group(1))
    AddEntry()

  def _AssocResults(self, errcode, stdout, stderr):
    """Callback for 'iw station dump' results."""
    Debug('assoc err:%r stdout:%r stderr:%r', errcode, stdout[:70], stderr)
    if errcode: return
    now = time.time()
    self.assoc_list = {}
    mac = None
    rssi = 0
    last_seen = now
    def AddEntry():
      if mac:
        self.assoc_list[mac] = Assoc(mac=mac, rssi=rssi, last_seen=last_seen)
        Debug('Added: %r', self.assoc_list[mac])
    for line in stdout.split('\n'):
      line = line.strip()
      g = re.match(r'Station (([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2})', line)
      if g:
        AddEntry()
        mac = EncodeMAC(g.group(1))
        rssi = 0
        last_seen = now
      g = re.match(r'inactive time:\s+([-.\d]+) ms', line)
      if g:
        last_seen = now - float(g.group(1)) / 1000
      g = re.match(r'signal:\s+([-.\d]+) .*dBm', line)
      if g:
        rssi = float(g.group(1))
    AddEntry()


def CreateManagers(managers, high_power):
  """Create WlanManager() objects, one per wifi PHY."""

  def ParseDevList(errcode, stdout, stderr):
    """Callback for 'iw dev' results."""
    if errcode:
      raise Exception('failed (%d) getting wifi dev list: %r'
                      % (errcode, stderr))
    phy = dev = devtype = None
    phy_devs = {}

    def AddEntry():
      if phy and dev and devtype == 'AP':
        phy_devs[phy] = dev

    for line in stdout.split('\n'):
      line = line.strip()
      g = re.match(r'phy#(\d+)', line)
      if g:
        AddEntry()
        phy = 'phy%s' % g.group(1)
        dev = devtype = None
      g = re.match(r'Interface ([a-zA-Z0-9.]+)', line)
      if g:
        dev = g.group(1)
      g = re.match(r'type (\w+)', line)
      if g:
        devtype = g.group(1)
    AddEntry()
    for phy, dev in phy_devs.iteritems():
      Debug('Creating wlan manager for (%r, %r)', phy, dev)
      managers.append(WlanManager(phy, dev, high_power=high_power))
  RunProc(callback=ParseDevList, args=['iw', 'dev'])


def main():
  o = options.Options(optspec)
  opt, unused_flags, unused_extra = o.parse(sys.argv[1:])

  if opt.debug:
    global debug_level
    debug_level = opt.debug

  # TODO(apenwarr): add/remove managers as devices come and go?
  #   ...on our current system they generally don't, so maybe not important.
  managers = []
  if opt.fake:
    managers.append(WlanManager(phyname='phy0', vdevname='wlan0',
                                high_power=opt.high_power))
  else:
    CreateManagers(managers, high_power=opt.high_power)
  if not managers:
    raise Exception('no wifi devices found')

  last_sent = 0
  while 1:
    rfds = []
    for m in managers:
      rfds.extend(m.GetReadFds())
    r, _, _ = select.select(rfds, [], [], 1.0)
    for i in r:
      for m in managers:
        if i in m.GetReadFds():
          m.ReadReady()
    now = time.time()
    # TODO(apenwarr): how to choose a good transmit frequency?
    #   Also, consider sending out an update (almost) immediately when a new
    #   node joins, so it can learn about the other nodes as quickly as
    #   possible.
    for m in managers:
      m.DoScans(initial_scans=opt.initial_scans,
                scan_interval=opt.scan_interval)
    if opt.tx_interval and now - last_sent > opt.tx_interval:
      last_sent = now
      for m in managers:
        m.SendUpdate()


if __name__ == '__main__':
  main()
