#!/usr/bin/python
"""Fake minissdpd for unit tests.

"""

import BaseHTTPServer
import socket
import sys


class XmlHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  def do_GET(self):
    self.send_response(200)
    self.send_header('Content-type','text/xml')
    self.end_headers()
    self.wfile.write("""<root>
        <specVersion><major>1</major><minor>0</minor></specVersion>
        <device><friendlyName>Test Device</friendlyName>
        <manufacturer>Google Fiber</manufacturer>
        <modelDescription>Unit Test</modelDescription>
        <modelName>ssdptax</modelName>
    </device></root>""")

def main():
  un = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
  un.bind(sys.argv[1])
  un.listen(1)
  conn, _ = un.accept()

  s = BaseHTTPServer.HTTPServer(("", 0), XmlHandler)
  sn = s.socket.getsockname()
  port = sn[1]
  url = 'http://127.0.0.1:%d/foo.xml' % port
  st = 'server type'
  uuid = 'uuid goes here'
  data = [1]
  data.extend([len(url)] + list(url))
  data.extend([len(st)] + list(st))
  data.extend([len(uuid)] + list(uuid))

  _ = conn.recv(8192)
  conn.sendall(bytearray(data))
  s.handle_request()


if __name__ == '__main__':
  main()
