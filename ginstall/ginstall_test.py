#!/usr/bin/python
# Copyright 2011 Google Inc. All Rights Reserved.

"""Tests for ginstall.py"""

__author__ = 'dgentry@google.com (Denton Gentry)'

import os
import shutil
import stat
import StringIO
import tempfile
import unittest
import ginstall


# TODO(apenwarr): These tests are too "unit testy"; should test end-to-end.
#  The best way to do that is probably to write some fake ubiformat/etc tools
#  in testdata/bin, and then run a shell script that actually executes
#  ginstall under various realistic conditions, in order to test more stuff
#  that historically has gone wrong, like option parsing and manifest checking.
class GinstallTest(unittest.TestCase):
  def setUp(self):
    self.old_PATH = os.environ['PATH']
    self.old_bufsize = ginstall.BUFSIZE
    self.old_etcplatform = ginstall.ETCPLATFORM
    self.old_hnvram = ginstall.HNVRAM
    self.old_mtd_prefix = ginstall.MTD_PREFIX
    self.old_mmcblk = ginstall.MMCBLK
    self.old_proc_mtd = ginstall.PROC_MTD
    self.old_sgdisk = ginstall.SGDISK
    os.environ['PATH'] = (os.path.join(os.getcwd(), 'testdata/bin.tmp') + ':' +
                          os.path.join(os.getcwd(), 'testdata/bin') + ':' +
                          self.old_PATH)
    shutil.rmtree('testdata/bin.tmp', ignore_errors=True)
    os.mkdir('testdata/bin.tmp')
    ginstall.ETCPLATFORM = 'testdata/etc/platform'
    ginstall.MTD_PREFIX = 'testdata/dev/mtd'
    ginstall.PROC_MTD = "testdata/proc/mtd"
    ginstall.SGDISK = 'testdata/sgdisk'
    ginstall.SIGNINGKEY = 'testdata/signing_key.der'
    self.files_to_remove = list()

  def tearDown(self):
    os.environ['PATH'] = self.old_PATH
    ginstall.BUFSIZE = self.old_bufsize
    ginstall.ETCPLATFORM = self.old_etcplatform
    ginstall.HNVRAM = self.old_hnvram
    ginstall.MTD_PREFIX = self.old_mtd_prefix
    ginstall.MMCBLK = self.old_mmcblk
    ginstall.PROC_MTD = self.old_proc_mtd
    ginstall.SGDISK = self.old_sgdisk
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

  def testVerify(self):
    self.assertTrue(ginstall.Verify(
        open("testdata/img/loader.bin"),
        open("testdata/img/loader.sig"),
        open("testdata/img/public.der")))

  def testVerifyFailure(self):
    self.assertFalse(ginstall.Verify(
        open("testdata/img/loader.bin"),
        open("testdata/img/loader_bad.sig"),
        open("testdata/img/public.der")))

  def testIsIdentical(self):
    self.assertFalse(ginstall.IsIdentical(
        'testloader',
        open("testdata/img/loader.bin"),
        open("testdata/img/loader1.bin")))

  def testVerifyAndIsIdentical(self):
    loader = open("testdata/img/loader.bin")
    self.assertTrue(ginstall.Verify(
        loader,
        open("testdata/img/loader.sig"),
        open("testdata/img/public.der")))
    self.assertRaises(IOError, ginstall.IsIdentical,
        'loader', loader, open("testdata/img/loader.bin"))
    loader.seek(0)
    self.assertTrue(IOError, ginstall.IsIdentical(
        'loader', loader, open("testdata/img/loader.bin")))
    loader.seek(0)
    self.assertFalse(ginstall.IsIdentical(
        'loader', loader, open("testdata/img/loader1.bin")))

  def testGetMtdDevForName(self):
    self.assertEqual(ginstall.GetMtdDevForName("foo1"), "testdata/dev/mtd1")
    self.assertEqual(ginstall.GetMtdDevForName("foo2"), "testdata/dev/mtd2")
    self.assertEqual(ginstall.GetMtdDevForName("foo9"), "testdata/dev/mtd9")
    self.assertRaises(KeyError, ginstall.GetMtdDevForName, "nonexistant")
    self.assertEqual(ginstall.GetMtdDevForNameOrNone("nonexistant"), None)

  def testEraseMtd(self):
    testscript = "#!/bin/sh\necho -n $* >> {0}\n"
    (script, out) = self.MakeTestScript(testscript)
    shutil.copy(script.name, 'testdata/bin.tmp/flash_erase')
    ginstall.EraseMtd("/dev/mtd3")
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
    tarimg = ginstall.TarImage("testdata/img/vmlinux_slc.tar")
    self.assertEqual(tarimg.GetKernel().read(), "vmlinux")
    self.assertEqual(tarimg.GetRootFs().read(), "rootfs.squashfs")
    self.assertEqual(tarimg.GetLoader().read(), "loader.bin")
    tarimg = ginstall.TarImage("testdata/img/vmlinuz_slc.tar")
    self.assertEqual(tarimg.GetKernel().read(), "vmlinuz")
    self.assertEqual(tarimg.GetRootFs().read(), "rootfs.squashfs")
    self.assertEqual(tarimg.GetLoader().read(), "loader.bin")
    tarimg = ginstall.TarImage("testdata/img/vmboth_slc.tar")
    self.assertEqual(tarimg.GetKernel().read(), "vmlinuz")
    self.assertEqual(tarimg.GetRootFs().read(), "rootfs.squashfs")

  def testTarImageV3(self):
    tarimg = ginstall.TarImage("testdata/img/image_v3.gi")
    self.assertEqual(tarimg.GetKernel().read(), "kernel.img")
    self.assertEqual(tarimg.GetRootFs().read(), "rootfs.img")
    self.assertEqual(tarimg.GetLoader().read(), "loader.img")
    self.assertEqual(tarimg.GetUloader().read(), "uloader.img")
    self.assertEqual(tarimg.GetVersion(), "image_version")

  def testFileImage(self):
    fileimg = ginstall.FileImage("testdata/img/vmlinux",
                                 "testdata/img/rootfs.ubi",
                                 "testdata/img/loader.bin",
                                 "testdata/img/loader.sig",
                                 "testdata/img/manifest",
                                 "testdata/img/uloader.bin")
    self.assertEqual(fileimg.GetKernel().read(), "vmlinux")
    self.assertEqual(fileimg.GetRootFs().read(), "rootfs.ubi")
    self.assertEqual(fileimg.GetLoader().read(), "loader.bin")
    self.assertEqual(fileimg.GetUloader().read(), "uloader.bin")

  def testGetFileSize(self):
    self.assertEqual(ginstall.GetFileSize(open("testdata/img/vmlinux")), 7)
    self.assertEqual(ginstall.GetFileSize(open("testdata/random")), 4096)

  def testWriteMtd(self):
    origfile = open("testdata/random", "r")
    origsize = os.fstat(origfile.fileno())[6]

    ginstall.BUFSIZE = 1024

    open('testdata/dev/mtd4', 'w').close()
    writesize = ginstall.InstallToMtd(origfile, 'testdata/dev/mtd4')
    self.assertEqual(writesize, origsize)

    # check that data was written to MTDBLOCK
    self.assertEqual(ginstall.GetFileSize(open('testdata/dev/mtd4')), origsize)
    origfile.seek(0, os.SEEK_SET)
    self.assertEqual(origfile.read(),
                     open('testdata/dev/mtd4').read())

  def testWriteMtdEraseException(self):
    origfile = open("testdata/random", "r")
    (f_erase, eraseout) = self.MakeTestScript("#!/bin/sh\nexit 1\n")
    shutil.copy(f_erase.name, 'testdata/bin.tmp/flash_erase')
    self.assertRaises(IOError, ginstall.InstallToMtd, origfile, '/dev/mtd0')

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
    self.assertRaises(IOError, ginstall.InstallToMtd, origfile, '/dev/mtd4')

  def testWriteUbi(self):
    ginstall.BUFSIZE = 1024

    origfile = open("testdata/random", "r")
    origsize = os.fstat(origfile.fileno()).st_size

    open('testdata/dev/mtd99', 'w').close()
    writesize = ginstall.InstallRawFileToUbi(origfile, 'testdata/dev/mtd9')
    self.assertEqual(writesize, origsize)

    self.assertEqual(open('testdata/dev/mtd99').read(),
                     open('testdata/random').read())

  def testWriteUbiException(self):
    (ubifmt, out) = self.MakeTestScript("#!/bin/sh\nexit 1\n")
    shutil.copy(ubifmt.name, 'testdata/bin.tmp/ubiformat')
    os.system('ubiformat')
    origfile = open("testdata/random", "r")
    self.assertRaises(IOError, ginstall.InstallRawFileToUbi,
                      origfile, 'mtd0.tmp')

  def testSetBootPartition0(self):
    s = "#!/bin/sh\necho $* >> {0}\nexit 0"
    (hnvram, nvout) = self.MakeTestScript(s)
    ginstall.HNVRAM = hnvram.name

    ginstall.SetBootPartition(0)
    nvout.seek(0, os.SEEK_SET)
    self.assertEqual(nvout.readline(),
                     '-q -w ACTIVATED_KERNEL_NAME=kernel0\n')

  def testSetBootPartition1(self):
    s = "#!/bin/sh\necho $* >> {0}\nexit 0"
    (hnvram, nvout) = self.MakeTestScript(s)
    ginstall.HNVRAM = hnvram.name

    ginstall.SetBootPartition(1)
    nvout.seek(0, os.SEEK_SET)
    self.assertEqual(nvout.readline(),
                     '-q -w ACTIVATED_KERNEL_NAME=kernel1\n')

  def testParseManifest(self):
    l = 'installer_version: 99\nimage_type: fake\nplatforms: [ GFHD100, GFMS100 ]\n'
    in_f = StringIO.StringIO(l)
    actual = ginstall.ParseManifest(in_f)
    expected = {'installer_version': '99', 'image_type': 'fake',
                'platforms': [ 'GFHD100', 'GFMS100' ]}
    self.assertEqual(actual, expected)
    l = 'installer_version: 99\nimage_type: fake\nplatforms: GFHD007\n'
    in_f = StringIO.StringIO(l)
    actual = ginstall.ParseManifest(in_f)
    expected = {'installer_version': '99', 'image_type': 'fake',
                'platforms': 'GFHD007'}
    self.assertEqual(actual, expected)

  def testGetKey(self):
    key = ginstall.GetKey()
    self.assertEqual(key, 'This is a signing key.\n')

  def testPlatformRoutines(self):
    self.assertEqual(ginstall.GetPlatform(), 'GFUNITTEST')
    in_f = StringIO.StringIO('platforms: [ GFUNITTEST, GFFOOBAR ]\n')
    manifest = ginstall.ParseManifest(in_f)
    self.assertTrue(ginstall.CheckPlatform(manifest))

  def testGetBootedFromCmdLine(self):
    ginstall.PROC_CMDLINE = "testdata/proc/cmdline1"
    self.assertEqual(ginstall.GetBootedPartition(), None)
    ginstall.PROC_CMDLINE = "testdata/proc/cmdline2"
    self.assertEqual(ginstall.GetBootedPartition(), 0)
    ginstall.PROC_CMDLINE = "testdata/proc/cmdline3"
    self.assertEqual(ginstall.GetBootedPartition(), 1)


if __name__ == '__main__':
  unittest.main()
