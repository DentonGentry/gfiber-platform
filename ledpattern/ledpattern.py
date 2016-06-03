#!/usr/bin/python

"""Blinks a specific LED pattern read from a simple pattern file.

The first value is the state that the LED pattern represents. Followed by
any combination of the following values:

  R = Red blink
  B = Blue blink
  P = Purple blink

An example pattern file might look like:

 echo "blink,R,R,R" > /tmp/test.pat

Invoking "ledpattern.py /tmp/test.pat blink" would result in the red LED
blinking three times.
"""

__author__ = 'Chris Gibson <cgibson@google.com>'

import csv
import os
import sys
import time

# Unit tests can override these values.
DISABLE_GPIOMAILBOX = '/tmp/gpio/disable'
SYSFS_RED_BRIGHTNESS = '/sys/class/leds/sys-red/brightness'
SYSFS_BLUE_BRIGHTNESS = '/sys/class/leds/sys-blue/brightness'
SLEEP_TIMEOUT = 0.5


class LedPattern(object):
  """Read, parse, and blink LEDs based on a pattern from a file."""

  def ReadCsvPatternFile(self, pattern_file, state):
    """Read a CSV pattern file."""
    if not os.path.exists(pattern_file):
      print 'Error: Pattern file: "%s" not found.' % pattern_file
      return None
    if not state:
      print 'Error: led state cannot be empty.'
      return None
    try:
      with open(pattern_file, 'r') as f:
        patterns = csv.reader(f, delimiter=',')
        for row in patterns:
          if row[0] == state:
            return [c for c in row[1:] if c in ['R', 'B', 'P']]
      print ('Error: Could not find led state: "%s" in pattern file: %s'
             % (state, pattern_file))
      return None
    except (IOError, OSError) as ex:
      print ('Failed to open the pattern file: %s, error: %s.'
             % (pattern_file, ex))
      return None

  def SetRedBrightness(self, level):
    with open(SYSFS_RED_BRIGHTNESS, 'w') as f:
      f.write(level)

  def SetBlueBrightness(self, level):
    with open(SYSFS_BLUE_BRIGHTNESS, 'w') as f:
      f.write(level)

  def SetLedsOff(self):
    self.SetRedBrightness('0')
    self.SetBlueBrightness('0')

  def RedBlink(self):
    self.SetLedsOff()
    time.sleep(SLEEP_TIMEOUT)
    self.SetRedBrightness('100')
    self.SetBlueBrightness('0')
    time.sleep(SLEEP_TIMEOUT)
    self.SetLedsOff()

  def BlueBlink(self):
    self.SetLedsOff()
    time.sleep(SLEEP_TIMEOUT)
    self.SetRedBrightness('0')
    self.SetBlueBrightness('100')
    time.sleep(SLEEP_TIMEOUT)
    self.SetLedsOff()

  def PurpleBlink(self):
    self.SetLedsOff()
    time.sleep(SLEEP_TIMEOUT)
    self.SetRedBrightness('100')
    self.SetBlueBrightness('100')
    time.sleep(SLEEP_TIMEOUT)
    self.SetLedsOff()

  def PlayPattern(self, pattern):
    for color in pattern:
      if color == 'R':
        self.RedBlink()
      elif color == 'B':
        self.BlueBlink()
      elif color == 'P':
        self.PurpleBlink()

  def Run(self, pattern_file, state):
    """Sets up an LED pattern to play.

    Arguments:
      pattern_file: Pattern file containing a list of LED states/patterns.
      state: Name of the LED state to play.

    Returns:
      An integer exit code: 0 means everything succeeded. Non-zero exit codes
      mean something went wrong.
    """
    try:
      open(DISABLE_GPIOMAILBOX, 'w').close()
    except (IOError, OSError) as ex:
      # If we can't disable gpio-mailbox then we can't guarantee control
      # over the LEDs, so we just have to admit defeat.
      print 'Error: Failed to disable gpio-mailbox! %s' % ex
      return 1

    try:
      pattern = self.ReadCsvPatternFile(pattern_file, state)
      if not pattern:
        print 'Reading pattern failed! Exiting!'
        return 1
    except (IOError, OSError) as ex:
      print 'Error: Failed to read pattern file, %s' % ex
      return 1

    # Depending on what state the LEDs are in when we touched the gpio-mailbox
    # disable file, the LEDs will remain in that last state. Firstly, turn both
    # LEDs off then sleep for a second to indicate that the pattern is about
    # to begin.
    self.SetLedsOff()
    time.sleep(1)

    self.PlayPattern(pattern)

    # Turn off the LEDs and sleep for a second to clearly delineate the end of
    # the current pattern.
    self.SetLedsOff()
    time.sleep(1)

    os.unlink(DISABLE_GPIOMAILBOX)
    return 0


def Usage():
  print 'Usage:'
  print '  %s {pattern file} {state}' % sys.argv[0]
  print
  print '    pattern file: path to a pattern file'
  print '    state: the LED state to select'


if __name__ == '__main__':
  if len(sys.argv) != 3:
    Usage()
    sys.exit(1)
  ledpattern = LedPattern()
  sys.exit(ledpattern.Run(sys.argv[1], sys.argv[2]))
