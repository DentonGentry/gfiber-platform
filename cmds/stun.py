#!/usr/bin/python
"""Basic implementation of a STUN client.

STUN lets you discover your publicly-visible IP address, as seen by a
particular server or servers.  Some NATs will assign a different address for
each outgoing connection, so this client lets you query multiple servers and
discover possibly multiple ports and IP addresses.

This is usable as a standalone program or a python library module.
"""
import os
import select
import socket
import struct
import sys
import time
import options


PORT = 3478
MAGIC_COOKIE = 0x2112a442  # defined by the STUN standard, not a secret

# STUN request types
BIND_REQUEST = 0x0001
BIND_INDICATION = 0x0011
BIND_SUCCESS = 0x0101
BIND_ERROR = 0x0111

# STUN tag:value attributes
ATTR_MASK = 0x07ff

ATTR_MAPPED_ADDRESS = 0x0001
ATTR_USERNAME = 0x0006
ATTR_MESSAGE_INTEGRITY = 0x0008
ATTR_ERROR_CODE = 0x0009
ATTR_UNKNOWN_ATTRIBUTES = 0x000a
ATTR_REALM = 0x0014
ATTR_NONCE = 0x0015
ATTR_XOR_MAPPED_ADDRESS = 0x0020
ATTR_SOFTWARE = 0x8022
ATTR_ALTERNATE_SERVER = 0x8023
ATTR_FINGERPRINT = 0x8028

# List of Google-provided STUN servers.
GOOGLE_SERVER_LIST = [
    'stun.l.google.com:19302',
    'stun1.l.google.com:19302',
    'stun2.l.google.com:19302',
    'stun3.l.google.com:19302',
    'stun4.l.google.com:19302',
]

# Some non-Google-provided STUN servers, not used by default.
# copied from:
# http://code.google.com/p/natvpn/source/browse/trunk/stun_server_list
EXTRA_SERVER_LIST = [
    'stun01.sipphone.com',
    'stun.ekiga.net',
    'stun.fwdnet.net',
    'stun.ideasip.com',
    'stun.iptel.org',
    'stun.rixtelecom.se',
    'stun.schlund.de',
    'stunserver.org',
    'stun.softjoys.com',
    'stun.voiparound.com',
    'stun.voipbuster.com',
    'stun.voipstunt.com',
    'stun.voxgratia.org',
    'stun.xten.com',
]


optspec = """
stun [options...]
--
s,servers=   Comma-separated STUN servers to use (default=google.com servers)
x,extended   Add extra non-Google default STUN servers
v,verbose    Print extra information about intermediate results
t,timeout=   STUN query timeout (in seconds) [10.0]
q,quiet      Don't print output except the requested fields
"""


class Error(Exception):
  pass


class ParseError(Error):
  pass


class TimeoutError(Error):
  pass


class DnsError(Error):
  pass


try:
  import monotime  # pylint: disable=unused-import,g-import-not-at-top
except ImportError:
  pass
try:
  gettime = time.monotonic
except AttributeError:
  gettime = time.time


def Log(s, *args):
  sys.stdout.flush()
  s += '\n'
  if args:
    sys.stderr.write(s % args)
  else:
    sys.stderr.write(s)
  sys.stderr.flush()


def _EncodeAttr(attrtype, value):
  value = str(value)
  pkt = struct.pack('!HH', attrtype, len(value)) + value
  while (len(pkt) % 4) != 0:  # 32-bit alignment
    pkt += '\0'
  return pkt


def _DecodeAddress(value):
  family, port = struct.unpack('!HH', value[:4])
  addr = value[4:]
  if family == 0x01:  # IPv4
    return socket.inet_ntop(socket.AF_INET, addr), port
  elif family == 0x02:  # IPv6
    return socket.inet_ntop(socket.AF_INET6, addr), port
  else:
    raise ValueError('invalid family %d: expected 1=IPv4 or 2=IPv6' % family)


