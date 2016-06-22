#!/usr/bin/python
"""Tests for log_hits.py."""

import os
import log_hits
from wvtest import wvtest


@wvtest.wvtest
def testProcessLine_firstHit():
  line = '123456789 www.yahoo.com\n'
  expected = {'www.yahoo.com': (1, '123456789')}
  actual = log_hits.process_line({}, line)
  wvtest.WVPASSEQ(actual, expected)


@wvtest.wvtest
def testProcessLine_updateHitCount():
  line = '123456789 www.yahoo.com\n'
  log = {'www.yahoo.com': (1, '123456789')}
  expected = 2
  actual = log_hits.process_line(log, line)['www.yahoo.com'][0]
  wvtest.WVPASSEQ(actual, expected)


@wvtest.wvtest
def testProcessLine_updateRecentHitTime():
  line = '123456789 www.yahoo.com\n'
  log = {'www.yahoo.com': (1, '987654321')}
  expected = '123456789'
  actual = log_hits.process_line(log, line)['www.yahoo.com'][1]
  wvtest.WVPASSEQ(actual, expected)


@wvtest.wvtest
def testMergeLogs_emptyLog():
  hist = {'www.yahoo.com': (1, '123456789')}
  expected = hist
  actual = log_hits.merge_logs({}, hist)
  wvtest.WVPASSEQ(actual, expected)


@wvtest.wvtest
def testMergeLogs_emptyHist():
  log = {'www.yahoo.com': (1, '123456789')}
  expected = log
  actual = log_hits.merge_logs(log, {})
  wvtest.WVPASSEQ(actual, expected)


@wvtest.wvtest
def testMergeLogs_bothEmpty():
  expected = {}
  actual = log_hits.merge_logs({}, {})
  wvtest.WVPASSEQ(actual, expected)


@wvtest.wvtest
def testMergeLogs_noOverlap():
  log = {'www.yahoo.com': (1, '123456789')}
  hist = {'www.google.com': (1, '123456789')}
  expected = {
      'www.yahoo.com': (1, '123456789'),
      'www.google.com': (1, '123456789')
  }
  actual = log_hits.merge_logs(log, hist)
  wvtest.WVPASSEQ(actual, expected)


@wvtest.wvtest
def testMergeLogs_updateHitCount():
  log = {'www.yahoo.com': (1, '987654321')}
  hist = {'www.yahoo.com': (1, '123456789')}
  expected = 2
  actual = log_hits.merge_logs(log, hist)['www.yahoo.com'][0]
  wvtest.WVPASSEQ(actual, expected)


@wvtest.wvtest
def testMergeLogs_updateRecentHitTime():
  log = {'www.yahoo.com': (1, '987654321')}
  hist = {'www.yahoo.com': (1, '123456789')}
  expected = '987654321'
  actual = log_hits.merge_logs(log, hist)['www.yahoo.com'][1]
  wvtest.WVPASSEQ(actual, expected)


@wvtest.wvtest
def testMergeLogs_histLargerNoOverlap():
  log = {'www.yahoo.com': (1, '123456789')}
  hist = {
      'www.google.com': (1, '123456789'),
      'www.espn.com': (1, '123456789')
  }
  expected = {
      'www.yahoo.com': (1, '123456789'),
      'www.google.com': (1, '123456789'),
      'www.espn.com': (1, '123456789')
  }
  actual = log_hits.merge_logs(log, hist)
  wvtest.WVPASSEQ(actual, expected)


@wvtest.wvtest
def testMergeLogs_histLargerUpdateHitCount():
  log = {'www.yahoo.com': (1, '987654321')}
  hist = {
      'www.yahoo.com': (1, '123456789'),
      'www.google.com': (1, '123456789')
  }
  expected = 2
  actual = log_hits.merge_logs(log, hist)['www.yahoo.com'][0]
  wvtest.WVPASSEQ(actual, expected)


@wvtest.wvtest
def testMergeLogs_histLargerUpdateRecentHitTime():
  log = {'www.yahoo.com': (1, '987654321')}
  hist = {
      'www.yahoo.com': (1, '123456789'),
      'www.google.com': (1, '123456789')
  }
  expected = '987654321'
  actual = log_hits.merge_logs(log, hist)['www.yahoo.com'][1]
  wvtest.WVPASSEQ(actual, expected)


@wvtest.wvtest
def testReadDNSQueryLog_empty():
  file_name = 'test_log.txt'
  open(file_name, 'w').close()
  expected = {}
  actual = log_hits.read_dns_query_log(file_name)
  wvtest.WVPASSEQ(actual, expected)
  os.remove(file_name)


@wvtest.wvtest
def testReadDNSQueryLog_nonEmpty():
  file_name = 'test_log.txt'
  f = open(file_name, 'w')
  f.write('123456789 www.yahoo.com\n987654321 www.google.com\n'
          '135792468 www.yahoo.com\n')
  f.close()
  expected = {
      'www.yahoo.com': (2, '135792468'),
      'www.google.com': (1, '987654321')
  }
  actual = log_hits.read_dns_query_log(file_name)
  wvtest.WVPASSEQ(actual, expected)
  os.remove(file_name)


@wvtest.wvtest
def testClearDNSQueryLog():
  file_name = 'test_log.txt'
  f = open(file_name, 'w')
  f.write('testing clear_dns_query_log()\n')
  f.close()

  log_hits.clear_dns_query_log(file_name)
  expected = 0
  actual = os.stat(file_name).st_size
  wvtest.WVPASSEQ(actual, expected)
  os.remove(file_name)


if __name__ == '__main__':
  wvtest.wvtest_main()
