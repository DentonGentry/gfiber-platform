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
#
# pylint:disable=invalid-name

"""Wifi channel selection and roaming daemon."""

from collections import namedtuple
import errno
import hmac
import os
import os.path
import random
import re
import select
import socket
import struct
import subprocess
import sys
import time
import zlib
import helpers
import options


try:
  import monotime  # pylint: disable=unused-import,g-import-not-at-top
except ImportError:
  pass
try:
  _gettime = time.monotonic
except AttributeError:
  _gettime = time.time


optspec = """
waveguide [options...]
--
high-power        This high-powered AP takes priority over low-powered ones
fake=             Create a fake instance with the given MAC address
initial-scans=    Number of immediate full channel scans at startup [0]
scan-interval=    Seconds between full channel scan cycles (0 to disable) [0]
tx-interval=      Seconds between state transmits (0 to disable) [15]
D,debug           Increase (non-anonymized!) debug output level
no-anonymize      Don't anonymize MAC addresses in logs
status-dir=       Directory to store status information [/tmp/waveguide]
watch-pid=        Shut down if the given process pid disappears
auto-disable-threshold=  Shut down if >= RSSI received from other AP [-30]
localhost         Reject packets not from local IP address (for testing)
"""

PROTO_MAGIC = 'wave'
PROTO_VERSION = 1
opt = None

# TODO(apenwarr): not sure what's the right multicast address to use.
# MCAST_ADDRESS = '224.0.0.2'  # "all routers" address
MCAST_ADDRESS = '239.255.0.1'  # "administratively scoped" RFC2365 subnet
MCAST_PORT = 4442


_gettime_rand = random.randint(0, 1000000)


def gettime():
  # using gettime_rand means two local instances will have desynced
  # local timers, which will show problems better in unit tests.  The
  # monotonic timestamp should never leak out of a given instance.
  return _gettime() + _gettime_rand


def Log(s, *args):
  if args:
    print s % args
  else:
    print s
  sys.stdout.flush()


def Debug(s, *args):
  if opt.debug >= 1:
    Log(s, *args)


def Debug2(s, *args):
  if opt.debug >= 2:
    Log(s, *args)


freq_to_chan = {}     # a mapping from wifi frequencies (MHz) to channel no.
chan_to_freq = {}     # a mapping from channel no. to wifi frequency (MHz)


class DecodeError(Exception):
  pass


# pylint: disable=invalid-name
class ApFlags(object):
  Can2G = 0x01          # device supports 2.4 GHz band
  Can5G = 0x02          # device supports 5 GHz band
  Can_Mask = 0x0f       # mask of all bits referring to band capability
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


def EncodeIP(ip):
  return socket.inet_aton(ip)


def DecodeIP(ipbin):
  return socket.inet_ntoa(ipbin)


SOFT = 'AEIOUY' 'V'
HARD = 'BCDFGHJKLMNPQRSTVWXYZ' 'AEIOU'


def Trigraph(num):
  """Given a value from 0..4095, encode it as a cons+vowel+cons sequence."""
  ns = len(SOFT)
  nh = len(HARD)
  assert nh * ns * nh >= 4096
  c3 = num % nh
  c2 = (num / nh) % ns
  c1 = num / nh / ns
  return HARD[c1] + SOFT[c2] + HARD[c3]


def WordFromBinary(s):
  """Encode a binary blob into a string of pronounceable syllables."""
  out = []
  while s:
    part = s[:3]
    s = s[3:]
    while len(part) < 4:
      part = '\0' + part
    bits = struct.unpack('!I', part)[0]
    out += [(bits >> 12) & 0xfff,
            (bits >> 0)  & 0xfff]
  return ''.join(Trigraph(i) for i in out)


def DecodeMAC(macbin):
  """Turn the given binary MAC address into a printable string."""
  assert len(macbin) == 6
  return ':'.join(['%02x' % ord(i) for i in macbin])


