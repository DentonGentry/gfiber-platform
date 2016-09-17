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
"""authorizer: processes Terms of Service acceptance for users."""

import logging
import subprocess
import sys
import time

import hash_mac_addr
import options

import tornado.escape
import tornado.httpclient
import tornado.ioloop
import tornado.netutil


optspec = """
authorizer [options...]
--
c,filter-chain= iptables chain to operate on [captive-portal-guests]
C,ca-certs=     path to CA certificates [/etc/ssl/certs/ca-certificates.crt]
d,dry-run       don't modify iptables
m,max-age=      oldest acceptance to consider as valid, in days [60]
n,nat-chain=    iptables NAT chain to operate on [captive-portal-guests-nat]
U,unix-path=    Unix socket to listen on [/tmp/authorizer.sock]
u,url=          URL to query for authentication [https://fiber-managed-wifi-tos.appspot.com/tos-accepted?id=%(mac)s]
"""

MAX_TRIES = 300

in_progress_users = {}
known_users = {}


def ip46tables(*args):
  if opt.dry_run:
    return 0

  x = subprocess.call(['iptables'] + list(args))
  y = subprocess.call(['ip6tables'] + list(args))
  return x | y


def is_valid_acceptance(response_obj):
  accepted_time = response_obj.get('accepted')
  return accepted_time + (opt.max_age * 86400) > time.time()


def allow_mac_rule(mac_addr):
  # iptables, unlike other Linux utilities, capitalizes MAC addresses
  return ('-m', 'mac', '--mac-source', mac_addr.upper(), '-j', 'ACCEPT')


class Checker(object):
  """Manage checking and polling for Terms of Service acceptance."""

  def __init__(self, mac_addr, url):
    self.mac_addr = mac_addr
    self.url = url % {'mac': hash_mac_addr.hash_mac_addr(self.mac_addr)}
    self.tries = 0
    self.callback = None

  def check(self):
    """Check if a remote service knows about a device with a supplied MAC."""
    logging.info('Checking TOS for %s', self.mac_addr)
    http_client = tornado.httpclient.HTTPClient()
    self.tries += 1

    try:
      response = http_client.fetch(self.url, ca_certs=opt.ca_certs)
      response_obj = tornado.escape.json_decode(response.body)
      valid = is_valid_acceptance(response_obj)
    except tornado.httpclient.HTTPError as e:
      logging.warning('Error checking authorization: %r', e)
      valid = False

    if valid:
      logging.info('TOS accepted for %s', self.mac_addr)

      known_users[self.mac_addr] = response_obj
      result = ip46tables('-A', opt.filter_chain,
                          *allow_mac_rule(self.mac_addr))
      result |= ip46tables('-t', 'nat', '-A', opt.nat_chain,
                           *allow_mac_rule(self.mac_addr))
      if result:
        logging.error('Could not update firewall for device %s',
                      self.mac_addr)

    if valid or self.tries > MAX_TRIES:
      if self.callback:
        self.callback.stop()
      if self.mac_addr in in_progress_users:
        del in_progress_users[self.mac_addr]
    else:
      in_progress_users[self.mac_addr] = self
      self.poll()

    return response

  def poll(self):
    if not self.callback:
      self.callback = tornado.ioloop.PeriodicCallback(self.check, 1000)
      self.callback.start()


def accept(connection, unused_address):
  """Accept a MAC address and find out if it's authorized."""
  cf = connection.makefile()

  maybe_mac_addr = cf.readline().strip()
  try:
    mac_addr = hash_mac_addr.normalize_mac_addr(maybe_mac_addr)
  except ValueError:
    logging.warning('can only check authorization for a MAC address.')
    cf.write('{}')
    return

  if mac_addr in known_users:
    cached_response = known_users[mac_addr]
    if is_valid_acceptance(cached_response):
      logging.info('TOS accepted (cached) for %s', mac_addr)
      cached_response['cached'] = True
      cf.write(tornado.escape.json_encode(cached_response))
      return

  if mac_addr in in_progress_users:
    checker = in_progress_users[mac_addr]
  else:
    checker = Checker(mac_addr, opt.url)

  response = checker.check()
  cf.write(response.body)


def expire_cache():
  """Remove users whose authorization has expired from the cache."""
  expired_users = set(mac_addr for mac_addr, cached_response
                      in known_users.items()
                      if not is_valid_acceptance(cached_response))

  for mac_addr in expired_users:
    logging.info('Removing expired user %s', mac_addr)
    del known_users[mac_addr]

    result = ip46tables('-D', opt.filter_chain, *allow_mac_rule(mac_addr))
    result |= ip46tables('-t', 'nat', '-D', opt.nat_chain,
                         *allow_mac_rule(mac_addr))
    if result:
      logging.warning('Error removing expired user %s !', mac_addr)


if __name__ == '__main__':
  o = options.Options(optspec)
  opt, flags, extra = o.parse(sys.argv[1:])

  if not opt.unix_path:
    o.fatal('unix-path is required\n')

  if not (opt.filter_chain and opt.nat_chain) and not opt.dry_run:
    o.fatal('(filter-chain and nat-chain) or dry-run is required\n')

  # work whether or not Tornado has configured the root logger already
  logging.basicConfig(level=logging.INFO)
  logging.getLogger().setLevel(logging.INFO)

  ip46tables('-F', opt.filter_chain)
  ip46tables('-t', 'nat', '-F', opt.nat_chain)

  sock = tornado.netutil.bind_unix_socket(opt.unix_path)
  ioloop = tornado.ioloop.IOLoop.instance()
  tornado.netutil.add_accept_handler(sock, accept, ioloop)

  logging.info('Started authorizer.')
  ioloop.start()

  expirer = tornado.ioloop.PeriodicCallback(expire_cache, 60 * 60 * 1000)
  expirer.start()

