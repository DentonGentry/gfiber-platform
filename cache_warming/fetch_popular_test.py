#!/usr/bin/python
"""Tests for fetch_popular.py."""

import os
import fetch_popular
from wvtest import wvtest


@wvtest.wvtest
def testSortHitsLog_empty():
  try:
    file_name = 'test_log.json'
    with open(file_name, 'w') as f:
      f.write('{}')

    expected = []
    actual = fetch_popular.sort_hits_log(file_name)
    wvtest.WVPASSEQ(actual, expected)
  finally:
    os.remove(file_name)


@wvtest.wvtest
def testSortHitsLog_nonEmpty():
  try:
    file_name = 'test_log.json'
    with open(file_name, 'w') as f:
      f.write('{"www.google.com": [2, "123456789"], "www.yahoo.com":'
      	       ' [1,"987654321"], "www.espn.com": [3, "135792468"]}')

    expected = ['www.espn.com', 'www.google.com', 'www.yahoo.com']
    actual = fetch_popular.sort_hits_log(file_name)
    wvtest.WVPASSEQ(actual, expected)
  finally:
    os.remove(file_name)


if __name__ == '__main__':
  wvtest.wvtest_main()
