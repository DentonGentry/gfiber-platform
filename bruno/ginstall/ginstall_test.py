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
  def setUp(self):
    self.old_bufsize = ginstall.BUFSIZE
    self.old_flash_erase = ginstall.FLASH_ERASE
    self.old_hnvram = ginstall.HNVRAM
    self.old_mtdblock = ginstall.MTDBLOCK
    self.old_proc_mtd = ginstall.PROC_MTD
    self.old_sys_ubi0 = ginstall.SYS_UBI0
    self.old_ubiformat = ginstall.UBIFORMAT
    self.files_to_remove = list()

  def tearDown(self):
    ginstall.BUFSIZE = self.old_bufsize
    ginstall.FLASH_ERASE = self.old_flash_erase
    ginstall.HNVRAM = self.old_hnvram
    ginstall.MTDBLOCK = self.old_mtdblock
    ginstall.PROC_MTD = self.old_proc_mtd
    ginstall.SYS_UBI0 = self.old_sys_ubi0
    ginstall.UBIFORMAT = self.old_ubiformat
    for file in self.files_to_remove:
      os.remove(file)

  def MakeTestScript(self, text):
    """Create a script in /tmp, with an output file."""
    scriptfile = tempfile.NamedTemporaryFile(mode="r+", delete=False)
    os.chmod(scriptfile.name, stat.S_IRWXU | stat.S_IRWXG | stat.S_IRWXO)
    outfile = tempfile.NamedTemporaryFile(delete=False)
    scriptfile.write(text.format(outfile.name))
    scriptfile.close()  # Linux won't run it if text file is busy
    self.files_to_remove.append(scriptfile.name)
    self.files_to_remove.append(outfile.name)
    return (scriptfile, outfile)

  def testGetMtdNum(self):
    self.assertEqual(ginstall.get_mtd_num(3), 3)
    self.assertEqual(ginstall.get_mtd_num("3"), 3)
    self.assertEqual(ginstall.get_mtd_num("mtd3"), 3)
    self.assertEqual(ginstall.get_mtd_num("mtd6743"), 6743)
    self.assertEqual(ginstall.get_mtd_num("invalid4"), False)

  def testGetEraseSize(self):
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

  def testGetMtdDevForPartition(self):
    ginstall.PROC_MTD = "testdata/proc/mtd"
    self.assertEqual(ginstall.get_mtd_dev_for_partition("foo1"), "mtd1")
    self.assertEqual(ginstall.get_mtd_dev_for_partition("foo2"), "mtd2")
    self.assertEqual(ginstall.get_mtd_dev_for_partition("foo9"), "mtd9")
    self.assertEqual(ginstall.get_mtd_dev_for_partition("nonexistant"), None)

  def testRoundTo(self):
    self.assertEqual(ginstall.round_to(255, 256), 256)
    self.assertEqual(ginstall.round_to(1, 256), 256)
    self.assertEqual(ginstall.round_to(257, 256), 512)
    self.assertEqual(ginstall.round_to(512, 256), 512)
    self.assertEqual(ginstall.round_to(513, 256), 768)

  def testEraseMtd(self):
    testscript = "#!/bin/sh\necho -n $* >> {0}\n"
    (script, out) = self.MakeTestScript(testscript)
    ginstall.FLASH_ERASE = script.name
    ginstall.erase_mtd("mtd3")
    # Script wrote its arguments to out.name, read them in to check.
    output = out.read()
    out.close()
    self.assertEqual(output, "--quiet /dev/mtd3 0 0")

  def testTarImage(self):
    tarimg = ginstall.TarImage("testdata/img/vmlinux.tar")
    self.assertEqual(tarimg.GetKernel().read(), "vmlinux")
    self.assertEqual(tarimg.GetRootFs().read(), "rootfs.squashfs_ubi")
    self.assertEqual(tarimg.GetLoader().read(), "loader.bin")
    tarimg = ginstall.TarImage("testdata/img/vmlinuz.tar")
    self.assertEqual(tarimg.GetKernel().read(), "vmlinuz")
    self.assertEqual(tarimg.GetRootFs().read(), "rootfs.squashfs_ubi")
    self.assertEqual(tarimg.GetLoader().read(), "loader.bin")
    tarimg = ginstall.TarImage("testdata/img/vmboth.tar")
    self.assertEqual(tarimg.GetKernel().read(), "vmlinuz")
    self.assertEqual(tarimg.GetRootFs().read(), "rootfs.squashfs_ubi")

  def testFileImage(self):
    fileimg = ginstall.FileImage("testdata/img/vmlinux",
                                 "testdata/img/rootfs.ubi",
                                 "testdata/img/loader.bin")
    self.assertEqual(fileimg.GetKernel().read(), "vmlinux")
    self.assertEqual(fileimg.GetRootFs().read(), "rootfs.ubi")
    self.assertEqual(fileimg.GetLoader().read(), "loader.bin")

  def testGetFileSize(self):
    self.assertEqual(ginstall.get_file_size(open("testdata/img/vmlinux")), 7)
    self.assertEqual(ginstall.get_file_size(open("testdata/random")), 4096)

  def testWriteMtd(self):
    origfile = open("testdata/random", "r")
    destfile = tempfile.NamedTemporaryFile()
    origsize = os.fstat(origfile.fileno())[6]

    # substitute fake /dev/mtdblock and /usr/bin/flash_erase
    ginstall.MTDBLOCK = destfile.name
    s = "#!/bin/sh\necho -n $* >> {0}\nexit 0\n"
    (f_erase, eraseout) = self.MakeTestScript(s)
    ginstall.FLASH_ERASE = f_erase.name
    ginstall.BUFSIZE = 1024

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

  def testWriteMtdEraseException(self):
    origfile = open("testdata/random", "r")
    (f_erase, eraseout) = self.MakeTestScript("#!/bin/sh\nexit 1\n")
    ginstall.FLASH_ERASE = f_erase.name
    self.assertRaises(IOError, ginstall.install_to_mtd, origfile, 0)

  def testWriteMtdVerifyException(self):
    origfile = open("testdata/random", "r")
    destfile = open("/dev/zero", "w")
    origsize = os.fstat(origfile.fileno())[6]

    # substitute fake /dev/mtdblock and /usr/bin/flash_erase
    ginstall.MTDBLOCK = destfile.name
    s = "#!/bin/sh\nexit 0\n"
    (f_erase, eraseout) = self.MakeTestScript(s)
    ginstall.FLASH_ERASE = f_erase.name

    # verify should fail, destfile will read back zero.
    self.assertRaises(IOError, ginstall.install_to_mtd, origfile, 4)

  def testWriteUbi(self):
    ginstall.PROC_MTD = "testdata/proc/mtd"
    s = "#!/bin/sh\necho $* >> {0}\nwc -c >> {0}\nexit 0"
    (ubifmt, ubiout) = self.MakeTestScript(s)
    ginstall.UBIFORMAT = ubifmt.name
    ginstall.BUFSIZE = 1024

    origfile = open("testdata/random", "r")
    origsize = os.fstat(origfile.fileno())[6]

    writesize = ginstall.install_to_ubi(origfile, 9)
    self.assertEqual(writesize, origsize)

    # check that ubiformat was run.
    ubiout.seek(0, os.SEEK_SET)
    # 2097152 is the eraseblock size in testdata/proc/mtd for mtd9
    self.assertEqual(ubiout.readline(), "/dev/mtd9 -f - -y -q -S 2097152\n")
    self.assertEqual(ubiout.readline(), "4096\n")

  def testWriteUbiException(self):
    ginstall.PROC_MTD = "testdata/proc/mtd"
    (ubifmt, out) = self.MakeTestScript("#!/bin/sh\nexit 1\n")
    ginstall.UBIFORMAT = ubifmt.name

    origfile = open("testdata/random", "r")
    self.assertRaises(IOError, ginstall.install_to_ubi, origfile, 0)

  def testBootedPartition(self):
    ginstall.PROC_MTD = "testdata/proc/mtd.bruno"
    ginstall.SYS_UBI0 = "/path/to/nonexistant/file"
    self.assertEqual(ginstall.get_booted_partition(), None)
    ginstall.SYS_UBI0 = "testdata/sys/class/ubi/ubi0.primary"
    self.assertEqual(ginstall.get_booted_partition(), "primary")
    ginstall.SYS_UBI0 = "testdata/sys/class/ubi/ubi0.secondary"
    self.assertEqual(ginstall.get_booted_partition(), "secondary")

  def testOtherPartition(self):
    self.assertEqual(ginstall.get_other_partition("primary"), "secondary")
    self.assertEqual(ginstall.get_other_partition("secondary"), "primary")

  def testSetBootPartition0(self):
    s = "#!/bin/sh\necho $* >> {0}\nexit 0"
    (hnvram, nvout) = self.MakeTestScript(s)
    ginstall.HNVRAM = hnvram.name

    ginstall.set_boot_partition(0)
    nvout.seek(0, os.SEEK_SET)
    self.assertEqual(nvout.readline(),
                     '-w MTD_TYPE_FOR_KERNEL=RAW -w ACTIVATED_KERNEL_NAME=kernel0 -w EXTRA_KERNEL_OPT=ubi.mtd=rootfs0 root=mtdblock:rootfs rootfstype=squashfs\n')

  def testSetBootPartition1(self):
    s = "#!/bin/sh\necho $* >> {0}\nexit 0"
    (hnvram, nvout) = self.MakeTestScript(s)
    ginstall.HNVRAM = hnvram.name

    ginstall.set_boot_partition(1)
    nvout.seek(0, os.SEEK_SET)
    self.assertEqual(nvout.readline(),
                     '-w MTD_TYPE_FOR_KERNEL=RAW -w ACTIVATED_KERNEL_NAME=kernel1 -w EXTRA_KERNEL_OPT=ubi.mtd=rootfs1 root=mtdblock:rootfs rootfstype=squashfs\n')


if __name__ == '__main__':
  unittest.main()
