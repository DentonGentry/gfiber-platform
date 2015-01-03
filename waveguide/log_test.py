#!/usr/bin/python
import log
from wvtest import wvtest


@wvtest.wvtest
def AnonTest():
  m1 = '\x01\x02\x03\x04\x05\x06'
  m2 = '\x31\x32\x33\x34\x35\x36'

  s1 = log.AnonymizeMAC(None, m1)
  s2 = log.AnonymizeMAC(None, m2)
  a1a = log.AnonymizeMAC('key', m1)
  a2a = log.AnonymizeMAC('key', m2)
  a1b = log.AnonymizeMAC('key2', m1)
  a2b = log.AnonymizeMAC('key2', m2)

  # make sure they're printable strings
  wvtest.WVPASSEQ(s1, str(s1))
  wvtest.WVPASSEQ(a1a, str(a1a))
  wvtest.WVPASSEQ(a1b, str(a1b))

  # and reasonably sized
  wvtest.WVPASSLE(len(a1a), 8)

  # and change when the key or MAC changes
  wvtest.WVPASSNE(s1, s2)
  wvtest.WVPASSNE(a1a, a1b)
  wvtest.WVPASSNE(a2a, a2b)
  wvtest.WVPASSNE(a1a, a2a)
  wvtest.WVPASSNE(a1b, a2b)


if __name__ == '__main__':
  wvtest.wvtest_main()
