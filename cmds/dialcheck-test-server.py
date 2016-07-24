#!/usr/bin/python
"""Fake SSDP server for unit tests.

"""

import errno
import os
import signal
import socket
import SocketServer
import struct
import sys


notify = """LOCATION: http://1.1.1.1:1/test.xml\r\n
CACHE-CONTROL: max-age=1800\r\n
EXT:\r\n
SERVER: test_ssdp/1.0\r\n
ST: urn:dial-multiscreen-org:service:dial:1\r\n
USN: uuid:number::urn:dial-multiscreen-org:service:dial:1\r\n"""


class SSDPHandler(SocketServer.BaseRequestHandler):
  def handle(self):
    self.request[1].sendto(notify, self.client_address)


def check_pid(pid):
  try:
    os.kill(pid, 0)
  except OSError as e:
    if e.errno == errno.ESRCH:
      return False
  return True


def timeout(unused_signum, unused_frame):
  ppid = os.getppid()
  if ppid == 1 or not check_pid(ppid):
    print 'timed out!'
    sys.exit(2)
  else:
    signal.alarm(1)


def main():
  signal.signal(signal.SIGALRM, timeout)
  signal.alarm(1)
  SocketServer.UDPServer.allow_reuse_address = True
  s = SocketServer.UDPServer(('', 0), SSDPHandler)
  s.socket.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP,
      socket.inet_aton('239.255.255.250') + socket.inet_aton('0.0.0.0'))
  sn = s.socket.getsockname()
  port = sn[1]
  open(sys.argv[1], "w").write(str(port))
  s.handle_request()


if __name__ == '__main__':
  main()
