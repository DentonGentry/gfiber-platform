#!/usr/bin/python2
"""
802.11 Managment Frame Parser for PCAP files and live interfaces
"""

from __future__ import print_function
import bz2
import csv
import gzip
import os
import struct
import sys
import string
import collections
import Vendor_Specific_OUI_Table
VENDOR_SPECIFIC_OUI = Vendor_Specific_OUI_Table.VENDOR_SPECIFIC_OUI

__author__ = 'shantanuj@google.com (Shantanu Jain)'


class Error(Exception):
  pass


class FileError(Error):
  pass


class PacketError(Error):
  pass


class Struct(dict):
  """Helper to allow accessing dict members using this.that notation."""

  def __init__(self, *args, **kwargs):
    dict.__init__(self, *args, **kwargs)
    self.__dict__.update(**kwargs)

  def __getattr__(self, name):
    return self[name]

  def __setattr__(self, name, value):
    self[name] = value

  def __delattr__(self, name):
    del self[name]


GZIP_MAGIC = '\x1f\x8b\x08'
TCPDUMP_MAGIC = 0xa1b2c3d4
TCPDUMP_VERSION = (2, 4)
LINKTYPE_IEEE802_11_RADIOTAP = 127


class Flags(object):
  """Flags in the radiotap header."""
  CFP = 0x01
  SHORT_PREAMBLE = 0x02
  WEP = 0x04
  FRAGMENTATION = 0x08
  FCS = 0x10
  DATA_PAD = 0x20
  BAD_FCS = 0x40
  SHORT_GI = 0x80


RADIOTAP_FIELDS = [
    ('mac_usecs', 'Q'),
    ('flags', 'B'),
    ('rate', 'B'),
    ('channel', 'HH'),
    ('fhss', 'BB'),
    ('dbm_antsignal', 'b'),
    ('dbm_antnoise', 'b'),
    ('lock_quality', 'H'),
    ('tx_attenuation', 'H'),
    ('db_tx_attenuation', 'B'),
    ('dbm_tx_power', 'b'),
    ('antenna', 'B'),
    ('db_antsignal', 'B'),
    ('db_antnoise', 'B'),
    ('rx_flags', 'H'),
    ('tx_flags', 'H'),
    ('rts_retries', 'B'),
    ('data_retries', 'B'),
    ('channelplus', 'II'),
    ('ht', 'BBB'),
    ('ampdu_status', 'IHBB'),
    ('vht', 'HBB4sBBH'),
]


# Fixed Length Attributes of 802.11 Management Frames
FIXED_LENGTH_ATTRIBUTES = {
    # 'Attribute_name' = byte_size
    'AUTHENTICATION_ALGORITHM_NUMBER' : 2,
    'AUTHENTICATION_TRANSACTION_SEQUENCE_NUMBER' : 2,
    'BEACON_INTERVAL' : 2,
    'CAPABILITY_INFO' : 2,
    'CURRENT_AP_ADDRESS' : 6,
    'LISTEN_INTERVAL' : 2,
    'REASON_CODE' : 2,
    'AID' : 2, # Association ID
    'STATUS_CODE' : 2,
    'TIMESTAMP' : 8,
    #'ACTION' = 0, # This is actually a variable length field -- see 802.11-2012.pdf p449
}


