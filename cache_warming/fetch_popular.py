#!/usr/bin/python
"""Pre-fetches top requested hosts.

Sorts dictionary represented in hit_log.json by number of hits
and sends DNS requests to a predetermined number of the top hosts.
"""

import argparse
import json
import dns.resolver

TOP_N = 50
HITS_LOG_JSON_PATH = '/tmp/hits_log.json'


def sort_hits_log(path):
  """Sorts hosts in hits log by number of hits.

  Args:
    path: Path of JSON representation of dictionary mapping host
          to tuple of most recent hit time and hit count.

  Returns:
    A list of hosts sorted by number of hits in descending order.
  """
  try:
    log_json = open(path, 'r')
  except IOError:
    print 'unable to open ' + path
    raise
  else:
    log = json.load(log_json)
    return sorted(log, key=log.get, reverse=True)


def prefetch(hosts, port, server):
  """Pre-fetches list of hosts.

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
    my_resolver.query(host)


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('-p', '--port', nargs='?', default=53, type=int,
                      help='port to which to send queries (default is 53).')
  parser.add_argument('-s', '--server', nargs='*', type=str,
                      help='alternate nameservers to query (default is None).')
  args = parser.parse_args()

  sorted_log = sort_hits_log(HITS_LOG_JSON_PATH)
  prefetch(sorted_log, args.port, args.server)
