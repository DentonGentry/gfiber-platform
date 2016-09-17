#!/usr/bin/python
# Copyright 2016 Google Inc. All Rights Reserved.
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
"""Redirects all HTTP requests to the specified URL."""

import logging
import socket
import subprocess
import sys
import urllib2

import hash_mac_addr
import options

import tornado.httpclient
import tornado.ioloop
import tornado.web


optspec = """
http_bouncer [options...]
--
p,port=      TCP port to listen on [8888]
u,url=       URL to redirect ("bounce") users to. Include the format specifier %(mac)s to write the users' MAC address into the URL when bouncing. []
U,unix-path= Unix socket to use for authorization checking [/tmp/authorizer.sock]
"""

PKI_HOSTS = set(['pki.google.com', 'clients1.google.com'])


def mac_for_ip(remote_ip):
  arp_response = subprocess.check_output(['arp', remote_ip])
  return arp_response.split()[3]


class Redirector(tornado.web.RequestHandler):
  """Redirect users' HTTP connections to a captive portal landing page."""

  def initialize(self, substitute_mac):
    self.substitute_mac = substitute_mac
    self._http_client = tornado.httpclient.HTTPClient()

  def get(self):
    if self._is_crl_request():
      # proxy CRL/OCSP requests. Workaround for b/19825798.
      url = '%s://%s%s' % (self.request.protocol, self.request.host,
                           self.request.uri)
      logging.info('Forwarding request to %s', url)
      response = self._http_client.fetch(url)
      for (name, value) in response.headers.get_all():
        self.set_header(name, value)

      if response.body:
        self.set_header('Content-Length', len(response.body))
        self.write(response.body)
    else:
      if self.substitute_mac:
        mac = mac_for_ip(self.request.remote_ip)
        self.redirect(opt.url % {'mac': hash_mac_addr.hash_mac_addr(mac)})

        if opt.unix_path:
          try:
            s = socket.socket(socket.AF_UNIX)
            s.connect(opt.unix_path)
            s.sendall('%s\n' % mac)
            s.close()
          except socket.error:
            logging.warning('Could not contact authorizer.')
      else:
        self.redirect(opt.url)

  def _is_crl_request(self):
    uri = self.request.uri
    return self.request.host in PKI_HOSTS and (uri.startswith('/ocsp/')
                                               or uri.endswith('.crl'))

if __name__ == '__main__':
  o = options.Options(optspec)
  opt, flags, extra = o.parse(sys.argv[1:])

  if not opt.port or not opt.url:
    o.fatal('port and url are required\n')

  # work whether or not Tornado has configured the root logger already
  logging.basicConfig(level=logging.INFO)
  logging.getLogger().setLevel(logging.INFO)

  try:
    formatted_url = opt.url % {'mac': '00:00:00:00:00:00'}
    urllib2.urlopen(formatted_url).getcode()
  except (TypeError, ValueError, urllib2.URLError):
    o.fatal('url must be a URL.')

  url_needs_mac = formatted_url != opt.url
  if url_needs_mac and not opt.unix_path:
    o.fatal('unix-path missing but URL requested MAC-based authorization')

  application = tornado.web.Application([
      (r'.*', Redirector, dict(substitute_mac=url_needs_mac)),
  ])

  try:
    application.listen(opt.port)
  except socket.gaierror:
    o.fatal('port must be a TCP port, and we must be able to bind it.')

  logging.info('Starting http_bouncer.')
  tornado.ioloop.IOLoop.instance().start()