# Information Elements in the Management Frame
# Always one byte long, followed by one byte indicating length of value field in bytes
# One to one mapping - Can 'invert' keys and values and still have a valid dictionary
# See 802.11-2012.pdf p474
INFORMATION_ELEMENTS = {
    0   : 'SSID',
    1   : 'SUPPORTED_RATES',
    2   : 'F_H_PARAMETER_SET',
    3   : 'DSSS_PARAMETER_SET',
    4   : 'CF_PARAMETER_SET',
    5   : 'TRAFFIC_INDICATION_MAP',
    6   : 'IBSS_PARAMETER_SET',
    7   : 'Country',
    8   : 'Hopping_Pattern_Parameters',
    9   : 'Hopping_Pattern_Table',
    10  : 'Request',
    11  : 'BSS_Load',
    12  : 'EDCA_Parameter_Set',
    13  : 'TSPEC',
    14  : 'TCLAS',
    15  : 'Schedule',
    16  : 'CHALLENGE_TEXT',
    # Reserved for challenge text extension 17-31
    32  : 'Power Constraint',
    33  : 'Power Capability',
    34  : 'TPC Request',
    35  : 'TPC Report',
    36  : 'Supported Channels',
    37  : 'Channel Switch Announcement',
    38  : 'Measurement Request ',
    39  : 'Measurement Report',
    40  : 'Quiet',
    41  : 'IBSS DFS',
    42  : 'ERP',
    43  : 'TS Delay',
    44  : 'TCLAS Processing',
    45  : 'HT Capabilities',
    46  : 'QoS Capability',
    # Reserved 47
    48  : 'RSN',
    # Reserved 49
    50  : 'Extended Supported Rates',
    51  : 'AP Channel Report',
    52  : 'Neighbor Report',
    53  : 'RCPI',
    54  : 'Mobility Domain (MDE)',
    55  : 'Fast BSS Transition (FTE)',
    56  : 'Timeout Interval',
    57  : 'RIC Data (RDE)',
    58  : 'DSE Registered Location ',
    59  : 'Supported Operating Classes',
    60  : 'Extended Channel Switch Announcement',
    61  : 'HT Operation',
    62  : 'Secondary Channe',
    63  : 'BSS Average Access Delay',
    64  : 'Antenna',
    65  : 'RSNI',
    66  : 'Measurement Pilot Transmission',
    67  : 'BSS Available Admission Capacity',
    68  : 'BSS AC Access Delay',
    69  : 'Time Advertisement',
    70  : 'RM Enabled Capabilities',
    71  : 'Multiple BSSID',
    72  : '20/40 BSS Coexistence',
    73  : '20/40 BSS Intolerant Channel Report',
    74  : 'Overlapping BSS Scan Parameters',
    75  : 'RIC Descriptor',
    76  : 'Management MIC',
    78  : 'Event Request',
    79  : 'Event Report',
    80  : 'Diagnostic Request',
    81  : 'Diagnostic Report',
    82  : 'Location Parameters',
    83  : 'Nontransmitted BSSID Capability',
    84  : 'SSID List',
    85  : 'Multiple BSSID-Index',
    86  : 'FMS Descriptor',
    87  : 'FMS Request',
    88  : 'FMS Response',
    89  : 'QoS Traffic Capability',
    90  : 'BSS Max Idle Period',
    91  : 'TFS Request',
    92  : 'TFS Response',
    93  : 'WNM-Sleep Mode',
    94  : 'TIM Broadcast Request',
    95  : 'TIM Broadcast Response',
    96  : 'Collocated Interference Report',
    97  : 'Channel Usage',
    98  : 'Time Zone',
    99  : 'DMS Request',
    100 : 'DMS Response',
    101 : 'Link Identifier',
    102 : 'Wakeup Schedule',
    104 : 'Channel Switch Timing',
    105 : 'PTI Control',
    106 : 'TPU Buffer Status',
    107 : 'Interworking',
    108 : 'Advertisement Protocol',
    109 : 'Expedited Bandwidth Request',
    110 : 'QoS Map Set',
    111 : 'Roaming Consortium',
    112 : 'Emergency Alert Identifier',
    113 : 'Mesh Configuration',
    114 : 'Mesh ID',
    115 : 'Mesh Link Metric Report',
    116 : 'Congestion Notification',
    117 : 'Mesh Peering Management',
    118 : 'Mesh Channel Switch Parameters',
    119 : 'Mesh Awake Window',
    120 : 'Beacon Timing',
    121 : 'MCCAOP Setup Request',
    122 : 'MCCAOP Setup Reply',
    123 : 'MCCAOP Advertisement',
    124 : 'MCCAOP Teardown',
    125 : 'GANN',
    126 : 'RANN',
    127 : 'Extended Capabilities',
    # Reserved 128-129
    130 : 'PREQ',
    131 : 'PREP',
    132 : 'PERR',
    # Reserved 133-136
    137 : 'PXU',
    138 : 'PXUC',
    139 : 'Authenticated Mesh Peering Exchange',
    140 : 'MIC',
    141 : 'Destination URI',
    142 : 'U-APSD Coexistence',
    # Reserved 143-173
    174 : 'MCCAOP Advertisement Overview',
    # Reserved 175-220
    221 : 'Vendor Specific',
    # Reserved 222-255
}


INF_ELE_AHEAD = 'INF_ELE_AHEAD'


_STDFRAME = ('ra', 'ta', 'xa', 'seq')


