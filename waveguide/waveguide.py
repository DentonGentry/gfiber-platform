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

import errno
import gc
import json
import os
import os.path
import random
import re
import select
import socket
import string
import struct
import subprocess
import sys
import time
import autochannel
import clientinfo
import helpers
import log
import options
import wgdata

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
initial-scans=    Number of immediate full channel scans at startup [1]
scan-interval=    Seconds between full channel scan cycles (0 to disable) [0]
tx-interval=      Seconds between state transmits (0 to disable) [15]
autochan-interval= Seconds between autochannel decisions (0 to disable) [300]
print-interval=   Seconds between state printouts to log (0 to disable) [16]
D,debug           Increase (non-anonymized!) debug output level
no-anonymize      Don't anonymize MAC addresses in logs
status-dir=       Directory to store status information [/tmp/waveguide]
watch-pid=        Shut down if the given process pid disappears
auto-disable-threshold=  Shut down if >= RSSI received from other AP [-30]
localhost         Reject packets not from local IP address (for testing)
"""

opt = None

# TODO(apenwarr): not sure what's the right multicast address to use.
# MCAST_ADDRESS = '224.0.0.2'  # "all routers" address
MCAST_ADDRESS = '239.255.0.1'  # "administratively scoped" RFC2365 subnet
MCAST_PORT = 4442
AP_LIST_FILE = ['']
PEER_AP_LIST_FILE = ['']
WIFIBLASTERDIR = '/tmp/wifi/wifiblaster'

_gettime_rand = random.randint(0, 1000000)


def gettime():
  # using gettime_rand means two local instances will have desynced
  # local timers, which will show problems better in unit tests.  The
  # monotonic timestamp should never leak out of a given instance.
  return _gettime() + _gettime_rand

# Do not assign consensus_key directly; call UpdateConsensus() instead.
consensus_key = None
consensus_start = None


def UpdateConsensus(new_uptime_ms, new_consensus_key):
  """Update the consensus key based on received multicast packets."""
  global consensus_key, consensus_start
  new_consensus_start = gettime() - new_uptime_ms / 1000.0
  if (consensus_start is None or (new_consensus_start < consensus_start and
                                  consensus_key != new_consensus_key)):
    consensus_key = new_consensus_key
    consensus_start = new_consensus_start

    key_file = os.path.join(opt.status_dir, 'consensus_key')
    helpers.WriteFileAtomic(key_file, consensus_key)
    return True

  return False


freq_to_chan = {}  # a mapping from wifi frequencies (MHz) to channel no.
chan_to_freq = {}  # a mapping from channel no. to wifi frequency (MHz)


def TouchAliveFile():
  alive_file = os.path.join(opt.status_dir, 'alive')
  with open(alive_file, 'a'):
    os.utime(alive_file, None)


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
      except socket.error as e:
        if e.errno == errno.ENOPROTOOPT:
          # some kernels don't support this even if python does
          pass
        else:
          raise
    self.rsock.bind(('', self.port))
    mreq = struct.pack('4sl', socket.inet_pton(socket.AF_INET, self.host),
                       socket.INADDR_ANY)
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
                       stderr=subprocess.PIPE, **kwargs)
  stdout, stderr = p.communicate()
  errcode = p.wait()
  callback(errcode, stdout, stderr)


def WriteFileIfMissing(filename, content):
  if not os.path.exists(filename):
    helpers.WriteFileAtomic(filename, content)


class WlanManager(object):
  """A class representing one wifi interface on the local host."""

  def __init__(self, phyname, vdevname, high_power):
    self.phyname = phyname
    self.vdevname = vdevname
    self.mac = '\0\0\0\0\0\0'
    self.ssid = ''
    self.flags = 0
    self.allowed_freqs = set()
    if high_power:
      self.flags |= wgdata.ApFlags.HighPower
    self.bss_list = {}
    self.channel_survey_list = {}
    self.assoc_list = {}
    self.arp_list = {}
    self.peer_list = {}
    self.starttime = gettime()
    self.mcast = MulticastSocket((MCAST_ADDRESS, MCAST_PORT))
    self.did_initial_scan = False
    self.next_scan_time = None
    self.scan_idx = -1
    self.last_survey = {}
    self.self_signals = {}
    self.ap_signals = {}
    self.auto_disabled = None
    self.autochan_2g = self.autochan_5g = self.autochan_free = 0
    helpers.Unlink(self.Filename('disabled'))

  def Filename(self, suffix):
    return os.path.join(opt.status_dir, '%s.%s' % (self.vdevname, suffix))

  def AnonymizeMAC(self, mac):
    return log.AnonymizeMAC(consensus_key, mac)

  def _LogPrefix(self):
    return '%s(%s): ' % (self.vdevname, self.AnonymizeMAC(self.mac))

  def Log(self, s, *args):
    log.Log(self._LogPrefix() + s, *args)

  def Debug(self, s, *args):
    log.Debug(self._LogPrefix() + s, *args)

  def Debug2(self, s, *args):
    log.Debug2(self._LogPrefix() + s, *args)

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
      p = wgdata.DecodePacket(data)
    except wgdata.DecodeError as e:
      self.Debug('recv: from %r: %s', hostport, e)
      return 0
    else:
      self.Debug('recv: from %r uptime=%d key=%r', hostport, p.me.uptime_ms,
                 p.me.consensus_key[:4])
    # the waveguide that has been running the longest gets to win the contest
    # for what anonymization key to use.  This prevents disruption in case
    # devices come and go.
    # TODO(apenwarr): make sure this doesn't accidentally undo key rotations.
    #   ...once we even do key rotations.
    if UpdateConsensus(p.me.uptime_ms, p.me.consensus_key):
      self.Log('new key: phy=%r vdev=%r mac=%r', self.phyname, self.vdevname,
               helpers.DecodeMAC(self.mac))
    if p.me.mac == self.mac:
      self.Debug('ignoring packet from self')
      return 0
    if p.me.consensus_key != consensus_key:
      self.Debug('ignoring peer due to key mismatch')
      return 0
    if p.me.mac not in self.peer_list:
      self.Log('added a peer: %s', self.AnonymizeMAC(p.me.mac))
    self.peer_list[p.me.mac] = p
    self.MaybeAutoDisable()
    return 1

  def GetState(self):
    """Return a wgdata.State() for this object."""
    me = wgdata.Me(now=time.time(),
                   uptime_ms=(gettime() - self.starttime) * 1000,
                   consensus_key=consensus_key,
                   mac=self.mac,
                   flags=self.flags)
    seen_bss_list = self.bss_list.values()
    channel_survey_list = self.channel_survey_list.values()
    assoc_list = self.assoc_list.values()
    arp_list = self.arp_list.values()
    return wgdata.State(me=me,
                        seen_bss=seen_bss_list,
                        channel_survey=channel_survey_list,
                        assoc=assoc_list,
                        arp=arp_list)

  def SendUpdate(self):
    """Constructs and sends a waveguide packet on the multicast interface."""
    state = self.GetState()
    p = wgdata.EncodePacket(state)
    self.Debug2('sending: %r', state)
    self.Debug('sent %s: %r bytes', self.vdevname, self.mcast.Send(p))

  def DoScans(self):
    """Calls programs and reads files to obtain the current wifi status."""
    now = gettime()
    if not self.did_initial_scan:
      log.Log('startup on %s (initial_scans=%d).', self.vdevname,
              opt.initial_scans)
      self._ReadArpTable()
      RunProc(callback=self._PhyResults,
              args=['iw', 'phy', self.phyname, 'info'])
      RunProc(callback=self._DevResults,
              args=['iw', 'dev', self.vdevname, 'info'])
      # channel scan more than once in case we miss hearing a beacon
      for _ in range(opt.initial_scans):
        RunProc(
            callback=self._ScanResults,
            args=['iw', 'dev', self.vdevname, 'scan', 'ap-force', 'passive'])
        self.UpdateStationInfo()
      self.next_scan_time = now
      self.did_initial_scan = True
    elif not self.allowed_freqs:
      self.Log('%s: no allowed frequencies.', self.vdevname)
    elif self.next_scan_time and now > self.next_scan_time:
      self.scan_idx = (self.scan_idx + 1) % len(self.allowed_freqs)
      scan_freq = list(sorted(self.allowed_freqs))[self.scan_idx]
      self.Log('scanning %d MHz (%d/%d)', scan_freq, self.scan_idx + 1,
               len(self.allowed_freqs))
      RunProc(callback=self._ScanResults,
              args=['iw', 'dev', self.vdevname, 'scan', 'freq', str(scan_freq),
                    'ap-force', 'passive'])
      chan_interval = opt.scan_interval / len(self.allowed_freqs)
      # Randomly fiddle with the timing to avoid permanent alignment with
      # other nodes also doing scans.  If we're perfectly aligned with
      # another node, they might never see us in their periodic scan.
      chan_interval = random.uniform(chan_interval * 0.5, chan_interval * 1.5)
      self.next_scan_time += chan_interval
      if not self.scan_idx:
        log.WriteEventFile('%s.scanned' % self.vdevname)
    if not opt.scan_interval:
      self.next_scan_time = None

  def UpdateStationInfo(self):
    # These change in the background, not as the result of a scan
    RunProc(callback=self._SurveyResults,
            args=['iw', 'dev', self.vdevname, 'survey', 'dump'])
    RunProc(callback=self._AssocResults,
            args=['iw', 'dev', self.vdevname, 'station', 'dump'])

  def WriteApListFile(self):
    """Write out a file of known APs."""
    ap_list = []
    for peer in self.peer_list.itervalues():
      if peer.me.mac not in self.bss_list:
        continue
      bssid = helpers.DecodeMAC(peer.me.mac)
      b = self.bss_list[peer.me.mac]
      txt = 'bssid:%s|freq:%d|cap:0x%x|phy:%d|reg:%s|rssi:%s|last_seen:%d'
      s = txt % (bssid, b.freq, b.cap, b.phy, b.reg, b.rssi, b.last_seen)
      ap_list.append(s)
    content = '\n'.join(ap_list)
    if AP_LIST_FILE[0]:
      filename = AP_LIST_FILE[0] + '.' + self.vdevname
      helpers.WriteFileAtomic(filename, content)

  def WritePeerApInfoFile(self, peer_data):
    """Writes files containing signal strength information.

    The files contain other access points' data about their peers;
    these are named PeerAPs.{interface}.

    Args:
      peer_data: address about each MAC.
    """
    peer_ap_list = []
    for peer_mac_addr in peer_data:
      for b in peer_data[peer_mac_addr]:
        peer_ap = helpers.DecodeMAC(b.mac)
        txt = ('peer:%s|bssid:%s|freq:%d|cap:0x%x|phy:%d|reg:%s|rssi:%s'
               '|last_seen:%d')
        if all(c in string.printable for c in b.reg):
          reg = b.reg
        else:
          reg = ''
        s = txt % (peer_mac_addr, peer_ap, b.freq, b.cap, b.phy, reg, b.rssi,
                   b.last_seen)
        peer_ap_list.append(s)
    content = '\n'.join(peer_ap_list)
    if PEER_AP_LIST_FILE[0]:
      filename = PEER_AP_LIST_FILE[0] + '.' + self.vdevname
      helpers.WriteFileAtomic(filename, content)

  def WriteJsonSignals(self):
    """Writes set of files containing JSON formatted signal data.

    The files are about the signal strength of other access points
    as seen by this access point (ap_signals) and the signal strength
    of this access point as seen by other access points (self_signals).
    These two files are in the signals_json directory.
    """
    signal_dir = os.path.join(opt.status_dir, 'signals_json')
    self_signals_file = os.path.join(signal_dir, 'self_signals')
    ap_signals_file = os.path.join(signal_dir, 'ap_signals')
    try:
      os.makedirs(signal_dir)
    except OSError as e:
      if e.errno != errno.EEXIST:
        raise
    if self.self_signals:
      helpers.WriteFileAtomic(self_signals_file, json.dumps(self.self_signals))
    if self.ap_signals:
      helpers.WriteFileAtomic(ap_signals_file, json.dumps(self.ap_signals))

  def ShouldAutoDisable(self):
    """Returns MAC address of high-powered peer if we should auto disable."""
    if self.flags & wgdata.ApFlags.HighPower:
      self.Debug('high-powered AP: never auto-disable')
      return None
    for peer in sorted(self.peer_list.values(), key=lambda p: p.me.mac):
      self.Debug('considering auto disable: peer=%s',
                 self.AnonymizeMAC(peer.me.mac))
      if peer.me.mac not in self.bss_list:
        self.Debug('--> peer no match')
      else:
        bss = self.bss_list[peer.me.mac]
        peer_age_secs = time.time() - peer.me.now
        scan_age_secs = time.time() - bss.last_seen
        peer_power = peer.me.flags & wgdata.ApFlags.HighPower
        # TODO(apenwarr): overlap should consider only our *current* band.
        #  This isn't too important right away since high powered APs
        #  are on all bands simultaneously anyway.
        overlap = self.flags & peer.me.flags & wgdata.ApFlags.Can_Mask
        self.Debug('--> peer matches! p_age=%.3f s_age=%.3f power=0x%x '
                   'band_overlap=0x%02x', peer_age_secs, scan_age_secs,
                   peer_power, overlap)
        if bss.rssi <= opt.auto_disable_threshold:
          self.Debug('--> peer is far away, keep going.')
        elif not peer_power:
          self.Debug('--> peer is not high-power, keep going.')
        elif not overlap:
          self.Debug('--> peer does not overlap our band, keep going.')
        elif (peer_age_secs > opt.tx_interval * 4 or
              (opt.scan_interval and scan_age_secs > opt.scan_interval * 4)):
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
      helpers.WriteFileAtomic(self.Filename('disabled'), helpers.DecodeMAC(ad))
    elif self.auto_disabled and not ad:
      self.Log('auto-enabling because %s disappeared',
               self.AnonymizeMAC(self.auto_disabled))
      helpers.Unlink(self.Filename('disabled'))
    self.auto_disabled = ad

  def _ChooseChannel(self, state, candidates, hysteresis_freq):
    """Recommend a wifi channel for a particular set of constraints."""
    spreading = helpers.Experiment('WifiPrimarySpreading')
    combos = autochannel.LegalCombos(self.allowed_freqs, candidates)
    use_active_time = helpers.Experiment('WifiUseActiveTime')
    cc = autochannel.SoloChooseChannel(state,
                                       candidates=combos,
                                       use_primary_spreading=spreading,
                                       use_active_time=use_active_time,
                                       hysteresis_freq=hysteresis_freq)
    self.Log('%s', cc)
    return cc.primary_freq

  def ChooseChannel(self):
    """Recommend a wifi channel for this device."""
    freqs = list(sorted(self.allowed_freqs))
    self.Log('Freqs: %s', ' '.join(str(f) for f in freqs))

    apc = ''
    for freq in freqs:
      apc += '%s ' % len([bss for bss in self.bss_list.values()
                          if bss.freq == freq])
    self.Log('APcounts: %s', apc)
    busy = ''

    for freq in freqs:
      cs = self.channel_survey_list.get(freq, None)
      if cs:
        frac = cs.busy_ms * 100 / (cs.observed_ms + 1)
        busy += '%s%d ' % (
            ('*'
             if cs.observed_ms < autochannel.AIRTIME_THRESHOLD_MS else ''), frac
        )
      else:
        busy += '*0 '
    self.Log('Busy%%: %s', busy)

    state = self.GetState()

    candidates_free = []
    if self.flags & wgdata.ApFlags.Can2G:
      if helpers.Experiment('WifiChannelsLimited2G'):
        candidates2g = autochannel.C_24MAIN
      else:
        candidates2g = autochannel.C_24ANY
      candidates_free += candidates2g
      self.autochan_2g = self._ChooseChannel(
          state, candidates2g, self.autochan_2g)
      WriteFileIfMissing(self.Filename('autochan_2g.init'),
                         str(self.autochan_2g))
      helpers.WriteFileAtomic(self.Filename('autochan_2g'),
                              str(self.autochan_2g))
    if self.flags & wgdata.ApFlags.Can5G:
      candidates5g = []
      if helpers.Experiment('WifiLowIsHigh'):
        # WifiLowIsHigh means to treat low-powered channels as part of the
        # high-powered category.  Newer FCC rules allow high power
        # transmission on the previously low-powered channels, but not all
        # devices support it.
        candidates5g += autochannel.C_5LOW + autochannel.C_5HIGH
      elif opt.high_power:
        candidates5g += autochannel.C_5HIGH
      else:
        candidates5g += autochannel.C_5LOW
      if helpers.Experiment('WifiUseDFS'):
        candidates5g += autochannel.C_5DFS
      candidates_free += candidates5g
      self.autochan_5g = self._ChooseChannel(
          state, candidates5g, self.autochan_5g)
      WriteFileIfMissing(self.Filename('autochan_5g.init'),
                         str(self.autochan_5g))
      helpers.WriteFileAtomic(self.Filename('autochan_5g'),
                              str(self.autochan_5g))
    self.autochan_free = self._ChooseChannel(
        state, candidates_free, self.autochan_free)
    WriteFileIfMissing(self.Filename('autochan_free.init'),
                       str(self.autochan_free))
    helpers.WriteFileAtomic(self.Filename('autochan_free'),
                            str(self.autochan_free))
    self.Log('Recommended freqs: %d %d -> %d', self.autochan_2g,
             self.autochan_5g, self.autochan_free)
    log.WriteEventFile('autochan_done')

  def _ReadArpTable(self):
    """Reads the kernel's ARP entries."""
    now = time.time()
    try:
      f = open('/proc/net/arp', 'r', 64 * 1024)
    except IOError as e:
      self.Log('arp table missing: %s', e)
      return
    data = f.read(64 * 1024)
    lines = data.split('\n')[1:]  # skip header line
    for line in lines:
      g = re.match(r'(\d+\.\d+\.\d+\.\d+)\s+.*\s+'
                   r'(([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2})', line)
      if g:
        ip = helpers.EncodeIP(g.group(1))
        mac = helpers.EncodeMAC(g.group(2))
        self.arp_list[mac] = wgdata.ARP(ip=ip, mac=mac, last_seen=now)
        self.Debug('arp %r', self.arp_list[mac])

  def _PhyResults(self, errcode, stdout, stderr):
    """Callback for 'iw phy xxx info' results."""
    self.Debug('phy %r err:%r stdout:%r stderr:%r', self.phyname, errcode,
               stdout[:70], stderr)
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
            self.flags |= wgdata.ApFlags.Can2G
          if freq / 1000 == 5:
            self.flags |= wgdata.ApFlags.Can5G
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
        self.mac = helpers.EncodeMAC(g.group(1))
        continue
      g = re.match(r'ssid (.*)', line)
      if g:
        self.ssid = g.group(1)

  def _ScanResults(self, errcode, stdout, stderr):
    """Callback for 'iw scan' results."""
    self.Debug('scan err:%r stdout:%r stderr:%r', errcode, stdout[:70], stderr)
    if errcode: return
    now = time.time()
    mac = freq = rssi = last_seen = None
    reg = ''
    flags = cap = phy = 0

    def AddEntry():
      if mac:
        is_ours = False  # TODO(apenwarr): calc from received waveguide packets
        bss = wgdata.BSS(is_ours=is_ours,
                         freq=freq,
                         mac=mac,
                         rssi=rssi,
                         flags=flags,
                         last_seen=last_seen,
                         cap=cap,
                         phy=phy,
                         reg=reg)
        if mac not in self.bss_list:
          self.Debug('Added: %r', bss)
        self.bss_list[mac] = bss

    for line in stdout.split('\n'):
      line = line.strip()
      g = re.match(r'BSS (([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2})', line)
      if g:
        AddEntry()
        mac = freq = rssi = last_seen = None
        reg = ''
        flags = cap = phy = 0
        mac = helpers.EncodeMAC(g.group(1))
      g = re.match(r'freq: (\d+)', line)
      if g:
        freq = int(g.group(1))
      g = re.match(r'signal: ([-\d.]+) dBm', line)
      if g:
        rssi = float(g.group(1))
      g = re.match(r'last seen: (\d+) ms ago', line)
      if g:
        last_seen = now - float(g.group(1)) / 1000
      g = re.match(r'capability: .* \((\S+)\)', line)
      if g:
        cap = int(g.group(1), 0)
      g = re.match(r'HT capabilities:', line)
      if g:
        phy = max(phy, 7)  # dot11_phy_type_ht = 7
      g = re.match(r'VHT capabilities:', line)
      if g:
        phy = max(phy, 8)  # dot11_phy_type_vht = 8
      g = re.match(r'Country: (\S\S) ', line)
      if g:
        reg = str(g.group(1))
    AddEntry()
    self.MaybeAutoDisable()
    self.WriteApListFile()

  def _SurveyResults(self, errcode, stdout, stderr):
    """Callback for 'iw survey dump' results."""
    self.Debug('survey err:%r stdout:%r stderr:%r', errcode, stdout[:70],
               stderr)
    if errcode: return
    freq = None
    noise = active_ms = busy_ms = rx_ms = tx_ms = 0

    def AddEntry():
      if freq:
        # TODO(apenwarr): ath9k: rx_ms includes all airtime, not just ours.
        #   tx_ms is only time *we* were transmitting, so it doesn't count
        #   toward the busy level of the channel for decision making
        #   purposes.  We'd also like to forget time spent receiving from
        #   our stations, but rx_ms includes that *plus* all time spent
        #   receiving packets from anyone, unfortunately.  I don't know
        #   the difference between rx_ms and busy_ms, but they seem to differ
        #   only by a small percentage usually.
        # TODO(apenwarr): ath10k: busy_ms is missing entirely.
        #   And it looks like active_ms is filled with what should be
        #   busy_ms, which means we have no idea what fraction of time it
        #   was active.  The code below will treat all channels as 0.
        real_busy_ms = busy_ms - tx_ms
        if real_busy_ms < 0: real_busy_ms = 0
        current = (active_ms, busy_ms, rx_ms, tx_ms)
        if current != self.last_survey.get(freq, None):
          oldch = self.channel_survey_list.get(freq, None)
          # 'iw survey dump' results are single readings, which we want to
          # accumulate over time.
          #
          # TODO(apenwarr): change iw to only clear counters when asked.
          #   Right now it zeroes one channel of data whenever you rescan
          #   that one channel, which leaves us to do this error-prone
          #   accumulation by hand later.
          #
          # The current channel will be active for >100ms, and other channels
          # will be ~50-100ms (because they record only the most recent
          # offchannel event).  So we add a margin of safety, and accumulate
          # for values <250ms, but *replace* for values >250ms.
          if oldch and active_ms < 250:
            old_observed, old_busy = oldch.observed_ms, oldch.busy_ms
          else:
            old_observed, old_busy = 0, 0
          ch = wgdata.Channel(freq=freq,
                              noise_dbm=noise,
                              observed_ms=old_observed + active_ms,
                              busy_ms=old_busy + real_busy_ms)
          if freq not in self.channel_survey_list:
            self.Debug('Added: %r', ch)
          self.channel_survey_list[freq] = ch
          # TODO(apenwarr): persist the survey results across reboots?
          #   The channel usage stats are probably most useful over a long
          #   time period.  On the other hand, if the device reboots, maybe
          #   the environment will be different when it comes back.
          self.last_survey[freq] = current

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
    self.Debug('assoc err:%r stdout:%r stderr:%r', errcode, stdout[:70], stderr)
    if errcode: return
    now = time.time()
    self.assoc_list = {}
    mac = None
    rssi = 0
    last_seen = now
    can5G = None

    def AddEntry():
      if mac:
        a = wgdata.Assoc(mac=mac, rssi=rssi, last_seen=last_seen, can5G=can5G)
        if mac not in self.assoc_list:
          self.Debug('Added: %r', a)
        self.assoc_list[mac] = a

    for line in stdout.split('\n'):
      line = line.strip()
      g = re.match(r'Station (([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2})', line)
      if g:
        AddEntry()
        unencoded_mac = g.group(1)
        mac = helpers.EncodeMAC(unencoded_mac)
        rssi = 0
        last_seen = now
        can5G = self._AssocCan5G(unencoded_mac)
      g = re.match(r'inactive time:\s+([-.\d]+) ms', line)
      if g:
        last_seen = now - float(g.group(1)) / 1000
      g = re.match(r'signal:\s+([-.\d]+) .*dBm', line)
      if g:
        rssi = float(g.group(1))
    AddEntry()

  def _AssocCan5G(self, mac):
    """Check whether a station supports 5GHz.

    Args:
      mac: The (unencoded) MAC address of the station.

    Returns:
      Whether the associated station supports 5GHz.
    """
    # If the station is associated with a 5GHz-only radio, then it supports
    # 5Ghz.
    if not self.flags & wgdata.ApFlags.Can2G:
      return True

    # If the station is associated with the 2.4GHz radio, check to see whether
    # hostapd determined it was 5GHz-capable (i.e. bandsteering failed).  See
    # hostap/src/ap/steering.h for details on the filename format, and /bin/wifi
    # for the location of the file.
    mac = ''.join(mac.split(':'))
    if os.path.exists('/tmp/wifi/steering/2.4/%s.2' % mac):
      return True

    # If the station is associated with the 2.4GHz radio and bandsteering wasn't
    # attempted, the station only supports 2.4GHz.
    return False


