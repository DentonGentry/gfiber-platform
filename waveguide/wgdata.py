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

"""Data format for waveguide packets."""

from collections import namedtuple
import struct
import zlib


PROTO_MAGIC = 'wave'
PROTO_VERSION = 2


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
# last_seen: the time of the last time this AP was seen in a scan.
# cap: capabilities bitmask.
# phy: the dot11PHYType.
# reg: regulatory domain, like 'US'.
BSS = namedtuple('BSS', 'is_ours,mac,freq,rssi,flags,last_seen,cap,phy,reg')
BSS_FMT = '!B6sHbIIHB2s'

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
# can5G: whether the station supports 5GHz.
Assoc = namedtuple('Assoc', 'mac,rssi,last_seen,can5G')
Assoc_FMT = '!6sbI?'

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


def EncodePacket(state):
  """Generate a binary waveguide packet for sending via multicast."""
  me = state.me
  now = me.now
  me_out = struct.pack(Me_FMT, me.now, me.uptime_ms,
                       me.consensus_key, me.mac, me.flags)
  bss_out = ''
  for bss in state.seen_bss:
    bss_out += struct.pack(BSS_FMT,
                           bss.is_ours,
                           bss.mac,
                           bss.freq,
                           bss.rssi,
                           bss.flags,
                           now - bss.last_seen,
                           bss.cap,
                           bss.phy,
                           bss.reg)
  chan_out = ''
  for chan in state.channel_survey:
    chan_out += struct.pack(Channel_FMT,
                            chan.freq,
                            chan.noise_dbm,
                            chan.observed_ms,
                            chan.busy_ms)
  assoc_out = ''
  for assoc in state.assoc:
    assoc_out += struct.pack(Assoc_FMT,
                             assoc.mac,
                             assoc.rssi,
                             now - assoc.last_seen,
                             assoc.can5G)
  arp_out = ''
  for arp in state.arp:
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
