#!/usr/bin/python
"""Tests for cache_warming.py."""

import os
import cache_warming
from wvtest import wvtest


@wvtest.wvtest
def test_process_query_first_hit():
  qry = '123456789 www.yahoo.com'
  expected = {'www.yahoo.com': (1, '123456789')}
  cache_warming.hit_log = {}
  cache_warming.process_query(qry)
  actual = cache_warming.hit_log
  wvtest.WVPASSEQ(actual, expected)


@wvtest.wvtest
def test_process_query_update_hit_count():
  qry = '123456789 www.yahoo.com'
  cache_warming.hit_log = {'www.yahoo.com': (1, '123456789')}
  cache_warming.process_query(qry)
  expected = 2
  actual = cache_warming.hit_log['www.yahoo.com'][0]
  wvtest.WVPASSEQ(actual, expected)


@wvtest.wvtest
def test_process_query_update_recent_hit_time():
  qry = '123456789 www.yahoo.com'
  cache_warming.hit_log = {'www.yahoo.com': (1, '987654321')}
  cache_warming.process_query(qry)
  expected = '123456789'
  actual = cache_warming.hit_log['www.yahoo.com'][1]
  wvtest.WVPASSEQ(actual, expected)


@wvtest.wvtest
def test_sort_hit_log_empty():
  cache_warming.hit_log = {}
  expected = []
  actual = cache_warming.sort_hit_log()
  wvtest.WVPASSEQ(actual, expected)


@wvtest.wvtest
def test_sort_hit_log_non_empty():
  cache_warming.hit_log = {
      'www.google.com': (2, '123456789'),
      'www.yahoo.com': (1, '987654321'),
      'www.espn.com': (3, '135792468')
  }
  expected = ['www.espn.com', 'www.google.com', 'www.yahoo.com']
  actual = cache_warming.sort_hit_log()
  wvtest.WVPASSEQ(actual, expected)


@wvtest.wvtest
def test_hit_log_subset():
  hosts = ['www.google.com', 'www.yahoo.com']
  cache_warming.hit_log = cache_warming.hit_log = {
      'www.youtube.com': (4, '987654321'),
      'www.google.com': (1, '987654321'),
      'www.espn.com': (3, '123456789'),
      'www.yahoo.com': (2, '135792468')
  }
  expected = {
      'www.yahoo.com': (2, '135792468'),
      'www.google.com': (1, '987654321')
  }
  actual = cache_warming.hit_log_subset(hosts)
  wvtest.WVPASSEQ(actual, expected)


@wvtest.wvtest
def test_no_cache_warming_hosts():
  crash = False
  cache_warming.HOSTS_JSON_PATH = '/tmp/cache_warming_hosts.json'
  if os.path.exists('/tmp/cache_warming_hosts.json'):
    os.remove('/tmp/cache_warming_hosts.json')
  try:
    cache_warming.load_hosts()
    cache_warming.warm_cache(53, None)
  except ValueError:
    crash = True
  wvtest.WVFAIL(crash)


@wvtest.wvtest
def test_empty_cache_warming_hosts():
  crash = False
  cache_warming.HOSTS_JSON_PATH = '/tmp/cache_warming_hosts.json'
  if os.path.exists('/tmp/cache_warming_hosts.json'):
    os.remove('/tmp/cache_warming_hosts.json')
  open('/tmp/cache_warming_hosts.json', 'w').close()
  try:
    cache_warming.load_hosts()
    cache_warming.warm_cache(53, None)
  except ValueError:
    crash = True
  finally:
    os.remove('/tmp/cache_warming_hosts.json')
  wvtest.WVFAIL(crash)


@wvtest.wvtest
def test_wrong_cache_warming_hosts():
  crash = False
  cache_warming.HOSTS_JSON_PATH = '/tmp/cache_warming_hosts.json'
  if os.path.exists('/tmp/cache_warming_hosts.json'):
    os.remove('/tmp/cache_warming_hosts.json')
  f = open('/tmp/cache_warming_hosts.json', 'w')
  f.write('[]')
  f.close()
  try:
    cache_warming.load_hosts()
    cache_warming.warm_cache(53, None)
  except ValueError:
    crash = True
  finally:
    os.remove('/tmp/cache_warming_hosts.json')
  wvtest.WVFAIL(crash)


if __name__ == '__main__':
  wvtest.wvtest_main()
