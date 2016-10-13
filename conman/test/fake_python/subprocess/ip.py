#!/usr/bin/python

"""Fake ip route implementation."""

import logging
import socket
import struct

import ifup


_ROUTING_TABLE = {}
_IP_TABLE = {}


def call(subcommand, *args):
  """Fake ip command."""
  subcommands = {
      'route': _ip_route,
      'addr': _ip_addr,
      'link': _link,
  }

  if subcommand not in subcommands:
    return 1, 'ip subcommand %r not supported' % subcommand

  return subcommands[subcommand](args)


def register_testonly(interface):
  if interface not in _IP_TABLE:
    _IP_TABLE[interface] = set()


def _ip_route(args):
  def can_add_route(dev):
    def ip_to_int(ip_addr):
      return struct.unpack('!I', socket.inet_pton(socket.AF_INET, ip_addr))[0]

    if args[1] != 'default':
      return True

    via = ip_to_int(args[args.index('via') + 1])
    for (ifc, route, _), _ in _ROUTING_TABLE.iteritems():
      if ifc != dev:
        continue

      netmask = 0
      if '/' in route:
        route, netmask = route.split('/')
        netmask = 32 - int(netmask)
      route = ip_to_int(route)

      if (route >> netmask) == (via >> netmask):
        return True

    return False

  if not args:
    return 0, '\n'.join(_ROUTING_TABLE.values())

  if 'dev' not in args:
    raise Exception('fake ip route got no dev')

  dev = args[args.index('dev') + 1]

  metric = None
  if 'metric' in args:
    metric = args[args.index('metric') + 1]
  if args[0] in ('add', 'del'):
    route = args[1]
  key = (dev, route, metric)
  if args[0] == 'add' and key not in _ROUTING_TABLE:
    if not can_add_route(dev):
      return (1, 'Tried to add default route without subnet route: %r' %
              _ROUTING_TABLE)
    logging.debug('Adding route for %r', key)
    _ROUTING_TABLE[key] = ' '.join(args[1:])
  elif args[0] == 'del':
    if key in _ROUTING_TABLE:
      logging.debug('Deleting route for %r', key)
      del _ROUTING_TABLE[key]
    elif key[2] is None:
      # pylint: disable=g-builtin-op
      for k in _ROUTING_TABLE.keys():
        if k[:-1] == key[:-1]:
          logging.debug('Deleting route for %r (generalized from %s)', k, key)
          del _ROUTING_TABLE[k]
          break

  return 0, ''


# pylint: disable=line-too-long
_IP_ADDR_SHOW_TPL = """4: {name}: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP qlen 1000
    link/ether fe:fb:01:80:1b:74 brd ff:ff:ff:ff:ff:ff
{ips}
"""

_IP_ADDR_SHOW_IP_TPL = """    inet {ip}/24 brd 100.100.255.255 scope global {name}
       valid_lft forever preferred_lft forever
"""


def _ip_addr(args):
  if 'dev' not in args:
    raise Exception('fake ip addr show got no dev')

  dev = args[args.index('dev') + 1]
  if dev not in _IP_TABLE:
    return 255, 'Device "%r" does not exist' % dev

  if 'show' in args:
    ips = '\n'.join(_IP_ADDR_SHOW_IP_TPL.format(name=dev, ip=addr)
                    for addr in _IP_TABLE[dev])
    return 0, _IP_ADDR_SHOW_TPL.format(name=dev, ips=ips)

  if 'add' in args:
    add = args[args.index('add') + 1]
    _IP_TABLE[dev].add(add)
    return 0, ''

  if 'del' in args:
    remove = args[args.index('del') + 1]
    if remove in _IP_TABLE[dev]:
      _IP_TABLE[dev].remove(remove)
      return 0, ''
    return 254, 'RTNETLINK answers: Cannot assign requested address'

  raise Exception('no recognized ip addr command in %r' % args)


def _link(args):
  return 0, '\n'.join('%s LOWER_UP' %  interface
                      for interface, state in ifup.INTERFACE_STATE.iteritems()
                      if state)
