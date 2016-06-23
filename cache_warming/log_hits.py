#!/usr/bin/python
"""Updates most recent hit time and hit count for hosts in hits log.

Reads queries from dns_query_log.txt and updates hosts in hits log
dictionary with most recent hit time and hit count for each host.
Saves hits log dictionary as hits_log.json for future modification.
"""

import json
import os.path

DNS_QUERY_LOG_PATH = '/tmp/dns_query_log.txt'
HITS_LOG_JSON_PATH = '/tmp/hits_log.json'


def process_line(log, ln):
  """Processes a line of DNS query log and updates hits log.

  Parses line and updates most recent hit time and hit count
  for host in hits log.

  Args:
    log: Dictionary mapping host to tuple of hit count and most
         recent hit time.
    ln: String representing a line of DNS query log of the
          format '[Unix time] [host name]'.

  Returns:
    An updated dictionary mapping host to tuple of hit count and
    most recent hit time.
  """
  time, _, host = ln[:-1].partition(' ')
  if host in log:
    log[host] = (log[host][0] + 1, time)
  else:
    log[host] = (1, time)
  return log


def read_dns_query_log(path):
  """Reads a DNS query log.

  Processes each line of file, updating a hits log.

  Args:
    path: Path of DNS query log to be read.

  Returns:
    An updated dictionary mapping host to tuple of hit count and
    most recent hit time.
  """
  try:
    dns_query_log = open(path, 'r')
  except IOError:
    print 'unable to open ' + path
  else:
    log = {}
    for line in dns_query_log:
      log = process_line(log, line)
    dns_query_log.close()
    return log


def clear_dns_query_log(path):
  """Clears a DNS query log.

  Opens file for write without writing anything.

  Args:
    path: Path of DNS query log to be cleared.
  """
  try:
    open(path, 'w').close()
    return
  except IOError:
    print 'unable to open ' + path


def merge_logs(log, hist):
  """Merges two hit logs.

  Merges smaller hit log to larger hit log. Uses most recent hit
  time and sums hit count from each log for each host.

  Args:
    log: Dictionary mapping host to tuple of hit count and
         most recent hit time.
    hist: Similar dictionary representing previous query history.

  Returns:
    An updated dictionary mapping host to tuple of hit count and
    most recent hit time.
  """
  hist_larger = len(hist) > len(log)
  big_log, small_log = (hist, log) if hist_larger else (log, hist)
  for k, v in small_log.iteritems():
    if k in big_log:
      time = log[k][1]
      big_log[k] = (big_log[k][0] + v[0], time)
    else:
      big_log[k] = (v[0], v[1])
  return big_log


if __name__ == '__main__':
  hit_log = read_dns_query_log(DNS_QUERY_LOG_PATH)
  clear_dns_query_log(DNS_QUERY_LOG_PATH)
  if os.path.isfile(HITS_LOG_JSON_PATH):
    hist_json = open(HITS_LOG_JSON_PATH, 'r')
    hit_log_hist = json.load(hist_json)
    hist_json.close()

    hist_json = open(HITS_LOG_JSON_PATH, 'w')
    json.dump(merge_logs(hit_log, hit_log_hist), hist_json)
    hist_json.close()
  else:
    try:
      hist_json = open(HITS_LOG_JSON_PATH, 'w')
    except IOError:
      print 'unable to open ' + HITS_LOG_JSON_PATH
      raise
    else:
      json.dump(hit_log, hist_json)
      hist_json.close()
