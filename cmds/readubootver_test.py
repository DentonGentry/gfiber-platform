#!/usr/bin/python

import os
import shutil
import struct
import tempfile
import unittest

import readubootver

class ReadUbootVerTest(unittest.TestCase):

  def setUp(self):
    self.tmp_dir = tempfile.mkdtemp()
    self.save_MTD_FILE = readubootver.MTD_FILE
    self.save_CMDLINE_FILE = readubootver.CMDLINE_FILE
    readubootver.MTD_FILE = tempfile.mktemp(prefix=self.tmp_dir)
    readubootver.CMDLINE_FILE = tempfile.mktemp(prefix=self.tmp_dir)

  def tearDown(self):
    try:
      os.unlink(readubootver.MTD_FILE)
      os.unlink(readubootver.CMDLINE_FILE)
      shutil.rmtree(self.tmp_dir)
    except OSError:
      pass
    readubootver.MTD_FILE = self.save_MTD_FILE
    readubootver.CMDLINE_FILE = self.save_CMDLINE_FILE

  def testGetRootPartition(self):
    with open(readubootver.CMDLINE_FILE, 'w') as f:
      f.write('ttyS0,115200 mtdparts=spi_flash:768k(loader),256k(env),' +
              '128k(var1),128k(var2),128k(sysvar1),128k(sysvar2),' +
              '14m(kernel0),14m(kernel1),-(user_data) debug=1 root=rootfs0 ' +
              'console=ttyS0,115200 log_buf_len=1048576')
    part = readubootver.GetRootPartition()
    self.assertEqual('kernel0', part)

    with open(readubootver.CMDLINE_FILE, 'w') as f:
      f.write('ttyS0,115200 debug=1 root=rootfs1')
    part = readubootver.GetRootPartition()
    self.assertEqual('kernel1', part)

    with open(readubootver.CMDLINE_FILE, 'w') as f:
      f.write('root=rootfs0 ttyS0,115200 debug=1')
    part = readubootver.GetRootPartition()
    self.assertEqual('kernel0', part)


  def testGetBootMtds(self):
    with open(readubootver.MTD_FILE, 'w') as f:
      f.write('dev:    size   erasesize  name\n' +
              'mtd0: 000c0000 00010000 "loader"\n' +
              'mtd1: 00040000 00010000 "env"\n' +
              'mtd2: 00020000 00010000 "var1"\n' +
              'mtd3: 00020000 00010000 "var2"\n' +
              'mtd4: 00020000 00010000 "sysvar1"\n' +
              'mtd5: 00020000 00010000 "sysvar2"\n' +
              'mtd6: 00e00000 00010000 "kernel0"\n' +
              'mtd7: 00e00000 00010000 "kernel1"\n' +
              'mtd8: 00280000 00010000 "user_data"\n')
    mtds = readubootver.GetBootMtds()
    self.assertEqual(2, len(mtds))
    self.assertTrue('kernel0' in mtds)
    self.assertTrue('kernel1' in mtds)
    self.assertEqual('mtd6', mtds['kernel0'])
    self.assertEqual('mtd7', mtds['kernel1'])

    with open(readubootver.MTD_FILE, 'w') as f:
      f.write('dev:    size   erasesize  name\n' +
              'mtd0: 000c0000 00010000 "loader"\n' +
              'mtd1: 00040000 00010000 "env"\n' +
              'mtd2: 00020000 00010000 "var1"\n' +
              'mtd3: 00020000 00010000 "var2"\n' +
              'mtd4: 00020000 00010000 "sysvar1"\n' +
              'mtd5: 00020000 00010000 "sysvar2"\n' +
              'mtd166: 00e00000 00010000 "kernel0"\n')
    mtds = readubootver.GetBootMtds()
    self.assertEqual(1, len(mtds))
    self.assertTrue('kernel0' in mtds)
    self.assertEqual('mtd166', mtds['kernel0'])


  def testReadUbootHeader(self):
    data = (0x27051956, 0xdeadf00d, 0x123, 0x1234, 10, 10, 0xf00d,
            0, 0, 0, 0, 'version123')
    blob = struct.pack('>LLLLLLLBBBB32s', *data)
    tmp = tempfile.mktemp()
    with open(tmp, 'w') as f:
      f.write(blob)

    header = readubootver.ReadUbootHeader(tmp)
    self.assertEqual(header['magic'], 0x27051956)
    self.assertEqual(header['size'], 0x1234)
    self.assertEqual(header['dcrc'], 0xf00d)
    self.assertEqual(header['name'], 'version123')

    try:
      os.unlink(tmp)
    except OSError:
      pass


if __name__ == '__main__':
  unittest.main()
