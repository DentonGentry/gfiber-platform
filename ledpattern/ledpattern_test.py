#!/usr/bin/python

"""Tests for ledpattern."""

import os
import tempfile
import unittest

import ledpattern


class TestLedpattern(unittest.TestCase):

  def setUp(self):
    self.ledpattern = ledpattern.LedPattern()
    self.fd_red, ledpattern.SYSFS_RED_BRIGHTNESS = tempfile.mkstemp()
    print ledpattern.SYSFS_RED_BRIGHTNESS
    self.fd_blue, ledpattern.SYSFS_BLUE_BRIGHTNESS = tempfile.mkstemp()
    print ledpattern.SYSFS_BLUE_BRIGHTNESS

  def tearDown(self):
    os.close(self.fd_red)
    os.close(self.fd_blue)
    os.unlink(ledpattern.SYSFS_RED_BRIGHTNESS)
    os.unlink(ledpattern.SYSFS_BLUE_BRIGHTNESS)

  def testReadCsvPatternFileTest(self):
    expected = ['R', 'B', 'P']
    actual = self.ledpattern.ReadCsvPatternFile('./testdata/test.pat', 'test')
    self.assertEqual(expected, actual)

  def testReadCsvPatternFileNotFound(self):
    actual = self.ledpattern.ReadCsvPatternFile('/does/not/exist.pat', 'test')
    self.assertEqual(None, actual)

  def testReadCsvPatternFileNoState(self):
    actual = self.ledpattern.ReadCsvPatternFile('./testdata/test.pat', '')
    self.assertEqual(None, actual)

  def testReadCsvPatternFileIsMissingState(self):
    actual = self.ledpattern.ReadCsvPatternFile('./testdata/test.pat', 'foo')
    self.assertEqual(None, actual)

  def testReadCsvPatternFileInvalidCharsGetFiltered(self):
    expected = ['B', 'B', 'B']
    actual = self.ledpattern.ReadCsvPatternFile('./testdata/test.pat',
                                                'test_with_invalid_chars')
    self.assertEqual(expected, actual)

  def testReadCsvPatternFileEmptyPattern(self):
    expected = []
    actual = self.ledpattern.ReadCsvPatternFile('./testdata/test.pat',
                                                'test_empty_pattern')
    self.assertEqual(expected, actual)

  def testSetRedBrightness(self):
    self.ledpattern.SetRedBrightness('foo')
    with open(ledpattern.SYSFS_RED_BRIGHTNESS, 'r') as f:
      actual = f.read()
    self.assertEqual('foo', actual)

  def testSetBlueBrightness(self):
    self.ledpattern.SetBlueBrightness('bar')
    with open(ledpattern.SYSFS_BLUE_BRIGHTNESS, 'r') as f:
      actual = f.read()
    self.assertEqual('bar', actual)


if __name__ == '__main__':
  unittest.main()