def _XorChars(s, keystr):
  for a, b in zip(s, keystr):
    yield chr(ord(a) ^ ord(b))


def _Xor(s, keystr):
  return ''.join(_XorChars(s, keystr))


def _DecodeAttrs(text, transaction_id):
  while text:
    attrtype, length = struct.unpack('!HH', text[:4])
    value = text[4:4+length]
    length += (4 - (length % 4)) % 4  # 32-bit alignment
    text = text[4+length:]
    if (attrtype & ATTR_MASK) == (ATTR_XOR_MAPPED_ADDRESS & ATTR_MASK):
      # technically I think XOR_MAPPED_ADDRESS *without* the 0x8000 flag is
      # a bug based on my reading of rfc5389, but some servers produce it
      # anyhow, so fine.
      key = struct.pack('!I', MAGIC_COOKIE) + transaction_id
      value = value[0:2] + _Xor(value[2:4], key) + _Xor(value[4:], key)
      yield ATTR_MAPPED_ADDRESS, _DecodeAddress(value)
    elif attrtype == ATTR_MAPPED_ADDRESS:
      yield ATTR_MAPPED_ADDRESS, _DecodeAddress(value)
    else:
      yield attrtype, value


def _Encode(msgtype, transaction_id, *attrs):
  """Encode message type, transaction id, and attrs into a STUN message."""
  transaction_id = str(transaction_id)
  if len(transaction_id) != 12:
    raise ValueError('transactionid %r must be exactly 12 bytes'
                     % transaction_id)
  attrtext = ''.join(_EncodeAttr(attrtype, attrval)
                     for attrtype, attrval in attrs)
  pkt = (struct.pack('!HHI', msgtype, len(attrtext), MAGIC_COOKIE) +
         transaction_id +
         attrtext)
  return pkt


def _Decode(text):
  """Decode a STUN message into message type, transaction id, and attrs."""
  if len(text) < 20:
    raise ParseError('packet length %d must be >= 20' % len(text))
  msgtype, length, cookie = struct.unpack('!HHI', text[:8])
  transaction_id = text[8:20]
  attrtext = text[20:]
  if cookie != MAGIC_COOKIE:
    raise ParseError('cookie %r should be %r' % (cookie, MAGIC_COOKIE))
  if length != len(attrtext):
    raise ParseError('packet length field: expected %d, got %d'
                     % (len(attrtext), length))
  attrs = list(_DecodeAttrs(attrtext, transaction_id))
  return msgtype, transaction_id, attrs


def ServerAddresses(sock, servers=None, extended=False, verbose=False):
  """Return a list of DNS-resolved addresses for a set of STUN servers.

  Args:
    sock: an example socket to look up STUN servers for.  (eg. if it's an IPv6
      socket, we need to use IPv6 STUN servers.)
    servers: a list of STUN server names (defaults to an internal list of
      Google-provided servers).
    extended: if using the default server list and this is true, extend
      the list of servers to include some non-Google servers for extra
      coverage.  (This is important when doing advanced NAT penetration.)
    verbose: log some progress messages to stderr.
  Returns:
    A list of (ip, port) pairs, each of which is suitable for use with
    sock.connect() or sock.sendto().
  Raises:
    DnsError: if none of the server names are valid.
    (DnsError is a subclass of Error)
  """
  if not servers:
    if extended:
      servers = GOOGLE_SERVER_LIST + EXTRA_SERVER_LIST
    else:
      servers = GOOGLE_SERVER_LIST
  out = []
  ex = None
  # TODO(apenwarr): much faster if we do DNS in parallel by fork()ing
  if verbose:
    Log('stun: looking up %d DNS names', len(servers))
  for server in servers:
    server = server.strip()
    if ':' in server:
      host, port = server.split(':', 1)
    else:
      host, port = server, PORT
    try:
      addrs = socket.getaddrinfo(host, port, sock.family, sock.type,
                                 sock.proto, socket.AI_V4MAPPED)
      for i in addrs:
        out.append(i[4])  # host, port only
    except socket.error, e:
      ex = e
  if ex and not out:
    raise DnsError(ex)
  return list(set(out))  # remove duplicates