def CreateManagers(managers, high_power):
  """Create WlanManager() objects, one per wifi interface."""

  def ParseDevList(errcode, stdout, stderr):
    """Callback for 'iw dev' results."""
    if errcode:
      raise Exception('failed (%d) getting wifi dev list: %r' %
                      (errcode, stderr))
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
          log.Debug('Skipping dev %r because type %r != AP', dev, devtype)

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
        log.Log('Forgetting interface %r.', dev)
        managers.remove(m)
    for phy, dev in phy_devs.iteritems():
      if dev not in existing_devs:
        log.Debug('Creating wlan manager for (%r, %r)', phy, dev)
        managers.append(WlanManager(phy, dev, high_power=high_power))

  RunProc(callback=ParseDevList, args=['iw', 'dev'])


class WifiblasterController(object):
  """State machine and scheduler for packet blast testing.

  WifiblasterController reads parameters from files:

    wifiblaster.duration  Packet blast duration in seconds.
    wifiblaster.enable    Enable packet blast testing.
    wifiblaster.fraction  Number of samples per duration.
    wifiblaster.interval  Average time between packet blasts.
    wifiblaster.size      Packet size in bytes.

  When enabled, WifiblasterController runs packet blasts at random times as
  governed by a Poisson process with rate = 1 / interval. Thus, packet blasts
  are distributed uniformly over time, and every point in time is equally likely
  to be measured by a packet blast. The average number of packet blasts in any
  given window of W seconds is W / interval.

  Each packet blast tests a random associated client. The results output by
  wifiblaster are anonymized and written directly to the log.
  """

  def __init__(self, managers, basedir):
    """Initializes WifiblasterController."""
    self._managers = managers
    self._basedir = basedir
    self._interval = 0  # Disabled.
    self._next_packet_blast_time = float('inf')
    self._next_timeout = 0

  def _ReadFile(self, filename):
    """Returns the contents of a file."""
    with open(filename) as f:
      return f.read()

  def _ReadParameter(self, name, typecast):
    """Returns a parameter value read from a file."""
    try:
      s = self._ReadFile(os.path.join(self._basedir, 'wifiblaster.%s' % name))
    except IOError:
      return None
    try:
      return typecast(s)
    except ValueError:
      return None

  def _SaveWifiblasterResult(self, line, client):
    """Append wifiblaster result to a file.

    Args:
      line: the string result of the wifiblaster run.
      client: the MAC address of the client, as a string.

    The last N results are retained in the file.
    """
    filename = os.path.join(WIFIBLASTERDIR, client)
    with open(filename, 'a+') as f:
      results = f.read(65536).splitlines()
    results.append(line)
    newresults = '\n'.join(results[-128:])
    helpers.WriteFileAtomic(filename, newresults)

  def _LogWifiblasterResults(self, errcode, stdout, stderr):
    """Callback for 'wifiblaster' results."""
    log.Debug('wifiblaster err:%r stdout:%r stderr:%r', errcode, stdout[:70],
              stderr)
    if errcode:
      return

    def Repl(match):
      return log.AnonymizeMAC(consensus_key, helpers.EncodeMAC(match.group()))

    for line in stdout.splitlines():
      log.Log('wifiblaster: %s' %
              re.sub(r'([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}', Repl, line))
      result = re.search(r'([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}', line)
      if result:
        self._SaveWifiblasterResult(line, result.group())

  def _StrToBool(self, s):
    """Returns True if a string expresses a true value."""
    return s.rstrip().lower() in ('true', '1')

  def NextTimeout(self):
    """Returns the time of the next event."""
    return self._next_timeout

  def Poll(self, now):
    """Polls the state machine."""

    def Disable():
      self._interval = 0
      self._next_packet_blast_time = float('inf')

    def StartPacketBlastTimer(interval):
      self._interval = interval
      # Inter-arrival times in a Poisson process are exponentially distributed.
      # The timebase slip prevents a burst of packet blasts in case we fall
      # behind.
      self._next_packet_blast_time = now + random.expovariate(1 / interval)

    # Read parameters.
    duration = self._ReadParameter('duration', float)
    enable = self._ReadParameter('enable', self._StrToBool)
    fraction = self._ReadParameter('fraction', int)
    interval = self._ReadParameter('interval', float)
    rapidpolling = self._ReadParameter('rapidpolling', int)
    size = self._ReadParameter('size', int)

    if rapidpolling > now:
      interval = 10.0

    # Disable.
    if (not enable or duration <= 0 or fraction <= 0 or interval <= 0 or
        size <= 0):
      Disable()

    # Enable or change interval.
    elif self._interval != interval:
      StartPacketBlastTimer(interval)

    # Packet blast.
    elif now >= self._next_packet_blast_time:
      StartPacketBlastTimer(interval)
      clients = [
          (manager.vdevname, assoc.mac)
          for manager in self._managers for assoc in manager.GetState().assoc
      ]
      if clients:
        (interface, client) = random.choice(clients)
        RunProc(
            callback=self._LogWifiblasterResults,
            args=['wifiblaster', '-i', interface, '-d', str(duration), '-f',
                  str(fraction), '-s', str(size), helpers.DecodeMAC(client)])

    # Poll again in at most one second. This allows parameter changes (e.g. a
    # long interval to a short interval) to take effect sooner than the next
    # scheduled packet blast.
    self._next_timeout = min(now + 1, self._next_packet_blast_time)