DOT11_TYPES = {
    # 0xByteIdentifier : (HumanNameString, AddressesIncluded,
    #                     Tuple_of_MGMT_Frame_Attrs_Else_None_If_Not_Interested)

    # Management
    0x00: ('AssocReq', _STDFRAME, ('CAPABILITY_INFO', 'LISTEN_INTERVAL', INF_ELE_AHEAD)),
    0x01: ('AssocResp', _STDFRAME, ('CAPABILITY_INFO', 'STATUS_CODE', 'AID', INF_ELE_AHEAD)),
    0x02: ('ReassocReq', _STDFRAME, ('CAPABILITY_INFO', 'LISTEN_INTERVAL',
                                     'CURRENT_AP_ADDRESS', INF_ELE_AHEAD)),
    0x03: ('ReassocResp', _STDFRAME, ('CAPABILITY_INFO', 'STATUS_CODE', 'AID', INF_ELE_AHEAD)),
    0x04: ('ProbeReq', _STDFRAME, (INF_ELE_AHEAD,)),
    0x05: ('ProbeResp', _STDFRAME, ('TIMESTAMP', 'BEACON_INTERVAL', 'CAPABILITY_INFO',
                                    INF_ELE_AHEAD)),
    0x08: ('Beacon', _STDFRAME, ('TIMESTAMP', 'BEACON_INTERVAL', 'CAPABILITY_INFO',
                                 INF_ELE_AHEAD)),
    0x09: ('ATIM', _STDFRAME, None), # Null Body
    0x0a: ('Disassoc', _STDFRAME, ('REASON_CODE', INF_ELE_AHEAD)),
    0x0b: ('Auth', _STDFRAME, ('AUTHENTICATION_ALGORITHM_NUMBER',
                               'AUTHENTICATION_TRANSACTION_SEQUENCE_NUMBER',
                               'STATUS_CODE', INF_ELE_AHEAD)),
    0x0c: ('Deauth', _STDFRAME, ('REASON_CODE', INF_ELE_AHEAD)),
    0x0d: ('Action', _STDFRAME, None),

    # Control
    0x16: ('CtlExt', ('ra',), None),
    0x18: ('BlockAckReq', ('ra', 'ta'), None),
    0x19: ('BlockAck', ('ra', 'ta'), None),
    0x1a: ('PsPoll', ('aid', 'ra', 'ta'), None),
    0x1b: ('RTS', ('ra', 'ta'), None),
    0x1c: ('CTS', ('ra',), None),
    0x1d: ('ACK', ('ra',), None),
    0x1e: ('CongestionFreeEnd', ('ra', 'ta'), None),
    0x1f: ('CongestionFreeEndAck', ('ra', 'ta'), None),

    # Data
    0x20: ('Data', _STDFRAME, None),
    0x21: ('DataCongestionFreeAck', _STDFRAME, None),
    0x22: ('DataCongestionFreePoll', _STDFRAME, None),
    0x23: ('DataCongestionFreeAckPoll', _STDFRAME, None),
    0x24: ('Null', _STDFRAME, None),
    0x25: ('CongestionFreeAck', _STDFRAME, None),
    0x26: ('CongestionFreePoll', _STDFRAME, None),
    0x27: ('CongestionFreeAckPoll', _STDFRAME, None),
    0x28: ('QosData', _STDFRAME, None),
    0x29: ('QosDataCongestionFreeAck', _STDFRAME, None),
    0x2a: ('QosDataCongestionFreePoll', _STDFRAME, None),
    0x2b: ('QosDataCongestionFreeAckPoll', _STDFRAME, None),
    0x2c: ('QosNull', _STDFRAME, None),
    0x2d: ('QosCongestionFreeAck', _STDFRAME, None),
    0x2e: ('QosCongestionFreePoll', _STDFRAME, None),
    0x2f: ('QosCongestionFreeAckPoll', _STDFRAME, None),
}


def Align(i, alignment):
  return i + (alignment - 1) & ~(alignment - 1)


def MacAddr(s):
  return ':'.join(('%02x' % i) for i in struct.unpack('6B', s))


