#!/usr/bin/python
"""Updates hit log and periodically fetches top requested hosts.

Loads previousy saved top requested hosts. Reads DNS queries
through socket and updates hit log dictionary with most recent
hit time and hit count for each host. Periodically sorts hosts
in hit log by number of hits, fetches a predetermined number of
the top requested hosts, and saves fetched hosts.
"""

import argparse
from datetime import datetime
import json
import os
import socket
import sys
import dns.exception
import dns.resolver

hit_log = {}
last_fetch = datetime.min
verbose = False
TOP_N = 50
FETCH_INTERVAL = 60  # seconds
UDP_SERVER_PATH = '/tmp/dns_query_log_socket'
HOSTS_JSON_PATH = '/config/cache_warming_hosts.json'


def save_hosts(log):
  """Saves predetermined number of top requested hosts in json file.

  Stores dictionary with host key and tuple value containing most recent hit
  time and hit count.

  Args:
    log: Dictionary of top requested hosts with host key and tuple value
         containing most recent hit time and hit count.
  """
  if verbose:
    print 'Saving hosts in %s.' % HOSTS_JSON_PATH
  d = os.path.dirname(HOSTS_JSON_PATH)
  if not os.path.exists(d):
    os.makedirs(d)
  with open(HOSTS_JSON_PATH, 'w') as hosts_json:
    json.dump(log, hosts_json)


def load_hosts():
  """Loads hosts stored in json file.

  Loads dictionary with host key and tuple value containing most recent hit
  time and hit count as hit_log if it exists.
  """
  if verbose:
    print 'Loading hosts from %s.' % HOSTS_JSON_PATH
  if os.path.isfile(HOSTS_JSON_PATH):
    with open(HOSTS_JSON_PATH, 'r') as hosts_json:
      global hit_log
      try:
        hit_log = json.load(hosts_json)
      except ValueError as e:
        if verbose:
          print 'Failed to open %s: %s.' % (HOSTS_JSON_PATH, e)
      finally:
        if not isinstance(hit_log, dict):
          hit_log = {}


def process_query(qry):
  """Processes DNS query and updates hit log.

  Parses DNS query and updates requested host's hit count and
  most recent hit time in hit log.

  Args:
    qry: String representing a DNS query of the format
            '[Unix time] [host name]'.
  """
  time, _, host = qry.partition(' ')
  if verbose:
    print 'Received query for %s.' % host
  if host in hit_log:
    hit_log[host] = (hit_log[host][0] + 1, time)
  else:
    hit_log[host] = (1, time)


def hit_log_subset(hosts):
  """Makes a subset of hit log containing selected hosts.

  Args:
    hosts: List of hosts to be included in the subset of hit log.

  Returns:
    Dictionary of selected hosts with host key and tuple value
    containing most recent hit time and hit count.
  """
  return {k: hit_log[k] for k in hosts}


def fetch(hosts, port, server):
  """Fetches hosts and saves subset of hit log containing fetched hosts.

  Only fetches predetermined number of top requested hosts. If fetch
  fails, host is removed from hit log and saved hosts list.

  Args:
    hosts: List of hosts to be fetched sorted by number of hits
           in descending order.
    port: Port to which to send queries (default is 53).
    server: Alternate nameservers to query (default is None).
  """
  my_resolver = dns.resolver.Resolver()
  my_resolver.port = port
  if server is not None:
    my_resolver.nameservers = server

  if len(hosts) > TOP_N:
    hosts = hosts[:TOP_N]
  for host in hosts:
    if verbose:
      print 'Fetching %s.' % host
    try:
      my_resolver.query(host)
    except dns.exception.DNSException:
      if verbose:
        print 'Failed to fetch %s.' % host
      del hit_log[host]
      hosts.remove(host)

  save_hosts(hit_log_subset(hosts))


def sort_hit_log():
  """Sorts hosts in hit log by number of hits.

  Returns:
    A list of hosts sorted by number of hits in descending order.
  """
  return sorted(hit_log, key=hit_log.get, reverse=True)


def warm_cache(port, server):
  """Warms cache with predetermined number of most requested hosts.

  Sorts hosts in hit log by hit count, fetches predetermined
  number of top requested hosts, updates last fetch time.

  Args:
    port: Port to which to send queries (default is 53).
    server: Alternate nameservers to query (default is None).
  """
  if verbose:
    print 'Warming cache.'
  sorted_hosts = sort_hit_log()
  fetch(sorted_hosts, port, server)
  global last_fetch
  last_fetch = datetime.now()


def set_args():
  """Sets arguments for script.

  Returns:
    List of arguments containing port and server.
  """
  parser = argparse.ArgumentParser()
  parser.add_argument('-p', '--port', nargs='?', default=53, type=int,
                      help='port to which to send queries (default is 53).')
  parser.add_argument('-s', '--server', nargs='*', type=str,
                      help='alternate nameservers to query (default is None).')
  parser.add_argument('-v', '--verbose', action='store_true')
  return parser.parse_args()


if __name__ == '__main__':
  sys.stdout = os.fdopen(1, 'w', 1)
  sys.stderr = os.fdopen(2, 'w', 1)
  args = set_args()
  verbose = args.verbose
  load_hosts()
  server_address = UDP_SERVER_PATH
  try:
    os.remove(server_address)
  except OSError:
    if os.path.exists(server_address):
      raise
  sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
  sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
  sock.bind(server_address)
  os.chmod(server_address, 0o777)
  if verbose:
    print 'Set up socket at %s.' % HOSTS_JSON_PATH

  while 1:
    diff = datetime.now() - last_fetch
    if diff.total_seconds() > FETCH_INTERVAL:
      warm_cache(args.port, args.server)
    data = sock.recv(128)
    process_query(data)
