#!/usr/bin/python
import waveguide
from wvtest import wvtest


class Empty(object):
  pass


@wvtest.wvtest
def EaterTest():
  e = waveguide.Eater('abcdefg')
  wvtest.WVPASSEQ(e.Eat(2), 'ab')
  wvtest.WVEXCEPT(waveguide.DecodeError, e.Eat, 10)
  wvtest.WVPASSEQ(e.Unpack('!3s'), ('cde',))
  wvtest.WVPASSEQ(e.Remainder(), 'fg')
  wvtest.WVPASSEQ(e.Remainder(), '')

  e = waveguide.Eater('\x01\x02\x03\x04\x05\x06')
  wvtest.WVPASSEQ(list(e.Iter('!H', 4)), [(0x0102,), (0x0304,)])
  wvtest.WVPASSEQ(list(e.Iter('!B', 1)), [(0x05,)])
  wvtest.WVEXCEPT(waveguide.DecodeError, lambda: list(e.Iter('!B', 2)))
  wvtest.WVPASSEQ(e.Remainder(), '\x06')


@wvtest.wvtest
def AnonTest():
  waveguide.opt = Empty()
  waveguide.opt.anonymize = True
  m1 = '\x01\x02\x03\x04\x05\x06'
  m2 = '\x31\x32\x33\x34\x35\x36'

  s1 = waveguide.AnonymizeMAC(None, m1)
  s2 = waveguide.AnonymizeMAC(None, m2)
  a1a = waveguide.AnonymizeMAC('key', m1)
  a2a = waveguide.AnonymizeMAC('key', m2)
  a1b = waveguide.AnonymizeMAC('key2', m1)
  a2b = waveguide.AnonymizeMAC('key2', m2)

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