def HexDump(s):
  """Convert a binary array to a printable hexdump."""
  out = ''
  for row in xrange(0, len(s), 16):
    out += '%04x ' % row
    for col in xrange(16):
      if len(s) > row + col:
        out += '%02x ' % ord(s[row + col])
      else:
        out += '   '
      if col == 7:
        out += ' '
    out += ' '
    for col in xrange(16):
      if len(s) > row + col:
        c = s[row + col]
        if len(repr(c)) != 3:  # x -> 'x' and newline -> '\\n'
          c = '.'
        out += c
      else:
        out += ' '
      if col == 7:
        out += ' '
    out += '\n'
  return out


MCS_TABLE = [
    (1, 'BPSK', '1/2', (6.50, 7.20, 13.50, 15.00)),
    (1, 'QPSK', '1/2', (13.00, 14.40, 27.00, 30.00)),
    (1, 'QPSK', '3/4', (19.50, 21.70, 40.50, 45.00)),
    (1, '16-QAM', '1/2', (26.00, 28.90, 54.00, 60.00)),
    (1, '16-QAM', '3/4', (39.00, 43.30, 81.00, 90.00)),
    (1, '64-QAM', '2/3', (52.00, 57.80, 108.00, 120.00)),
    (1, '64-QAM', '3/4', (58.50, 65.00, 121.50, 135.00)),
    (1, '64-QAM', '5/6', (65.00, 72.20, 135.00, 150.00)),
    (2, 'BPSK', '1/2', (13.00, 14.40, 27.00, 30.00)),
    (2, 'QPSK', '1/2', (26.00, 28.90, 54.00, 60.00)),
    (2, 'QPSK', '3/4', (39.00, 43.30, 81.00, 90.00)),
    (2, '16-QAM', '1/2', (52.00, 57.80, 108.00, 120.00)),
    (2, '16-QAM', '3/4', (78.00, 86.70, 162.00, 180.00)),
    (2, '64-QAM', '2/3', (104.00, 115.60, 216.00, 240.00)),
    (2, '64-QAM', '3/4', (117.00, 130.00, 243.00, 270.00)),
    (2, '64-QAM', '5/6', (130.00, 144.40, 270.00, 300.00)),
    (3, 'BPSK', '1/2', (19.50, 21.70, 40.50, 45.00)),
    (3, 'QPSK', '1/2', (39.00, 43.30, 81.00, 90.00)),
    (3, 'QPSK', '3/4', (58.50, 65.00, 121.50, 135.00)),
    (3, '16-QAM', '1/2', (78.00, 86.70, 162.00, 180.00)),
    (3, '16-QAM', '3/4', (117.00, 130.00, 243.00, 270.00)),
    (3, '64-QAM', '2/3', (156.00, 173.30, 324.00, 360.00)),
    (3, '64-QAM', '3/4', (175.50, 195.00, 364.50, 405.00)),
    (3, '64-QAM', '5/6', (195.00, 216.70, 405.00, 450.00)),
    (4, 'BPSK', '1/2', (26.00, 28.80, 54.00, 60.00)),
    (4, 'QPSK', '1/2', (52.00, 57.60, 108.00, 120.00)),
    (4, 'QPSK', '3/4', (78.00, 86.80, 162.00, 180.00)),
    (4, '16-QAM', '1/2', (104.00, 115.60, 216.00, 240.00)),
    (4, '16-QAM', '3/4', (156.00, 173.20, 324.00, 360.00)),
    (4, '64-QAM', '2/3', (208.00, 231.20, 432.00, 480.00)),
    (4, '64-QAM', '3/4', (234.00, 260.00, 486.00, 540.00)),
    (4, '64-QAM', '5/6', (260.00, 288.80, 540.00, 600.00)),
    (1, 'BPSK', '1/2', (0, 0, 6.50, 7.20)),
]


def McsToRate(known, flags, index):
  """Given MCS information for a packet, return the corresponding bitrate."""
  if known & (1 << 0):
    bw = (20, 40, 20, 20)[flags & 0x3]
  else:
    bw = 20
  if known & (1 << 2):
    gi = ((flags & 0x4) >> 2)
  else:
    gi = 0
  if known & (1 << 1):
    mcs = index
  else:
    mcs = 0
  if bw == 20:
    si = 0
  else:
    si = 2
  if gi:
    si += 1
  return MCS_TABLE[mcs][3][si]


