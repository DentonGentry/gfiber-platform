#!/usr/bin/python
"""Fake minissdpd for unit tests."""

import BaseHTTPServer
import socket
import SocketServer
import sys
import threading


text_device_xml = """<root>
  <specVersion><major>1</major><minor>0</minor></specVersion>
  <device><friendlyName>Test Device</friendlyName>
  <manufacturer>Google Fiber</manufacturer>
  <modelDescription>Unit Test</modelDescription>
  <modelName>ssdptax</modelName>
</device></root>"""


email_address_xml = """<root>
  <specVersion><major>1</major><minor>0</minor></specVersion>
  <device><friendlyName>FOOBAR: foo@example.com:</friendlyName>
  <manufacturer>Google Fiber</manufacturer>
  <modelDescription>Unit Test</modelDescription>
  <modelName>ssdptax</modelName>
</device></root>"""


no_friendlyname_xml = """<root>
  <specVersion><major>1</major><minor>0</minor></specVersion>
  <device></device></root>"""


ssdp_device_xml = """<root>
  <specVersion><major>1</major><minor>0</minor></specVersion>
  <device><friendlyName>Test Device</friendlyName>
  <manufacturer>Google Fiber</manufacturer>
  <modelDescription>Unit Test</modelDescription>
  <modelName>ssdptax multicast</modelName>
</device></root>"""


notify_template = 'NOTIFY\r\nHOST:239.255.255.250:1900\r\nLOCATION:%s\r\n'
notify_text = ['']


minissdpd_response = ['']
keep_running = [True]


class HttpHandler(BaseHTTPServer.BaseHTTPRequestHandler):

  def do_GET(self):  # pylint: disable=invalid-name
    """Respond to an HTTP GET for SSDP DeviceInfo."""
    self.send_response(200)
    self.send_header('Content-type', 'text/xml')
    self.end_headers()
    if self.path.endswith('text_device_xml'):
      self.wfile.write(text_device_xml)
    if self.path.endswith('email_address_xml'):
      self.wfile.write(email_address_xml)
    if self.path.endswith('no_friendlyname_xml'):
      self.wfile.write(no_friendlyname_xml)
    if self.path.endswith('ssdp_device_xml'):
      self.wfile.write(ssdp_device_xml)


class ThreadingHTTPServer(SocketServer.ThreadingMixIn,
                          BaseHTTPServer.HTTPServer):
  pass


class UnixHandler(SocketServer.StreamRequestHandler):
  """Respond to a command on MiniSSDPd's Unix socket."""

  def handle(self):
    data = self.request.recv(8192)
    if 'quitquitquit' in data:
      print 'Received quitquitquit, exiting...'
      keep_running[0] = False
      return
    else:
      self.request.sendall(bytearray(minissdpd_response[0]))


class UdpHandler(SocketServer.DatagramRequestHandler):

  def handle(self):
    self.request[1].sendto(bytearray(notify_text[0]), self.client_address)


class ThreadingUdpServer(SocketServer.ThreadingUDPServer):
  allow_reuse_address = True


def main():
  socketpath = sys.argv[1]
  testnum = int(sys.argv[2])
  if testnum == 1:
    pathend = 'text_device_xml'
  if testnum == 2:
    pathend = 'email_address_xml'
  if testnum == 3:
    pathend = 'no_friendlyname_xml'
  if testnum == 4:
    pathend = 'ssdp_device_xml'

  h = ThreadingHTTPServer(('', 0), HttpHandler)
  sn = h.socket.getsockname()
  port = sn[1]
  url = 'http://127.0.0.1:%d/%s' % (port, pathend)
  st = 'server type'
  uuid = 'uuid goes here'
  if testnum == 4:
    minissdpd_response[0] = [0]
  else:
    minissdpd_response[0] = [1]
    minissdpd_response[0].extend([len(url)] + list(url))
    minissdpd_response[0].extend([len(st)] + list(st))
    minissdpd_response[0].extend([len(uuid)] + list(uuid))
  notify_text[0] = notify_template % url

  h_thread = threading.Thread(target=h.serve_forever)
  h_thread.daemon = True
  h_thread.start()

  d = ThreadingUdpServer(('', 1900), UdpHandler)
  d.socket.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP,
                      socket.inet_pton(socket.AF_INET, '239.255.255.250') +
                      socket.inet_pton(socket.AF_INET, '0.0.0.0'))
  d_thread = threading.Thread(target=d.serve_forever)
  d_thread.daemon = True
  d_thread.start()

  u = SocketServer.UnixStreamServer(socketpath, UnixHandler)
  while keep_running[0]:
    u.handle_request()


if __name__ == '__main__':
  main()
