#!/usr/bin/python -S

"""Tests for autochannel.py."""


import autochannel
from wvtest import wvtest


@wvtest.wvtest
def get_permitted_frequencies_test():
  wvtest.WVPASSEQ('2412 2432 2462',
                  autochannel.get_permitted_frequencies('2.4', 'ANY', '20'))
  wvtest.WVPASSEQ('5180 5745 5260 5500',
                  autochannel.get_permitted_frequencies('5', 'ANY', '80'))

  for case in [(2.4, 'ANY', '80'),
               ('2.4', 'LOW', '80'),
               ('5', 'OVERLAP', '20'),
               ('60', 'INVALID_AUTOTYPE', '3.14'),
               (1, 2, 3)]:
    wvtest.WVEXCEPT(ValueError, autochannel.get_permitted_frequencies, *case)


if __name__ == '__main__':
  wvtest.wvtest_main()