def Packetize(stream):
  """Packetize a stream of 802.11 Packets.

  Given a file stream containing pcap data or a valid interface string,
  yield a series of packets. All interfaces must have Radiotap headers.
  A live interface without Radiotap headers will fail silently with
  incorrect parsing.
    Args:
      stream: either an open pcap file, or a string of a valid interface
          name.
    Returns:
      An iterator that reveals an (opt, frame) tuple. Opt is a Struct()-
      style dictionary that contains all the parsed fields in the frame.
      Of particular interest is the 'dot11_mgmt_frame_attrs' key in opt,
      which contains an OrderedDict of all the elements in a 802.11 MGMT
      frame in stream. frame is the raw bytes of the packet.
    Raises:
      FileError: if stream is either:
        1) Not a PCAP file
        2) Does not have Radiotap headers
  """
  isFile = type(stream) is file

  if isFile:
    magicbytes = stream.read(4)

    if magicbytes[:len(GZIP_MAGIC)] == GZIP_MAGIC:
      stream.seek(-4, os.SEEK_CUR)
      stream = gzip.GzipFile(mode='rb', fileobj=stream)
      magicbytes = stream.read(4)

    # pcap global header
    if struct.unpack('<I', magicbytes) == (TCPDUMP_MAGIC,):
      byteorder = '<'
    elif struct.unpack('>I', magicbytes) == (TCPDUMP_MAGIC,):
      byteorder = '>'
    else:
      raise FileError('unexpected tcpdump magic %r' % magicbytes)
    (version_major, version_minor,
     unused_thiszone,
     unused_sigfigs,
     snaplen,
     network) = struct.unpack(byteorder + 'HHiIII', stream.read(20))
    version = (version_major, version_minor)
    if version != TCPDUMP_VERSION:
      raise FileError('unexpected tcpdump version %r' % version)
    if network != LINKTYPE_IEEE802_11_RADIOTAP:
      raise FileError('unexpected tcpdump network type %r' % network)

  else:
    assert type(stream) is str
    # imported here so user doesn't need pypcap dependency for PCAPs
    import pcap
    handle = pcap.pcap(stream)

  last_ta = None
  last_ra = None
  while 1:
    opt = Struct({})

    if isFile:
      # pcap packet header
      pcaphdr = stream.read(16)
      if len(pcaphdr) < 16: break  # EOF
      (ts_sec, ts_usec, incl_len, orig_len) = struct.unpack(
          byteorder + 'IIII', pcaphdr)
      if incl_len > orig_len:
        raise FileError('packet incl_len(%d) > orig_len(%d): invalid'
                        % (incl_len, orig_len))
      if incl_len > snaplen:
        raise FileError('packet incl_len(%d) > snaplen(%d): invalid'
                        % (incl_len, snaplen))

      opt.pcap_secs = ts_sec + (ts_usec / 1e6)

      # pcap packet data
      radiotap = stream.read(incl_len)
      if len(radiotap) < incl_len: break  # EOF

      opt.incl_len = incl_len
      opt.orig_len = orig_len

    else:
      # get next frame from iface handle
      ts, radiotap = handle.next()

    # radiotap header (always little-endian)
    (it_version, unused_it_pad,
     it_len, it_present) = struct.unpack('<BBHI', radiotap[:8])
    if it_version != 0:
      raise PacketError('unknown radiotap version %d' % it_version)
    frame = radiotap[it_len:]
    optbytes = radiotap[8:it_len]

    ofs = 0
    for i, (name, structformat) in enumerate(RADIOTAP_FIELDS):
      if it_present & (1 << i):
        ofs = Align(ofs, struct.calcsize(structformat[0]))
        sz = struct.calcsize(structformat)
        v = struct.unpack(structformat, optbytes[ofs:ofs + sz])
        if name == 'mac_usecs':
          opt.mac_usecs = v[0]
        elif name == 'channel':
          opt.freq = v[0]
          opt.channel_flags = v[1]
        elif name == 'ht':
          ht_known, ht_flags, ht_index = v
          opt.ht = v
          opt.mcs = ht_index
          opt.rate = McsToRate(ht_known, ht_flags, ht_index)
          if isFile:
            # Occasionally causes errors in live parsing
            opt.spatialstreams = MCS_TABLE[ht_index][0]
        else:
          opt[name] = v if len(v) > 1 else v[0]
        ofs += sz

    try:
      (fctl, duration) = struct.unpack('<HH', frame[0:4])
    except struct.error:
      (fctl, duration) = 0, 0
    dot11ver = fctl & 0x0003
    dot11type = (fctl & 0x000c) >> 2
    dot11subtype = (fctl & 0x00f0) >> 4
    dot11dsmode = (fctl & 0x0300) >> 8
    dot11morefrag = (fctl & 0x0400) >> 10
    dot11retry = (fctl & 0x0800) >> 11
    dot11powerman = (fctl & 0x1000) >> 12
    dot11moredata = (fctl & 0x2000) >> 13
    dot11wep = (fctl & 0x4000) >> 14
    dot11order = (fctl & 0x8000) >> 15
    fulltype = (dot11type << 4) | dot11subtype
    opt.type = fulltype
    opt.duration = duration
    (typename, typefields, dot11_mgmt_frame_attrs) = DOT11_TYPES.get(
        fulltype, ('Unknown', ('ra',), None))
    opt.typestr = typename
    opt.dsmode = dot11dsmode
    opt.retry = dot11retry
    opt.powerman = dot11powerman
    opt.order = dot11order

    ofs = 4
    for i, fieldname in enumerate(typefields):
      if fieldname == 'seq':
        if len(frame) < ofs + 2: break
        seq = struct.unpack('<H', frame[ofs:ofs + 2])[0]
        opt.seq = (seq & 0xfff0) >> 4
        opt.frag = (seq & 0x000f)
        ofs += 2
      else:  # ta, ra, xa
        if len(frame) < ofs + 6: break
        opt[fieldname] = MacAddr(frame[ofs:ofs + 6])
        ofs += 6

    # Extract 802.11 MAC Header frame payload IFF frame is a MGMT Frame
    if dot11_mgmt_frame_attrs is not None:
      dot11_mgmt_frame_values = collections.OrderedDict()
      opt['dot11_mgmt_frame_attrs'] = dot11_mgmt_frame_values
      for attribute in dot11_mgmt_frame_attrs:
        if attribute is INF_ELE_AHEAD:
          # Parse in TLV formatting, until there are not enough bytes left to:
          #   1)  extract a type and length field
          #   2)  not enough bytes after the specificed length to extract the value
          # ##
          # One byte for type, one byte for length
          while len(frame) > ofs + 2:
            (attr_type, attr_length) = struct.unpack('<BB', frame[ofs:ofs+2])
            ofs += 2
            if len(frame) < ofs + attr_length: break
            attr_value = struct.unpack('<'+'B'*attr_length, frame[ofs:ofs+attr_length])
            attr_value = map(hex, attr_value)
            if attr_type in dot11_mgmt_frame_values:
              # Repeated IE case
              dot11_mgmt_frame_values[attr_type].append(attr_value)
            else:
              dot11_mgmt_frame_values[attr_type] = [attr_value]
            ofs += attr_length
        else:
          # Fixed Fields case, assume all are present
          attr_length = FIXED_LENGTH_ATTRIBUTES[attribute]
          if len(frame) < ofs + attr_length:
            raise EOFError('Bad Packet, a Fixed Attribute ' + attribute + ' is not present')
          attr_value = struct.unpack('<'+'B'*attr_length, frame[ofs:ofs+attr_length])
          dot11_mgmt_frame_values[attribute] = [map(hex, attr_value)]
          ofs += attr_length

    # ACK and CTS packets omit TA field for efficiency, so we have to fill
    # it in from the previous packet's RA field.  We can check that the
    # new packet's RA == the previous packet's TA, just to make sure we're
    # not lying about it.
    if opt.get('flags', Flags.BAD_FCS) & Flags.BAD_FCS:
      opt.bad = 1
    else:
      opt.bad = 0
    if not opt.get('ta'):
      if (last_ta and last_ra
          and last_ta == opt.get('ra')
          and last_ra != opt.get('ra')):
        opt['ta'] = last_ra
      last_ta = None
      last_ra = None
    else:
      last_ta = opt.get('ta')
      last_ra = opt.get('ra')

    yield opt, frame