def do_ssids_match(managers):
  """Check whether the same primary interface name is used on both PHYs.

  Assumes the primary interface is the shortest one, since secondary interface
  names probably use the primary as a prefix.

  Args:
    managers: A list of WlanManagers.

  Returns:
    Returns true iff the same primary interface name is used on both PHYs.
  """
  phy_ssids = {}
  for m in managers:
    if (m.ssid and (m.phyname not in phy_ssids or
                    len(m.ssid) < len(phy_ssids[m.phyname]))):
      phy_ssids[m.phyname] = m.ssid

  ssids = phy_ssids.values()
  return len(ssids) == 2 and ssids[0] == ssids[1]


def main():
  gc.set_debug(gc.DEBUG_STATS |
               gc.DEBUG_COLLECTABLE |
               gc.DEBUG_UNCOLLECTABLE |
               gc.DEBUG_INSTANCES |
               gc.DEBUG_OBJECTS)

  global opt
  o = options.Options(optspec)
  opt, flags, unused_extra = o.parse(sys.argv[1:])
  if helpers.Experiment('WifiUseActiveTime'):
    opt.initial_scans = max(opt.initial_scans, 5)
  opt.scan_interval = float(opt.scan_interval)
  opt.tx_interval = float(opt.tx_interval)
  opt.autochan_interval = float(opt.autochan_interval)
  opt.print_interval = float(opt.print_interval)
  if opt.watch_pid and opt.watch_pid <= 1:
    o.fatal('--watch-pid must be empty or > 1')
  log.LOGLEVEL = opt.debug
  log.ANONYMIZE = opt.anonymize
  log.STATUS_DIR = opt.status_dir

  try:
    os.makedirs(opt.status_dir)
  except OSError as e:
    if e.errno != errno.EEXIST:
      raise

  try:
    os.makedirs(WIFIBLASTERDIR)
  except OSError as e:
    if e.errno != errno.EEXIST:
      raise

  AP_LIST_FILE[0] = os.path.join(opt.status_dir, 'APs')
  PEER_AP_LIST_FILE[0] = os.path.join(opt.status_dir, 'PeerAPs')

  # Seed the consensus key with random data.
  UpdateConsensus(0, os.urandom(16))
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
        wlm.mac = helpers.EncodeMAC(fakemac)
        managers.append(wlm)
  else:
    # The list of managers is also refreshed occasionally in the main loop
    CreateManagers(managers, high_power=opt.high_power)
  if not managers:
    raise Exception('no wifi AP-mode devices found.  Try --fake.')

  wifiblaster_controller = WifiblasterController(managers, opt.status_dir)

  last_sent = last_autochan = last_print = 0
  while 1:
    TouchAliveFile()
    if opt.watch_pid > 1:
      try:
        os.kill(opt.watch_pid, 0)
      except OSError as e:
        if e.errno == errno.ESRCH:
          log.Log('watch-pid %r died; shutting down', opt.watch_pid)
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
    if opt.autochan_interval:
      timeouts.append(last_autochan + opt.autochan_interval)
    if opt.print_interval:
      timeouts.append(last_print + opt.print_interval)
    timeouts.append(wifiblaster_controller.NextTimeout())
    timeout = min(filter(None, timeouts))
    log.Debug2('now=%f timeout=%f  timeouts=%r', gettime(), timeout, timeouts)
    if timeout: timeout -= gettime()
    if timeout < 0: timeout = 0
    if timeout is None and opt.watch_pid: timeout = 5.0
    gotpackets = 0
    for _ in xrange(64):
      r, _, _ = select.select(rfds, [], [], timeout)
      if not r:
        log.WriteEventFile('nopackets')
        break
      timeout = 0
      for i in r:
        for m in managers:
          if i in m.GetReadFds():
            gotpackets += m.ReadReady()
    if gotpackets:
      log.WriteEventFile('gotpacket')
    now = gettime()
    # TODO(apenwarr): how often should we really transmit?
    #   Also, consider sending out an update (almost) immediately when a new
    #   node joins, so it can learn about the other nodes as quickly as
    #   possible.  But if we do that, we need to rate limit it somehow.
    for m in managers:
      m.DoScans()
    if ((opt.tx_interval and now - last_sent > opt.tx_interval) or (
        opt.autochan_interval and now - last_autochan > opt.autochan_interval)):
      if not opt.fake:
        CreateManagers(managers, high_power=opt.high_power)
      for m in managers:
        m.UpdateStationInfo()
    if opt.tx_interval and now - last_sent > opt.tx_interval:
      last_sent = now
      for m in managers:
        m.SendUpdate()
        log.WriteEventFile('sentpacket')
    if opt.autochan_interval and now - last_autochan > opt.autochan_interval:
      last_autochan = now
      for m in managers:
        m.ChooseChannel()
    if opt.print_interval and now - last_print > opt.print_interval:
      last_print = now
      selfmacs = set()
      peers = {}
      peer_data = {}
      seen_peers = {}
      self_signals = {}
      bss_signal = {}
      # Get all peers from all interfaces; remove duplicates.
      for m in managers:
        selfmacs.add(m.mac)
        for peermac, p in m.peer_list.iteritems():
          peers[peermac] = m, p
          if p.me.mac not in m.bss_list:
            continue
          b = m.bss_list[p.me.mac]
          m.ap_signals[helpers.DecodeMAC(p.me.mac)] = b.rssi
      for m, p in peers.values():
        seen_bss_peers = [bss for bss in p.seen_bss if bss.mac in peers]
        if p.me.mac in selfmacs: continue
        seen_peers[helpers.DecodeMAC(p.me.mac)] = seen_bss_peers
        for b in seen_bss_peers:
          if b.mac in selfmacs:
            bss_signal[helpers.DecodeMAC(p.me.mac)] = b.rssi
        self_signals[m.mac] = bss_signal
        peer_data[m.mac] = seen_peers
        log.Log('%s: APs=%-4d peer-APs=%s stations=%s',
                m.AnonymizeMAC(p.me.mac), len(p.seen_bss),
                ','.join('%s(%d)' % (m.AnonymizeMAC(i.mac), i.rssi)
                         for i in sorted(seen_bss_peers,
                                         key=lambda i: -i.rssi)),
                ','.join('%s(%d)' % (m.AnonymizeMAC(i.mac), i.rssi)
                         for i in sorted(p.assoc,
                                         key=lambda i: -i.rssi)))

      for m in managers:
        if m.mac in self_signals:
          m.self_signals = self_signals[m.mac]
          m.WritePeerApInfoFile(peer_data[m.mac])
          m.WriteJsonSignals()
      # Log STA information. Only log station band capabilities if there we can
      # determine it, i.e. if there are APs on both bands with the same SSID.
      log_sta_band_capabilities = do_ssids_match(managers)
      can2G_count = can5G_count = 0
      for m in managers:
        for assoc in m.assoc_list.itervalues():
          anon = m.AnonymizeMAC(assoc.mac)
          if log_sta_band_capabilities:
            if assoc.can5G:
              can5G_count += 1
              capability = '5'
            else:
              can2G_count += 1
              capability = '2.4'
            log.Log('Connected station %s supports %s GHz', anon, capability)
          station = helpers.DecodeMAC(assoc.mac)
          species = clientinfo.taxonomize(station)
          if species:
            log.Log('Connected station %s taxonomy: %s' % (anon, species))
      if log_sta_band_capabilities:
        log.Log('Connected stations: total %d, 5 GHz %d, 2.4 GHz %d',
                can5G_count + can2G_count, can5G_count, can2G_count)

    wifiblaster_controller.Poll(now)
    if not r:
      log.WriteEventFile('ready')


if __name__ == '__main__':
  main()