def Lookup(sock, addrs, timeout, verbose=False):
  """Return a list of our publicly-visible (ipaddr, port) combinations.

  This queries a list of STUN servers.  Some NATs may map the same local
  address to different IP addresses and/or ports depending on what destination
  address you connect to, so this function may return multiple answers.

  For the same reason, this function needs to send/receive STUN packets
  from/to exactly the same socket you intend to use for communication after
  discovering its address.  Thus you should be prepared to receive and
  discard any extraneous STUN response packets on that socket for a while,
  even after this function returns.

  Args:
    sock: the socket to discover the address of.
    addrs: a list of STUN servers, probably from calling ServerAddresses().
    timeout: the maximum number of seconds to wait for a response, in seconds.
    verbose: log some progress messages to stderr.
  Returns:
    A list of (ip, port) pairs, each of which is suitable for use with
    sock.connect() or sock.sendto().  Presumably you will provide one or
    more of these addresses to some other client who wants to talk to you.
  Raises:
    ParseError: if we receive an invalid STUN packet.
    TimeoutError: if no servers respond before the timeout.
    (Both errors are subclasses of Error)
  """
  waiting = set(addrs)
  out = set()
  step = 0
  now = gettime()
  next_time = now
  end_time = now + timeout
  transaction_id = os.urandom(12)
  pkt = _Encode(BIND_REQUEST, transaction_id)
  while now < end_time:
    if now >= next_time:
      if verbose:
        Log('stun: sending %d requests', len(waiting))
      for addr in waiting:
        sock.sendto(pkt, 0, addr)
      next_time = min(end_time, now + 0.5 * (2 ** step))
      step += 1
    now = gettime()
    r, _, _ = select.select([sock], [], [], max(0, next_time - now))
    if r:
      rpkt, raddr = sock.recvfrom(4096)
      if raddr not in waiting:
        if verbose:
          Log('warning: %r: unexpected response, ignoring.', raddr)
      else:
        rtype, rtransaction_id, rattrs = _Decode(rpkt)
        if rtype != BIND_SUCCESS:
          if verbose:
            Log('warning: %r: got transaction type %r, expected %r',
                raddr, rtype, BIND_SUCCESS)
        elif rtransaction_id != transaction_id:
          if verbose:
            Log('warning: %r: got transaction id %r, expected %r',
                raddr, rtransaction_id, transaction_id)
        else:
          for rattr_type, rattr_value in rattrs:
            if rattr_type == ATTR_MAPPED_ADDRESS:
              out.add(rattr_value)
              if raddr in waiting:
                waiting.remove(raddr)
    elif len(waiting) <= len(addrs)/3:
      # 2/3 of the servers ought to be enough
      break
    now = gettime()
  if not out:
    raise TimeoutError('no servers responded')
  return list(out)


def main():
  """Standalone STUN command-line client."""
  o = options.Options(optspec)
  opt, _, _ = o.parse(sys.argv[1:])

  s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
  addrs = ServerAddresses(s, opt.servers and opt.servers.split(',') or None,
                          extended=opt.extended, verbose=opt.verbose)
  if opt.verbose:
    Log('servers: %s', addrs)
  myaddrs = Lookup(s, addrs, timeout=float(opt.timeout), verbose=opt.verbose)
  if opt.verbose:
    Log('myaddrs: %s', myaddrs)
  myips, myports = zip(*myaddrs)
  if opt.verbose:
    Log('myips: %s', list(set(myips)))
    Log('myports: %s', list(set(myports)))
  for ip in myips:
    print ip


if __name__ == '__main__':
  main()