def StdOut(p, VERBOSE=True, DESIRED_OUTPUT=None):
  """Formatted parser output to StdOut."""
  if VERBOSE and DESIRED_OUTPUT is not None:
    for k, v in DESIRED_OUTPUT.items():
      print("Want " + str(v), DOT11_TYPES[k][0])

  print('\n#', 'Type', 'src-MAC', 'Timestamp')
  for i, (opt, frame) in enumerate(Packetize(p)):
    if DESIRED_OUTPUT is None:
      pass
    elif opt.type not in DESIRED_OUTPUT or DESIRED_OUTPUT[opt.type] == 0:
      continue
    else:
      DESIRED_OUTPUT[opt.type] -= 1

    # ts = opt.pcap_secs
    # ts = '%.09f' % ts
    if 'xa' in opt:
      src = opt.xa
    else:
      src = 'no:xa:00:00:00:00'
    if 'mac_usecs' in opt:
      mac_usecs = opt.mac_usecs
    else:
      mac_usecs = 0
    if 'seq' in opt:
      seq = opt.seq
    else:
      seq = 'noseq'
    # print(i+1, opt.get('flags'), opt.get('bad'))
    if 'flags' in opt:
      if opt.flags & Flags.BAD_FCS:
        continue
    # if 'mcs' in opt:
    #   print(i + 1,
    #       src, opt.dsmode, opt.typestr, ts, opt.rate, mac_usecs,
    #       opt.orig_len, seq, opt.flags)
    # else:
    #   print(i + 1,
    #       src, opt.dsmode, opt.typestr, ts, opt.get('rate'), mac_usecs,
    #       opt.orig_len, seq, opt.get('flags'))
    if VERBOSE:
      print(i + 1, opt.typestr, opt.get('ta', 'Unknown Sender'))
    else:
      print(opt.typestr, opt.get('ta', 'Unknown Sender'))
    if 'dot11_mgmt_frame_attrs' in opt:
      for type_tag, values in opt['dot11_mgmt_frame_attrs'].iteritems():
        for value in values:
          if type(type_tag) is str:
            # Fixed Field case
            tag_readable = type_tag
          else:
            tag_readable = INFORMATION_ELEMENTS.get(type_tag, hex(type_tag) + ' (Unknown Information Element)')
          output = tag_readable + ': '
          if type_tag == 0x00: #SSID -- interpret as ASCII string
            value = map(chr, map(lambda x: int(x, 16), value))
            output += '\n'+ ''.join(value)
          else:
            value = map(lambda x: string.replace(x, '0x', ''), value)
            value_formatted = []
            # Prepend '0' to single char values
            for item in value:
              if len(item) == 1:
                value_formatted.append('0'+item)
              else:
                value_formatted.append(item)
            if type_tag == 221: # Vendor Specific -- Add vendor tag and hex OUI repr.
              output += (VENDOR_SPECIFIC_OUI[int(''.join(value_formatted[0:3]), 16)] +
                         ' (' + ' '.join(value_formatted[0:3]) + '): ')
              value_formatted = value_formatted[3:]
            output += '\n'
            output += ' '.join(value_formatted)
          print(output)
    print('') # implicit newline
  if VERBOSE and DESIRED_OUTPUT is not None:
    for k, v in DESIRED_OUTPUT.items():
      if v != 0:
        print('Did not find ' + str(v), DOT11_TYPES[k][0])