# Note(apenwarr): There are a few ways to do this.  I elected to go with
# short human-usable strings (allowing for the small possibility of
# collisions) since the log messages will probably be "mostly" used by
# humans.
#
# An alternative would be to use "format preserving encryption" (basically
# a secure 1:1 mapping of unencrypted to anonymized, in the same number of
# bits) and then produce longer "words" with no possibility of collision.
# But with our current WordFromBinary() implementation, that would be
# 12 characters long, which is kind of inconvenient and we probably don't
# need that level of care.  Inside waveguide we use the real MAC addresses
# so collisions won't cause a real problem.
#
# TODO(apenwarr): consider not anonymizing the OUI.
#   That way we could see any behaviour differences between vendors.
#   Sadly, that might make it too easy to brute force a MAC address back out;
#   the remaining 3 bytes have too little entropy.
#
def AnonymizeMAC(consensus_key, macbin):
  """Anonymize a binary MAC address using the given key."""
  assert len(macbin) == 6
  if consensus_key and opt.anonymize:
    return WordFromBinary(hmac.new(consensus_key, macbin).digest())[:6]
  else:
    return DecodeMAC(macbin)


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
    if hasattr(socket, 'SO_REUSEPORT'):  # needed for MacOS
      try:
        self.rsock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
      except socket.error, e:
        if e.errno == errno.ENOPROTOOPT:
          # some kernels don't support this even if python does
          pass
        else:
          raise
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
def RunProc(callback, args, *xargs, **kwargs):
  p = subprocess.Popen(args, *xargs,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE,
                       **kwargs)
  stdout, stderr = p.communicate()
  errcode = p.wait()
  callback(errcode, stdout, stderr)


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
    self.did_initial_scan = False
    self.next_scan_time = None
    self.scan_idx = -1
    self.disabled_filename = os.path.join(opt.status_dir,
                                          '%s.disabled' % vdevname)
    self.auto_disabled = None
    helpers.Unlink(self.disabled_filename)

  def AnonymizeMAC(self, mac):
    return AnonymizeMAC(self.consensus_key, mac)

  def _LogPrefix(self):
    return '%s(%s): ' % (self.vdevname, self.AnonymizeMAC(self.mac))

  def Log(self, s, *args):
    Log(self._LogPrefix() + s, *args)

  def Debug(self, s, *args):
    Debug(self._LogPrefix() + s, *args)

  def Debug2(self, s, *args):
    Debug2(self._LogPrefix() + s, *args)

  # TODO(apenwarr): when we have async subprocs, add those here
  def GetReadFds(self):
    return [self.mcast.rsock]

  def NextTimeout(self):
    return self.next_scan_time

  def ReadReady(self):
    """Call this when select.select() returns true on GetReadFds()."""
    data, hostport = self.mcast.Recv()
    if opt.localhost and hostport[0] != self.mcast.wsock.getsockname()[0]:
      self.Debug('ignored packet not from localhost: %r', hostport)
      return 0
    try:
      p = DecodePacket(data)
    except DecodeError as e:
      self.Debug('recv: from %r: %s', hostport, e)
      return 0
    else:
      self.Debug('recv: from %r uptime=%d key=%r',
                 hostport, p.me.uptime_ms, p.me.consensus_key[:4])
    # the waveguide that has been running the longest gets to win the contest
    # for what anonymization key to use.  This prevents disruption in case
    # devices come and go.
    # TODO(apenwarr): make sure this doesn't accidentally undo key rotations.
    #   ...once we even do key rotations.
    consensus_start = gettime() - p.me.uptime_ms/1000.0
    if (consensus_start < self.consensus_start and
        self.consensus_key != p.me.consensus_key):
      self.consensus_key = p.me.consensus_key
      self.consensus_start = consensus_start
      self.Log('new key: phy=%r vdev=%r mac=%r',
               self.phyname,
               self.vdevname,
               DecodeMAC(self.mac))
    if p.me.mac == self.mac:
      self.Debug('ignoring packet from self')
      return 0
    if p.me.consensus_key != self.consensus_key:
      self.Debug('ignoring peer due to key mismatch')
      return 0
    if p.me.mac not in self.peer_list:
      self.Log('added a peer: %s', self.AnonymizeMAC(p.me.mac))
    self.peer_list[p.me.mac] = p
    self.MaybeAutoDisable()
    seen_bss_peers = [bss for bss in p.seen_bss
                      if bss.mac in self.peer_list]
    Log('%s: %s: APs=%-4d peer-APs=%s stations=%s',
        self.vdevname,
        self.AnonymizeMAC(p.me.mac),
        len(p.seen_bss),
        ','.join('%s(%d)' % (self.AnonymizeMAC(i.mac), i.rssi)
                 for i in sorted(seen_bss_peers, key=lambda i: -i.rssi)),
        ','.join('%s(%d)' % (self.AnonymizeMAC(i.mac), i.rssi)
                 for i in sorted(p.assoc, key=lambda i: -i.rssi)))
    return 1

  def SendUpdate(self):
    """Constructs and sends a waveguide packet on the multicast interface."""
    me = Me(now=time.time(),
            uptime_ms=(gettime() - self.starttime) * 1000,
            consensus_key=self.consensus_key,
            mac=self.mac,
            flags=self.flags)
    seen_bss_list = self.bss_list.values()
    channel_survey_list = self.channel_survey_list.values()
    assoc_list = self.assoc_list.values()
    arp_list = self.arp_list.values()
    p = EncodePacket(me=me,
                     seen_bss_list=seen_bss_list,
                     channel_survey_list=channel_survey_list,
                     assoc_list=assoc_list,
                     arp_list=arp_list)
    self.Debug2('sending: %r',
                (me, seen_bss_list, channel_survey_list, assoc_list, arp_list))
    self.Debug('sent %s: %r bytes', self.vdevname, self.mcast.Send(p))

  def DoScans(self):
    """Calls programs and reads files to obtain the current wifi status."""
    now = gettime()
    if not self.did_initial_scan:
      Log('startup on %s (initial_scans=%d).',
          self.vdevname, opt.initial_scans)
      self._ReadArpTable()
      RunProc(callback=self._PhyResults,
              args=['iw', 'phy', self.phyname, 'info'])
      RunProc(callback=self._DevResults,
              args=['iw', 'dev', self.vdevname, 'info'])
      # channel scan more than once in case we miss hearing a beacon
      for _ in range(opt.initial_scans):
        RunProc(callback=self._ScanResults,
                args=['iw', 'dev', self.vdevname, 'scan',
                      'ap-force',
                      'passive'])
      self.next_scan_time = now
      self.did_initial_scan = True
    elif not self.allowed_freqs:
      self.Log('%s: no allowed frequencies.', self.vdevname)
    elif self.next_scan_time and now > self.next_scan_time:
      self.scan_idx = (self.scan_idx + 1) % len(self.allowed_freqs)
      scan_freq = list(sorted(self.allowed_freqs))[self.scan_idx]
      self.Log('scanning %d MHz (%d/%d)',
               scan_freq, self.scan_idx + 1, len(self.allowed_freqs))
      RunProc(callback=self._ScanResults,
              args=['iw', 'dev', self.vdevname, 'scan',
                    'freq', str(scan_freq),
                    'ap-force',
                    'passive'])
      chan_interval = opt.scan_interval / len(self.allowed_freqs)
      # Randomly fiddle with the timing to avoid permanent alignment with
      # other nodes also doing scans.  If we're perfectly aligned with
      # another node, they might never see us in their periodic scan.
      chan_interval = random.uniform(chan_interval * 0.5,
                                     chan_interval * 1.5)
      self.next_scan_time += chan_interval
      if not self.scan_idx:
        WriteEventFile('%s.scanned' % self.vdevname)
    if not opt.scan_interval:
      self.next_scan_time = None

  def UpdateStationInfo(self):
    # These change in the background, not as the result of a scan
    RunProc(callback=self._SurveyResults,
            args=['iw', 'dev', self.vdevname, 'survey', 'dump'])
    RunProc(callback=self._AssocResults,
            args=['iw', 'dev', self.vdevname, 'station', 'dump'])

  def ShouldAutoDisable(self):
    """Returns MAC address of high-powered peer if we should auto disable."""
    if self.flags & ApFlags.HighPower:
      self.Debug('high-powered AP: never auto-disable')
      return None
    for peer in sorted(self.peer_list.values(),
                       key=lambda p: p.me.mac):
      self.Debug('considering auto disable: peer=%s',
                 self.AnonymizeMAC(peer.me.mac))
      if peer.me.mac not in self.bss_list:
        self.Debug('--> peer no match')
      else:
        bss = self.bss_list[peer.me.mac]
        peer_age_secs = time.time() - peer.me.now
        scan_age_secs = time.time() - bss.last_seen
        peer_power = peer.me.flags & ApFlags.HighPower
        # TODO(apenwarr): overlap should consider only our *current* band.
        #  This isn't too important right away since high powered APs
        #  are on all bands simultaneously anyway.
        overlap = self.flags & peer.me.flags & ApFlags.Can_Mask
        self.Debug('--> peer matches! p_age=%.3f s_age=%.3f power=0x%x '
                   'band_overlap=0x%02x',
                   peer_age_secs, scan_age_secs, peer_power, overlap)
        if bss.rssi <= opt.auto_disable_threshold:
          self.Debug('--> peer is far away, keep going.')
        elif not peer_power:
          self.Debug('--> peer is not high-power, keep going.')
        elif not overlap:
          self.Debug('--> peer does not overlap our band, keep going.')
        elif (peer_age_secs > opt.tx_interval * 4
              or (opt.scan_interval and
                  scan_age_secs > opt.scan_interval * 4)):
          self.Debug('--> peer is too old, keep going.')
        else:
          self.Debug('--> peer overwhelms us, shut down.')
          return peer.me.mac
    return None

  def MaybeAutoDisable(self):
    """Writes/removes the auto-disable file based on ShouldAutoDisable()."""
    ad = self.ShouldAutoDisable()
    if ad and self.auto_disabled != ad:
      self.Log('auto-disabling because of %s', self.AnonymizeMAC(ad))
      helpers.WriteFileAtomic(self.disabled_filename, DecodeMAC(ad))
    elif self.auto_disabled and not ad:
      self.Log('auto-enabling because %s disappeared',
               self.AnonymizeMAC(self.auto_disabled))
      helpers.Unlink(self.disabled_filename)
    self.auto_disabled = ad

  def _ReadArpTable(self):
    """Reads the kernel's ARP entries."""
    now = time.time()
    try:
      f = open('/proc/net/arp', 'r', 64*1024)
    except IOError, e:
      self.Log('arp table missing: %s', e)
      return
    data = f.read(64*1024)
    lines = data.split('\n')[1:]  # skip header line
    for line in lines:
      g = re.match(r'(\d+\.\d+\.\d+\.\d+)\s+.*\s+'
                   r'(([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2})',
                   line)
      if g:
        ip = EncodeIP(g.group(1))
        mac = EncodeMAC(g.group(2))
        self.arp_list[mac] = ARP(ip=ip, mac=mac, last_seen=now)
        self.Debug('arp %r', self.arp_list[mac])

  def _PhyResults(self, errcode, stdout, stderr):
    """Callback for 'iw phy xxx info' results."""
    self.Debug('phy %r err:%r stdout:%r stderr:%r',
               self.phyname, errcode, stdout[:70], stderr)
    if errcode: return
    for line in stdout.split('\n'):
      line = line.strip()
      g = re.match(r'\* (\d+) MHz \[(\d+)\] \((.*)\)', line)
      if g:
        freq = int(g.group(1))
        chan = int(g.group(2))
        disabled = (g.group(3) == 'disabled')
        self.Debug('phy freq=%d chan=%d disabled=%d', freq, chan, disabled)
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
    self.Debug('dev err:%r stdout:%r stderr:%r', errcode, stdout[:70], stderr)
    if errcode: return
    for line in stdout.split('\n'):
      line = line.strip()
      g = re.match(r'addr (([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2})', line)
      if g:
        self.mac = EncodeMAC(g.group(1))
        break

  def _ScanResults(self, errcode, stdout, stderr):
    """Callback for 'iw scan' results."""
    self.Debug('scan err:%r stdout:%r stderr:%r', errcode, stdout[:70], stderr)
    if errcode: return
    now = time.time()
    mac = rssi = flags = last_seen = None
    def AddEntry():
      if mac:
        is_ours = False  # TODO(apenwarr): calc from received waveguide packets
        bss = BSS(is_ours=is_ours, freq=freq, mac=mac,
                  rssi=rssi, flags=flags, last_seen=last_seen)
        if mac not in self.bss_list:
          self.Debug('Added: %r', bss)
        self.bss_list[mac] = bss
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
    self.MaybeAutoDisable()

  def _SurveyResults(self, errcode, stdout, stderr):
    """Callback for 'iw survey dump' results."""
    self.Debug('survey err:%r stdout:%r stderr:%r',
               errcode, stdout[:70], stderr)
    if errcode: return
    freq = None
    noise = active_ms = busy_ms = rx_ms = tx_ms = 0
    def AddEntry():
      if freq:
        real_busy_ms = busy_ms - rx_ms - tx_ms
        if real_busy_ms < 0: real_busy_ms = 0
        ch = Channel(freq=freq, noise_dbm=noise, observed_ms=active_ms,
                     busy_ms=real_busy_ms)
        if freq not in self.channel_survey_list:
          self.Debug('Added: %r', ch)
        self.channel_survey_list[freq] = ch
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
    self.Debug('assoc err:%r stdout:%r stderr:%r',
               errcode, stdout[:70], stderr)
    if errcode: return
    now = time.time()
    self.assoc_list = {}
    mac = None
    rssi = 0
    last_seen = now
    def AddEntry():
      if mac:
        a = Assoc(mac=mac, rssi=rssi, last_seen=last_seen)
        if mac not in self.assoc_list:
          self.Debug('Added: %r', a)
        self.assoc_list[mac] = a
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
      if phy and dev:
        if devtype == 'AP':
          # We only want one vdev per PHY.  Special-purpose vdevs are
          # probably the same name with an extension, so use the shortest one.
          if phy not in phy_devs or len(phy_devs[phy]) > len(dev):
            phy_devs[phy] = dev
        else:
          Debug('Skipping dev %r because type %r != AP', dev, devtype)

    for line in stdout.split('\n'):
      line = line.strip()
      g = re.match(r'phy#(\d+)', line)
      if g:
        # A new phy
        AddEntry()
        phy = 'phy%s' % g.group(1)
        dev = devtype = None
      g = re.match(r'Interface ([_a-zA-Z0-9.]+)', line)
      if g:
        # A new interface inside this phy
        AddEntry()
        dev = g.group(1)
        devtype = None
      g = re.match(r'type (\w+)', line)
      if g:
        devtype = g.group(1)
    AddEntry()
    existing_devs = dict((m.vdevname, m) for m in managers)
    for dev, m in existing_devs.iteritems():
      if dev not in phy_devs.values():
        Log('Forgetting interface %r.', dev)
        managers.remove(m)
    for phy, dev in phy_devs.iteritems():
      if dev not in existing_devs:
        Debug('Creating wlan manager for (%r, %r)', phy, dev)
        managers.append(WlanManager(phy, dev, high_power=high_power))

  RunProc(callback=ParseDevList, args=['iw', 'dev'])


