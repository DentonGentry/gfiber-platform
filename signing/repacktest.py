#!/usr/bin/python
# Copyright 2012 Google Inc. All Rights Reserved.

"""Test suites for repack."""

# Disable "Invalid method name"
# pylint: disable-msg=C6409

__author__ = 'kedong@google.com (Ke Dong)'

import commands
import os
import struct
import subprocess
import tempfile
import unittest
import repack


INFO_LENGTH = 4080
VERITY_TABLE = ''.join([
    '0 204800 verity payload=/dev/sda1 hashtree=/dev/sda2 hashstart=204800',
    ' alg=sha1 root_hexdigest=9f74809a2ee7607b16fcc70d9399a4de9725a727'])
KERNEL_SIZE = [18903, 40960, 4096, 2048]
VERITY_TABLE_SIZE = [16035, 20480, 4096, 1024]


class RepackTest(unittest.TestCase):
  """Test class for repack."""

  def setUp(self):
    repack.quiet = True

  def testCheckOutput(self):
    self.assertEqual(
        repack.CheckOutput(['ls', '-ltr']),
        commands.getstatusoutput('ls -ltr')[1])

  def testGetRandom(self):
    self.assertEqual(repack.GetRandom().__len__(), 64)

  def testCeilingBlock(self):
    self.assertEqual(repack.CeilingBlock(4096), 1)
    self.assertEqual(repack.CeilingBlock(4097), 2)
    self.assertEqual(repack.CeilingBlock(8192), 2)
    self.assertEqual(repack.CeilingBlock(40959), 10)

  def testUpdateVerityTable(self):
    offset = 100
    new_table = repack.UpdateVerityTable(VERITY_TABLE, offset)
    self.assertEqual(
        new_table,
        '0 204800 verity payload=/dev/sda1 '
        'hashtree=/dev/sda2 hashstart=100 alg=sha1 '
        'root_hexdigest=9f74809a2ee7607b16fcc70d9399a4de9725a727')

  def testFakeSign(self):
    vmlinuz = tempfile.NamedTemporaryFile()
    for i in KERNEL_SIZE:
      subprocess.check_call(['dd', 'if=/dev/urandom', 'of='+vmlinuz.name,
                             'bs=1', 'status=noxfer', 'count=' + str(i)],
                            stderr=subprocess.PIPE)
      f_sz = os.stat(vmlinuz.name).st_size
      fc = ''
      with open(vmlinuz.name, 'rb') as f:
        fc = f.read()
      repack.FakeSign(vmlinuz.name)
      self.assertEqual(os.stat(vmlinuz.name).st_size, f_sz + 16)
      with open(vmlinuz.name, 'rb') as f:
        self.assertEqual(f_sz, struct.unpack('I', f.read(4))[0])
        self.assertEqual(0x90091efb, struct.unpack('I', f.read(4))[0])
        self.assertEqual(0x0, struct.unpack('I', f.read(4))[0])
        self.assertEqual(0x0, struct.unpack('I', f.read(4))[0])
        self.assertEqual(fc, f.read())

  def testPackVerity(self):
    vmlinuz = tempfile.NamedTemporaryFile()
    hashbin = tempfile.NamedTemporaryFile()
    for i in KERNEL_SIZE:
      for j in VERITY_TABLE_SIZE:
        subprocess.check_call(['dd', 'if=/dev/urandom', 'of='+vmlinuz.name,
                               'bs=1', 'status=noxfer', 'count=' + str(i)],
                              stderr=subprocess.PIPE)
        subprocess.check_call(['dd', 'if=/dev/urandom', 'of='+hashbin.name,
                               'bs=1', 'status=noxfer', 'count=' + str(j)],
                              stderr=subprocess.PIPE)
        oc = open(vmlinuz.name, 'rb').read()
        f_sz = repack.CeilingBlock(oc.__len__())*repack.BLOCK_SIZE
        repack.PackVerity(vmlinuz.name, hashbin.name, VERITY_TABLE)
        h_sz = os.stat(hashbin.name).st_size
        self.assertEqual(os.stat(vmlinuz.name).st_size,
                         INFO_LENGTH + f_sz + h_sz)
        with open(vmlinuz.name, 'rb') as f:
          with open(hashbin.name, 'rb') as h:
            hc = h.read()
            info = f.read(INFO_LENGTH)
            offset = (f_sz+repack.BLOCK_SIZE) >> 9
            self.assertTrue(info.startswith(
                repack.VERITY_START +
                '0 204800 verity payload=/dev/sda1'
                ' hashtree=/dev/sda2 hashstart=' + str(offset) + ' alg=sha1'
                ' root_hexdigest=9f74809a2ee7607b16fcc70d9399a4de9725a727'
                + repack.VERITY_STOP
                ))
            fc = f.read(f_sz)
            self.assertTrue(fc.startswith(oc))
            fc = f.read(h_sz)
            self.assertEqual(fc, hc)

if __name__ == '__main__':
  unittest.main()
