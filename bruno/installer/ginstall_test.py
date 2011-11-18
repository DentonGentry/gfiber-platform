#!/usr/bin/python
# Copyright 2011 Google Inc. All Rights Reserved.

"""Tests for ginstall.py"""

__author__ = 'dgentry@google.com (Denton Gentry)'

import ginstall
import os
import stat
import tempfile
import unittest


class ImginstTest(unittest.TestCase):
  def MakeTestScript(self, text):
    """Create a script in /tmp, with an output file."""
    scriptfile = tempfile.NamedTemporaryFile(mode="r+", delete=False)
    os.chmod(scriptfile.name, stat.S_IRWXU | stat.S_IRWXG | stat.S_IRWXO)
    outfile = tempfile.NamedTemporaryFile(delete=False)
    scriptfile.write(text.format(outfile.name))
    return (scriptfile, outfile)

  def testGetMtdNum(self):
    self.assertEqual(ginstall.get_mtd_num(3), 3)
    self.assertEqual(ginstall.get_mtd_num("3"), 3)
    self.assertEqual(ginstall.get_mtd_num("mtd3"), 3)
    self.assertEqual(ginstall.get_mtd_num("mtd6743"), 6743)
    self.assertEqual(ginstall.get_mtd_num("invalid4"), False)

  def testGetEraseSize(self):
    old_proc_mtd = ginstall.PROC_MTD
    ginstall.PROC_MTD = "testdata/proc/mtd"
    siz = ginstall.get_erase_size("mtd3")
    self.assertEqual(siz, 128)
    siz = ginstall.get_erase_size("3")
    self.assertEqual(siz, 128)
    siz = ginstall.get_erase_size(3)
    self.assertEqual(siz, 128)
    siz = ginstall.get_erase_size("mtd4")
    self.assertEqual(siz, 256)
    siz = ginstall.get_erase_size("nonexistent")
    self.assertEqual(siz, 0)
    ginstall.PROC_MTD = old_proc_mtd

  def testRoundTo(self):
    self.assertEqual(ginstall.round_to(255, 256), 256)
    self.assertEqual(ginstall.round_to(1, 256), 256)
    self.assertEqual(ginstall.round_to(257, 256), 512)
    self.assertEqual(ginstall.round_to(512, 256), 512)
    self.assertEqual(ginstall.round_to(513, 256), 768)

  def testEraseMtd(self):
    testscript = "#!/bin/sh\necho -n $* >> {0}\n"
    (script, out) = self.MakeTestScript(testscript)
    flash_erase = ginstall.FLASH_ERASE
    ginstall.FLASH_ERASE = script.name
    script.close()
    ginstall.erase_mtd("mtd3")
    # Script wrote its arguments to out.name, read them in to check.
    output = out.read()
    out.close()
    self.assertEqual(output, "--quiet /dev/mtd3 0 0")
    ginstall.FLASH_ERASE = flash_erase
    os.remove(script.name)
    os.remove(out.name)

  def testTarImage(self):
    tarimg = ginstall.TarImage("testdata/img/vmlinux.tar")
    self.assertEqual(tarimg.GetKernel().read(), "vmlinux")
    self.assertEqual(tarimg.GetRootFs().read(), "rootfs.ubi")
    tarimg = ginstall.TarImage("testdata/img/vmlinuz.tar")
    self.assertEqual(tarimg.GetKernel().read(), "vmlinuz")
    self.assertEqual(tarimg.GetRootFs().read(), "rootfs.ubi")
    tarimg = ginstall.TarImage("testdata/img/vmboth.tar")
    self.assertEqual(tarimg.GetKernel().read(), "vmlinuz")
    self.assertEqual(tarimg.GetRootFs().read(), "rootfs.ubi")

  def testFileImage(self):
    fileimg = ginstall.FileImage("testdata/img/vmlinux",
                                "testdata/img/rootfs.ubi")
    self.assertEqual(fileimg.GetKernel().read(), "vmlinux")
    self.assertEqual(fileimg.GetRootFs().read(), "rootfs.ubi")

  def testGetFileSize(self):
    self.assertEqual(ginstall.get_file_size(open("testdata/img/vmlinux")), 7)
    self.assertEqual(ginstall.get_file_size(open("testdata/random")), 4096)

  def testWriteToMtd(self):
    origfile = open("testdata/random", "r")
    destfile = tempfile.NamedTemporaryFile()
    origsize = os.fstat(origfile.fileno())[6]

    # substitute fake /dev/mtdblock and /usr/bin/flash_erase
    oldmtdblock = ginstall.MTDBLOCK
    oldflasherase = ginstall.FLASH_ERASE
    ginstall.MTDBLOCK = destfile.name
    s = "#!/bin/sh\necho -n $* >> {0}\nexit 0\n"
    (f_erase, eraseout) = self.MakeTestScript(s)
    f_erase.close()
    ginstall.FLASH_ERASE = f_erase.name

    writesize = ginstall.install_to_mtd(origfile, 4)
    self.assertEqual(writesize, origsize)

    # check that flash_erase was run.
    self.assertEqual(eraseout.read(), "--quiet /dev/mtd4 0 0")

    # check that data was written to MTDBLOCK
    self.assertEqual(ginstall.get_file_size(destfile), origsize)
    origfile.seek(0, os.SEEK_SET)
    origdata = origfile.read()
    copydata = destfile.read()
    self.assertEqual(origdata, copydata)

    ginstall.MTDBLOCK = oldmtdblock
    ginstall.FLASH_ERASE = oldflasherase
    os.remove(f_erase.name)
    os.remove(eraseout.name)

  def testWriteToUbi(self):
    oldubiformat = ginstall.UBIFORMAT
    oldprocmtd = ginstall.PROC_MTD
    ginstall.PROC_MTD = "testdata/proc/mtd"
    s = "#!/bin/sh\necho $* >> {0}\nwc -c >> {0}\nexit 0"
    (ubifmt, ubiout) = self.MakeTestScript(s)
    ubifmt.close()
    ginstall.UBIFORMAT = ubifmt.name

    origfile = open("testdata/random", "r")
    origsize = os.fstat(origfile.fileno())[6]

    writesize = ginstall.install_to_ubi(origfile, 9)
    self.assertEqual(writesize, origsize)

    # check that ubiformat was run.
    ubiout.seek(0, os.SEEK_SET)
    # 2097152 is the eraseblock size in testdata/proc/mtd for mtd9
    self.assertEqual(ubiout.readline(), "/dev/mtd9 -f - -y -q -S 2097152\n")
    self.assertEqual(ubiout.readline(), "4096\n")

    ginstall.UBIFORMAT = oldubiformat
    ginstall.PROC_MTD = oldprocmtd
    os.remove(ubifmt.name)
    os.remove(ubiout.name)


if __name__ == '__main__':
  unittest.main()
