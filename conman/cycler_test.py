#!/usr/bin/python

"""Tests for cycler.py."""

import time

import cycler
from wvtest import wvtest


@wvtest.wvtest
def cycler_test():
  c = cycler.AgingPriorityCycler()
  wvtest.WVPASS(c.next() is None)

  cycle_length_s = .01
  c = cycler.AgingPriorityCycler(cycle_length_s=cycle_length_s,
                                 items=(('A', 10), ('B', 5), ('C', 1)))

  # We should get all three in order, since they all have the same insertion
  # time.  They will all get slightly different insertion times, but next()
  # should be fast enough that the differences don't make much difference.
  wvtest.WVPASSEQ(c.peek(), 'A')
  wvtest.WVPASSEQ(c.next(), 'A')
  wvtest.WVPASSEQ(c.next(), 'B')
  wvtest.WVPASSEQ(c.next(), 'C')
  wvtest.WVPASS(c.peek() is None)
  wvtest.WVPASS(c.next() is None)
  wvtest.WVPASS(c.next() is None)

  # Now, wait for items to be ready again and just cycle one of them.
  time.sleep(cycle_length_s)
  wvtest.WVPASSEQ(c.next(), 'A')

  # Now, if we wait 1.9 cycles, the aged priorities will be as follows:
  # A: 0.9 * 10 = 9
  # B: 1.9 * 5 = 9.5
  # C: 1.9 * 1 = 1.9
  time.sleep(cycle_length_s * 1.9)
  wvtest.WVPASSEQ(c.next(), 'B')
  wvtest.WVPASSEQ(c.next(), 'A')
  wvtest.WVPASSEQ(c.next(), 'C')

  # Update c, keeping A as-is, removing B, updating C's priority, and adding D.
  # Sleep for two cycles.  After the first cycle, D has priority 20 and A and C
  # have priority 0 (since we just cycled them).  After the second cycle, the
  # priorities are as follows:
  # A: 1 * 10 = 10
  # C: 1 * 20 = 20
  # D: 2 * 20 = 40
  c.update((('A', 10), ('C', 20), ('D', 20)))
  time.sleep(cycle_length_s * 2)
  wvtest.WVPASSEQ(c.next(), 'D')
  wvtest.WVPASSEQ(c.next(), 'C')
  wvtest.WVPASSEQ(c.next(), 'A')
  wvtest.WVPASS(c.next() is None)

if __name__ == '__main__':
  wvtest.wvtest_main()
