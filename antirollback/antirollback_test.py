#!/usr/bin/python
# Copyright 2011 Google Inc. All Rights Reserved.

"""Tests for antirollbackd."""

__author__ = 'dgentry@google.com (Denton Gentry)'

import tempfile
import unittest

import antirollback


global_test_vars = {'timedottime': 0.0, 'timedotsleep': None}


def TimeDotSleep(t):
  global_test_vars['timedotsleep'] = t


def TimeDotTime():
  return global_test_vars['timedottime']


class AntirollbackTest(unittest.TestCase):
  def setUp(self):
    self.old_build_filename = antirollback.BUILD_FILENAME
    self.old_proc_ar = antirollback.PROC_AR
    self.old_proc_uptime = antirollback.PROC_UPTIME
    self.old_sleep = antirollback.SLEEP
    self.old_timenow = antirollback.TIMENOW
    self.ar_file = tempfile.NamedTemporaryFile()
    self.build_file = tempfile.NamedTemporaryFile()
    self.proc_file = tempfile.NamedTemporaryFile()
    self.uptime_file = tempfile.NamedTemporaryFile()
    antirollback.BUILD_FILENAME = self.build_file.name
    antirollback.PROC_AR = self.proc_file.name
    antirollback.PROC_UPTIME = self.uptime_file.name
    antirollback.RUNFOREVER = True
    antirollback.SLEEP = TimeDotSleep
    antirollback.TIMENOW = TimeDotTime
    global_test_vars['timedottime'] = 0.0
    global_test_vars['timedotsleep'] = None

  def tearDown(self):
    antirollback.BUILD_FILENAME = self.old_build_filename
    antirollback.PROC_AR = self.old_proc_ar
    antirollback.PROC_UPTIME = self.old_proc_uptime
    antirollback.SLEEP = self.old_sleep
    antirollback.TIMENOW = self.old_timenow

  def WriteToFile(self, f, content):
    f.seek(0, 0)
    f.write(str(content))
    f.flush()

  def testMonotime(self):
    self.WriteToFile(self.uptime_file, 123456789.0)
    self.assertEqual(antirollback.GetMonotime(), 123456789.0)
    antirollback.PROC_UPTIME = '/nosuchfile'
    self.assertRaises(IOError, antirollback.GetMonotime)

  def testPersistTime(self):
    self.WriteToFile(self.ar_file, 12345.0)
    self.assertEqual(antirollback.GetPersistTime(self.ar_file.name), 12345.0)
    self.assertEqual(antirollback.GetPersistTime('/nosuchfile'), 0.0)

  def testBuildDate(self):
    self.WriteToFile(self.build_file, '1350910920')
    self.assertEqual(antirollback.GetBuildDate(self.build_file.name),
                     1350910920.0)
    self.assertEqual(antirollback.GetBuildDate('/nosuchfile'), 0.0)

  def GetKernelArTime(self):
    self.proc_file.seek(0, 0)
    return float(self.proc_file.read())

  def testLoopSimple(self):
    uptime = 999.0
    new_uptime = 8.0 * 60.0 * 60.0 + 1.0
    self.WriteToFile(self.uptime_file, new_uptime)
    now = 1000000.0
    new_now = now + (new_uptime - uptime)
    self.proc_file.seek(0, 0)
    (uptime, now) = antirollback.LoopIterate(uptime=uptime, now=now,
                                             sleeptime=111,
                                             ar_filename=self.ar_file.name,
                                             kern_f=self.proc_file)
    self.assertEqual(global_test_vars['timedotsleep'], 111)
    self.assertEqual(uptime, new_uptime)
    self.assertEqual(now, new_now)
    self.assertEqual(antirollback.GetPersistTime(self.ar_file.name), new_now)
    self.assertEqual(self.GetKernelArTime(), new_now)

  def testGetAntirollbackTime(self):
    global_test_vars['timedottime'] = 99.0
    self.WriteToFile(self.ar_file, 999.0)
    self.WriteToFile(self.build_file, 888.0)
    n = self.ar_file.name
    self.assertEqual(antirollback.GetAntirollbackTime(n), antirollback.BIRTHDAY)
    global_test_vars['timedottime'] = 1500000000.0
    self.assertEqual(antirollback.GetAntirollbackTime(n), 1500000000.0)
    self.WriteToFile(self.ar_file, 1500000001.0)
    self.assertEqual(antirollback.GetAntirollbackTime(n), 1500000001.0)
    self.WriteToFile(self.build_file, 1500000002.0)
    self.assertEqual(antirollback.GetAntirollbackTime(n), 1500000002.0)


if __name__ == '__main__':
  unittest.main()
