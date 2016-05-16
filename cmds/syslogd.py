#!/usr/bin/python
"""syslogd: a simple syslog daemon."""

import errno
import os
import re
import socket
import sys

import options

optspec = """
syslogd [-f FILTERFILE] [-l listenip] [-p port]
--
f,filters=   path to a file containing filters [/etc/syslog.conf]
l,listenip=  IP address to listen on [::]
p,port=      UDP port to listen for syslog messages on [5514]
v,verbose    increase log output
"""


MAX_LENGTH = 1180


def serve(sock, filters, verbose=False):
  """Handle a raw syslog message on the wire."""
  while True:
    raw_message, address = sock.recvfrom(4096)
    client_ip = address[0]

    # util-linux `logger` writes a trailing NUL;
    # /dev/log required this long ago.
    message = raw_message.strip('\x00\r\n')

    # Require only printable 7-bit ASCII to remain (RFC3164).
    if not all(32 <= ord(c) <= 126 for c in message):
      print >>sys.stderr, ('[%s]: discarded message with unprintable characters'
                           % client_ip)
      continue

    if len(message) > MAX_LENGTH:
      print >>sys.stderr, ('[%s]: discarded %dB message over max length %dB'
                           % (client_ip, len(message), MAX_LENGTH))
      continue

    for f in filters:
      if f.search(message):
        if verbose:
          print >>sys.stderr, 'matched by filter: %r' % f.pattern

        print '[%s]: %s' % (client_ip, message)
        sys.stdout.flush()
        break
    else:
      if verbose:
        print >>sys.stderr, ('[%s]: discarded unrecognized message: %s'
                             % (client_ip, message))
      else:
        print >>sys.stderr, '[%s]: discarded unrecognized message' % client_ip


def main(argv):
  o = options.Options(optspec)
  opt, unused_flags, unused_extra = o.parse(argv)

  if opt.filters:
    filter_patterns = open(opt.filters).read().splitlines()
    filters = [re.compile(p) for p in filter_patterns]
    print >>sys.stderr, 'using filters: %r' % filter_patterns

  sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
  sock.bind((opt.listenip, opt.port))

  print >>sys.stderr, 'listening on UDP [%s]:%d' % (opt.listenip, opt.port)
  try:
    os.makedirs('/tmp/syslogd')
  except OSError as e:
    if e.errno != errno.EEXIST:
      raise

  open('/tmp/syslogd/ready', 'w')
  serve(sock, filters, opt.verbose)


if __name__ == '__main__':
  main(sys.argv)