def WriteEventFile(name):
  """Create a file in opt.status_dir if it does not already exist.

  This is useful for reporting that an event has occurred.  We use O_EXCL
  to prevent any filesystem churn at all if the file still exists, so it's
  very fast.  A program watching for the event can unlink the file, then
  wait for it to be re-created as an indication that the event has
  occurred.

  Args:
    name: the name of the file to create.
  """
  fullname = os.path.join(opt.status_dir, name)
  try:
    fd = os.open(fullname, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0666)
  except OSError, e:
    if e.errno != errno.EEXIST:
      raise
  else:
    os.close(fd)


def main():
  global opt
  o = options.Options(optspec)
  opt, flags, unused_extra = o.parse(sys.argv[1:])
  opt.scan_interval = float(opt.scan_interval)
  opt.tx_interval = float(opt.tx_interval)
  if opt.watch_pid and opt.watch_pid <= 1:
    o.fatal('--watch-pid must be empty or > 1')

  try:
    os.makedirs(opt.status_dir)
  except OSError, e:
    if e.errno != errno.EEXIST:
      raise

  # TODO(apenwarr): add/remove managers as devices come and go?
  #   ...on our current system they generally don't, so maybe not important.
  managers = []
  if opt.fake:
    for k, fakemac in flags:
      if k == '--fake':
        # Name the fake phy/mac devices after the fake MAC address, but
        # skip the first bit of the MAC to make it less obnoxiously long.
        # The remainder should generally be enough to uniquely identify
        # them.
        wlm = WlanManager(phyname='phy-%s' % fakemac[12:],
                          vdevname='wlan-%s' % fakemac[12:],
                          high_power=opt.high_power)
        wlm.mac = EncodeMAC(fakemac)
        managers.append(wlm)
  else:
    CreateManagers(managers, high_power=opt.high_power)
  if not managers:
    raise Exception('no wifi AP-mode devices found.  Try --fake.')

  last_sent = 0
  while 1:
    if opt.watch_pid > 1:
      try:
        os.kill(opt.watch_pid, 0)
      except OSError, e:
        if e.errno == errno.ESRCH:
          Log('watch-pid %r died; shutting down', opt.watch_pid)
          break
        else:
          raise
      # else process is still alive, continue
    rfds = []
    for m in managers:
      rfds.extend(m.GetReadFds())
    timeouts = [m.NextTimeout() for m in managers]
    if opt.tx_interval:
      timeouts.append(last_sent + opt.tx_interval)
    timeout = min(filter(None, timeouts))
    Debug2('now=%f timeout=%f  timeouts=%r', gettime(), timeout, timeouts)
    if timeout: timeout -= gettime()
    if timeout < 0: timeout = 0
    if timeout is None and opt.watch_pid: timeout = 5.0
    gotpackets = 0
    for _ in xrange(64):
      r, _, _ = select.select(rfds, [], [], timeout)
      if not r:
        WriteEventFile('nopackets')
        break
      timeout = 0
      for i in r:
        for m in managers:
          if i in m.GetReadFds():
            gotpackets += m.ReadReady()
    if gotpackets:
      WriteEventFile('gotpacket')
    now = gettime()
    # TODO(apenwarr): how often should we really transmit?
    #   Also, consider sending out an update (almost) immediately when a new
    #   node joins, so it can learn about the other nodes as quickly as
    #   possible.  But if we do that, we need to rate limit it somehow.
    for m in managers:
      m.DoScans()
    if opt.tx_interval and now - last_sent > opt.tx_interval:
      last_sent = now
      if not opt.fake:
        CreateManagers(managers, high_power=opt.high_power)
      for m in managers:
        m.UpdateStationInfo()
      for m in managers:
        m.SendUpdate()
        WriteEventFile('sentpacket')
    if not r:
      WriteEventFile('ready')


if __name__ == '__main__':
  main()