def ZOpen(fn):
  if fn.endswith('.bz2'):
    return bz2.BZ2File(fn)
  return open(fn)


if __name__ == '__main__':
  USAGE_STRING = (
    'USAGE: wifipacket.py [options] "Interface_or_PCAP_File"\n\n' +
    '-a : Parse all packets, ignoring DESIRED_OUTPUT\n' +
    '-i : Specify an interface for live parsing instead of a PCAP file\n' +
    '--help: Get to this page\n' +
    'Input must always have Radiotap headers. PCAPs are always checked,\n' +
    'but user must check that a live interface supplies Radiotap headers.\n' +
    'Check this via Wireshark or another libpcap based utility. It may\n' +
    'be possible to force Radiotap headers by entering monitor mode on\n' +
    'the wlan interface.\n' +
    '$ ip link set wlan0 down\n' +
    '$ iwconfig wlan0 mode monitor\n' +
    '$ ip link set wlan0 up\n'
  )

  # Look in DOT11_TYPES for packet types (hex)
  DESIRED_OUTPUT = {0x00: 1, 0x02: 1, 0x04: 1, 0x0b: 1}
  VERBOSE = True

  if '--help' in sys.argv:
    print(USAGE_STRING)
  else:
    if '-a' in sys.argv:
      DESIRED_OUTPUT = None
    if '-i' in sys.argv:
      StdOut(sys.argv[-1], VERBOSE, DESIRED_OUTPUT)
    StdOut(ZOpen(sys.argv[-1]), VERBOSE, DESIRED_OUTPUT)
