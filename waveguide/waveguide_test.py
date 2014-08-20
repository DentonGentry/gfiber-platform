#!/usr/bin/python
import waveguide
from wvtest import wvtest


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


if __name__ == '__main__':
  wvtest.wvtest_main()
