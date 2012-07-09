#!/usr/bin/python
# Copyright 2012 Google Inc. All Rights Reserved.

"""Tests for readallfiles."""

__author__ = 'kedong@google.com (Ke Dong)'

import os
import tempfile
import unittest
import readallfiles


class ReadAllFilesTest(unittest.TestCase):

  def setUp(self):
    readallfiles.quiet = True
    pass

  def tearDown(self):
    pass

  def testReadFile(self):
    tf = tempfile.NamedTemporaryFile()
    tfName = tf.name
    st = os.stat(tfName)
    tf.seek(0)
    fc, siz = readallfiles.ReadFile(tfName, st)
    self.assertEqual(fc, 1)
    self.assertEqual(siz, 0)
    tf.seek(0)
    tf.write(128*'8')
    tf.flush()
    fc, siz = readallfiles.ReadFile(tfName, st)
    self.assertEqual(fc, 1)
    self.assertEqual(siz, 128)
    tf.seek(0)
    tf.write(255*'8')
    tf.flush()
    fc, siz = readallfiles.ReadFile(tfName, st)
    self.assertEqual(fc, 1)
    self.assertEqual(siz, 255)
    st = os.stat('/proc')
    tf.seek(0)
    fc, siz = readallfiles.ReadFile(tfName, st)
    self.assertEqual(fc, 0)
    self.assertEqual(siz, 0)

  def testReadAllFiles(self):
    cnt = readallfiles.ReadAllFiles('testdata/readallfiles')
    self.assertEqual(cnt, 1)

if __name__ == '__main__':
  unittest.main()
