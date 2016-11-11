#!/usr/bin/python
# Copyright 2014 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Tests for ginstall.py."""

__author__ = 'dgentry@google.com (Denton Gentry)'

import os
import shutil
import StringIO
import struct
import sys
import tempfile
import unittest
import ginstall


class FakeImgWManifest(object):

  def __init__(self, manifest):
    self.manifest = ginstall.ParseManifest(manifest)

  def ManifestVersion(self):
    return int(self.manifest['installer_version'])


class GinstallTest(unittest.TestCase):

  def setUp(self):
    self.tmpdir = tempfile.mkdtemp()
    self.hnvram_dir = self.tmpdir + '/hnvram'
    self.script_out = self.tmpdir + '/out'
    self.old_path = os.environ['PATH']
    self.old_bufsize = ginstall.BUFSIZE
    self.old_files = ginstall.F
    os.environ['GINSTALL_HNVRAM_DIR'] = self.hnvram_dir
    os.environ['GINSTALL_OUT_FILE'] = self.script_out
    os.environ['GINSTALL_TEST_FAIL'] = ''
    os.environ['PATH'] = 'testdata/bin:' + self.old_path
    os.makedirs(self.hnvram_dir)
    os.makedirs(self.tmpdir + '/dev')
    ginstall.F['ETCPLATFORM'] = 'testdata/etc/platform'
    ginstall.F['DEV'] = self.tmpdir + '/dev'
    ginstall.F['MMCBLK'] = self.tmpdir + '/dev/mmcblk0'
    ginstall.F['MTD_PREFIX'] = self.tmpdir + '/dev/mtd'
    ginstall.F['PROC_MTD'] = 'testdata/proc/mtd.GFHD100'
    ginstall.F['SECUREBOOT'] = 'testdata/tmp/gpio/ledcontrol/secure_boot'
    ginstall.F['SGDISK'] = 'testdata/bin/sgdisk'
    ginstall.F['SIGNINGKEY'] = 'testdata/signing_key.der'
    ginstall.F['SYSBLOCK'] = self.tmpdir + '/sys/block'
    os.makedirs(ginstall.F['SYSBLOCK'])
    ginstall.F['SYSCLASSMTD'] = 'testdata/sys/class/mtd'
    for i in range(0, 10):
      open(ginstall.F['MTD_PREFIX'] + str(i), 'w').write('1')

    os.mkdir(self.tmpdir + '/mmcblk0boot0')
    os.mkdir(self.tmpdir + '/mmcblk0boot1')
    ginstall.MMC_RO_LOCK['MMCBLK0BOOT0'] = (
        self.tmpdir + '/mmcblk0boot0/force_ro')
    ginstall.MMC_RO_LOCK['MMCBLK0BOOT1'] = (
        self.tmpdir + '/mmcblk0boot1/force_ro')

    # default OS to 'fiberos'
    self.WriteOsFile('fiberos')

  def tearDown(self):
    os.environ['PATH'] = self.old_path
    shutil.rmtree(self.tmpdir, ignore_errors=True)
    ginstall.F = self.old_files

  def WriteVersionFile(self, version):
    """Create a fake /etc/version file in /tmp."""
    filename = self.tmpdir + '/version'
    open(filename, 'w').write(version)
    ginstall.F['ETCVERSION'] = filename

  def WriteOsFile(self, os_name):
    """Create a fake /etc/os file in /tmp."""
    filename = self.tmpdir + '/os'
    open(filename, 'w').write(os_name)
    ginstall.F['ETCOS'] = filename

  def WriteHnvramAttr(self, attr, val):
    filename = self.hnvram_dir + '/%s' % attr
    open(filename, 'w').write(val)

  def ReadHnvramAttr(self, attr):
    filename = self.hnvram_dir + '/%s' % attr
    try:
      return open(filename).read()
    except IOError:
      return None

  def testVerify(self):
    self.assertTrue(ginstall.Verify(
        open('testdata/img/loader.bin'),
        open('testdata/img/loader.sig'),
        open('testdata/etc/google_public.der')))

  def testVerifyFailure(self):
    self.assertFalse(ginstall.Verify(
        open('testdata/img/loader.bin'),
        open('testdata/img/loader_bad.sig'),
        open('testdata/etc/google_public.der')))

  def testIsIdentical(self):
    self.assertFalse(ginstall.IsIdentical(
        'testloader',
        open('testdata/img/loader.bin'),
        open('testdata/img/loader1.bin')))

  def testVerifyAndIsIdentical(self):
    loader = open('testdata/img/loader.bin')
    self.assertTrue(ginstall.Verify(
        loader,
        open('testdata/img/loader.sig'),
        open('testdata/etc/google_public.der')))
    self.assertRaises(IOError, ginstall.IsIdentical,
                      'loader', loader, open('testdata/img/loader.bin'))
    loader.seek(0)
    self.assertTrue(ginstall.IsIdentical(
        'loader', loader, open('testdata/img/loader.bin')))
    loader.seek(0)
    self.assertFalse(ginstall.IsIdentical(
        'loader', loader, open('testdata/img/loader1.bin')))

  def testIsMtdNand(self):
    mtd = ginstall.F['MTD_PREFIX']
    self.assertFalse(ginstall.IsMtdNand(mtd + '6'))
    self.assertTrue(ginstall.IsMtdNand(mtd + '7'))

  def testInstallToMtdNandFails(self):
    # A nanddump that writes incorrect data to stdout
    ginstall.NANDDUMP = 'testdata/bin/nanddump.wrong'
    in_f = StringIO.StringIO('Testing123')
    mtdfile = self.tmpdir + '/mtd'
    self.assertRaises(IOError, ginstall.InstallToMtd, in_f, mtdfile)

  def testWriteMtd(self):
    origfile = open('testdata/random', 'r')
    origsize = os.fstat(origfile.fileno())[6]

    ginstall.BUFSIZE = 1024

    mtd = ginstall.F['MTD_PREFIX']
    open(mtd + '4', 'w').close()
    writesize = ginstall.InstallToMtd(origfile, mtd + '4')
    self.assertEqual(writesize, origsize)

    # check that data was written to MTDBLOCK
    origfile.seek(0, os.SEEK_SET)
    self.assertEqual(origfile.read(), open(mtd + '4').read())

  def testWriteMtdEraseException(self):
    origfile = open('testdata/random', 'r')
    self.assertRaises(IOError, ginstall.InstallToMtd, origfile, '/dev/mtd0')

  def testWriteMtdVerifyException(self):
    origfile = open('testdata/random', 'r')
    ginstall.MTDBLOCK = '/dev/zero'
    # verify should fail, destfile will read back zero.
    self.assertRaises(IOError, ginstall.InstallToMtd, origfile, '/dev/mtd4')

  def testWriteUbiException(self):
    os.environ['GINSTALL_TEST_FAIL'] = 'fail'
    os.system('ubiformat')
    origfile = open('testdata/random', 'r')
    self.assertRaises(IOError, ginstall.InstallRawFileToUbi,
                      origfile, 'mtd0.tmp')

  def testSetBootPartition(self):
    self.WriteOsFile('fiberos')
    ginstall.SetBootPartition('fiberos', 0)
    self.assertEqual('kernel0', self.ReadHnvramAttr('ACTIVATED_KERNEL_NAME'))
    ginstall.SetBootPartition('fiberos', 1)
    self.assertEqual('kernel1', self.ReadHnvramAttr('ACTIVATED_KERNEL_NAME'))
    ginstall.SetBootPartition('android', 0)
    self.assertEqual('a', self.ReadHnvramAttr('ANDROID_ACTIVE_PARTITION'))
    self.assertEqual('android', self.ReadHnvramAttr('BOOT_TARGET'))
    ginstall.SetBootPartition('android', 1)
    self.assertEqual('b', self.ReadHnvramAttr('ANDROID_ACTIVE_PARTITION'))
    self.assertEqual('android', self.ReadHnvramAttr('BOOT_TARGET'))

    self.WriteOsFile('android')
    ginstall.SetBootPartition('fiberos', 0)
    self.assertEqual('kernel0', self.ReadHnvramAttr('ACTIVATED_KERNEL_NAME'))
    self.assertEqual('fiberos', self.ReadHnvramAttr('BOOT_TARGET'))
    ginstall.SetBootPartition('fiberos', 1)
    self.assertEqual('kernel1', self.ReadHnvramAttr('ACTIVATED_KERNEL_NAME'))
    self.assertEqual('fiberos', self.ReadHnvramAttr('BOOT_TARGET'))
    ginstall.SetBootPartition('android', 0)
    self.assertEqual('a', self.ReadHnvramAttr('ANDROID_ACTIVE_PARTITION'))
    ginstall.SetBootPartition('android', 1)
    self.assertEqual('b', self.ReadHnvramAttr('ANDROID_ACTIVE_PARTITION'))

    # also verify the hnvram command history for good measures
    out = open(self.script_out).read().splitlines()
    self.assertEqual(out[0], 'hnvram -q -w ACTIVATED_KERNEL_NAME=kernel0')
    self.assertEqual(out[1], 'hnvram -q -w ACTIVATED_KERNEL_NAME=kernel1')
    self.assertEqual(out[2], 'hnvram -q -w ANDROID_ACTIVE_PARTITION=a')
    self.assertEqual(out[3], 'hnvram -q -w BOOT_TARGET=android')
    self.assertEqual(out[4], 'hnvram -q -w ANDROID_ACTIVE_PARTITION=b')
    self.assertEqual(out[5], 'hnvram -q -w BOOT_TARGET=android')
    self.assertEqual(out[6], 'hnvram -q -w ACTIVATED_KERNEL_NAME=kernel0')
    self.assertEqual(out[7], 'hnvram -q -w BOOT_TARGET=fiberos')
    self.assertEqual(out[8], 'hnvram -q -w ACTIVATED_KERNEL_NAME=kernel1')
    self.assertEqual(out[9], 'hnvram -q -w BOOT_TARGET=fiberos')
    self.assertEqual(out[10], 'hnvram -q -w ANDROID_ACTIVE_PARTITION=a')
    self.assertEqual(out[11], 'hnvram -q -w ANDROID_ACTIVE_PARTITION=b')

  def testParseManifest(self):
    l = ('installer_version: 99\nimage_type: fake\n'
         'platforms: [ GFHD100, GFMS100 ]\n')
    in_f = StringIO.StringIO(l)
    actual = ginstall.ParseManifest(in_f)
    expected = {'installer_version': '99', 'image_type': 'fake',
                'platforms': ['GFHD100', 'GFMS100']}
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

  def testGetOs(self):
    self.WriteOsFile('fiberos')
    self.assertEqual('fiberos', ginstall.GetOs())
    self.WriteOsFile('android')
    self.assertEqual('android', ginstall.GetOs())
    # in case file doesn't exist, default is 'fiberos'
    os.remove(self.tmpdir + '/os')
    self.assertEqual('fiberos', ginstall.GetOs())

  def testGetMtdPrefix(self):
    self.WriteOsFile('fiberos')
    self.assertEqual(ginstall.F['MTD_PREFIX'], ginstall.GetMtdPrefix())
    self.WriteOsFile('android')
    self.assertEqual(ginstall.F['MTD_PREFIX-ANDROID'], ginstall.GetMtdPrefix())
    # unknown OS returns 'fiberos'
    self.WriteOsFile('windows')
    self.assertEqual(ginstall.F['MTD_PREFIX'], ginstall.GetMtdPrefix())

  def testGetMmcblk0Prefix(self):
    self.WriteOsFile('fiberos')
    self.assertEqual(ginstall.F['MMCBLK0'], ginstall.GetMmcblk0Prefix())
    self.WriteOsFile('android')
    self.assertEqual(ginstall.F['MMCBLK0-ANDROID'],
                     ginstall.GetMmcblk0Prefix())
    # unknown OS returns 'fiberos'
    self.WriteOsFile('windows')
    self.assertEqual(ginstall.F['MMCBLK0'], ginstall.GetMmcblk0Prefix())

  def testGetInternalHarddisk(self):
    self.assertEqual(ginstall.GetInternalHarddisk(), None)

    os.mkdir(ginstall.F['SYSBLOCK'] + '/sda')
    os.symlink(ginstall.F['SYSBLOCK'] + '/sda/usb_disk',
               ginstall.F['SYSBLOCK'] + '/sda/device')
    os.mkdir(ginstall.F['SYSBLOCK'] + '/sdc')
    os.symlink(ginstall.F['SYSBLOCK'] + '/sdc/sata_disk',
               ginstall.F['SYSBLOCK'] + '/sdc/device')
    expected = ginstall.F['DEV'] + '/sdc'
    self.assertEqual(ginstall.GetInternalHarddisk(), expected)

    os.mkdir(ginstall.F['SYSBLOCK'] + '/sdb')
    expected = ginstall.F['DEV'] + '/sdb'
    self.assertEqual(ginstall.GetInternalHarddisk(), expected)

  def MakeImgWManifestVersion(self, version):
    in_f = StringIO.StringIO('installer_version: %s\n' % version)
    return FakeImgWManifest(in_f)

  def testCheckManifestVersion(self):
    manifest = {}
    for v in ['2', '3', '4']:
      manifest['installer_version'] = v
      self.assertTrue(ginstall.CheckManifestVersion(manifest))
    for v in ['1', '5']:
      manifest['installer_version'] = v
      self.assertRaises(ginstall.Fatal, ginstall.CheckManifestVersion, manifest)
    for v in ['3junk']:
      manifest['installer_version'] = v
      self.assertRaises(ValueError, ginstall.CheckManifestVersion, manifest)

  def MakeManifestWMinimumVersion(self, version):
    in_f = StringIO.StringIO('minimum_version: %s\n' % version)
    return ginstall.ParseManifest(in_f)

  def testCheckMinimumVersion(self):
    self.WriteVersionFile('gftv200-38.10')
    for v in [
        'gftv200-38.5',
        'gftv200-38-pre2-58-g72b3037-da',
        'gftv200-38-pre2']:
      manifest = self.MakeManifestWMinimumVersion(v)
      self.assertTrue(ginstall.CheckMinimumVersion(manifest))
    for v in [
        'gftv200-39-pre0-58-g72b3037-da',
        'gftv200-39-pre0',
        'gftv200-39-pre1-58-g72b3037-da',
        'gftv200-39-pre1',
        'gftv200-38.11',
        ]:
      manifest = self.MakeManifestWMinimumVersion(v)
      self.assertRaises(ginstall.Fatal, ginstall.CheckMinimumVersion,
                        manifest)
    manifest = self.MakeManifestWMinimumVersion('junk')
    self.assertRaises(ginstall.Fatal, ginstall.CheckMinimumVersion,
                      manifest)

  def testCheckMisc(self):
    ginstall.F['ETCPLATFORM'] = 'testdata/etc/platform.GFHD200'

    for v in [
        'gftv200-38.11',
        'gftv200-39-pre2-58-g72b3037-da',
        'gftv200-39-pre2']:
      manifest = {'version': v}
      ginstall.CheckMisc(manifest)  # checking that it does not raise exception

    for v in [
        'gftv200-39-pre0-58-g72b3037-da',
        'gftv200-39-pre0',
        'gftv200-39-pre1-58-g72b3037-da',
        'gftv200-39-pre1',
        'gftv200-38.9',
        'gftv200-38.10'
        ]:
      manifest = {'version': v}
      self.assertRaises(ginstall.Fatal, ginstall.CheckMisc, manifest)

  def MakeManifestWithFilenameSha1s(self, filename):
    m = ('installer_version: 4\n'
         'image_type: unlocked\n'
         'version: gftv254-48-pre2-1100-g25ff8d0-ck\n'
         'platforms: [ GFHD254 ]\n')
    if filename is not None:
      m += '%s-sha1: 9b5236c282b8c11b38a630361b6c690d6aaa50cb\n' % filename

    in_f = StringIO.StringIO(m)
    return ginstall.ParseManifest(in_f)

  def testGetOsFromManifest(self):
    # android specific image names return 'android'
    for img in ginstall.ANDROID_IMAGES:
      manifest = self.MakeManifestWithFilenameSha1s(img)
      self.assertEqual('android', ginstall.GetOsFromManifest(manifest))

    # fiberos image names or anything non-android returns 'fiberos'
    for img in ['rootfs.img', 'kernel.img', 'whatever.img']:
      manifest = self.MakeManifestWithFilenameSha1s(img)
      self.assertEqual('fiberos', ginstall.GetOsFromManifest(manifest))

    # no sha1 entry in the manifest returns 'fiberos'
    manifest = self.MakeManifestWithFilenameSha1s(None)
    self.assertEqual('fiberos', ginstall.GetOsFromManifest(manifest))

  def testGetBootedPartition(self):
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.none'
    self.assertEqual(None, ginstall.GetBootedPartition())
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.0'
    self.assertEqual(0, ginstall.GetBootedPartition())
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.1'
    self.assertEqual(1, ginstall.GetBootedPartition())

    # Android
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.android.none'
    self.assertEqual(None, ginstall.GetBootedPartition())
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.android.0'
    self.assertEqual(0, ginstall.GetBootedPartition())
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.android.1'
    self.assertEqual(1, ginstall.GetBootedPartition())

    # Prowl
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.prowl.none'
    self.assertEqual(ginstall.GetBootedPartition(), None)
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.prowl.0'
    self.assertEqual(ginstall.GetBootedPartition(), 0)
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.prowl.1'
    self.assertEqual(ginstall.GetBootedPartition(), 1)

  def testGetActivePartitionFromHNVRAM(self):
    # FiberOS looks at ACTIVATED_KERNEL_NAME, not ANDROID_ACTIVE_PARTITION
    # 0
    self.WriteHnvramAttr('ACTIVATED_KERNEL_NAME', '0')
    self.assertEqual(0, ginstall.GetActivePartitionFromHNVRAM('fiberos'))
    self.WriteHnvramAttr('ANDROID_ACTIVE_PARTITION', '0')
    self.assertEqual(0, ginstall.GetActivePartitionFromHNVRAM('fiberos'))
    self.WriteHnvramAttr('ANDROID_ACTIVE_PARTITION', '1')
    self.assertEqual(0, ginstall.GetActivePartitionFromHNVRAM('fiberos'))
    # 1
    self.WriteHnvramAttr('ACTIVATED_KERNEL_NAME', '1')
    self.assertEqual(1, ginstall.GetActivePartitionFromHNVRAM('fiberos'))
    self.WriteHnvramAttr('ANDROID_ACTIVE_PARTITION', '0')
    self.assertEqual(1, ginstall.GetActivePartitionFromHNVRAM('fiberos'))

    # Android looks at ANDROID_ACTIVE_PARTITION, not ACTIVATED_KERNEL_NAME
    # 0
    self.WriteHnvramAttr('ANDROID_ACTIVE_PARTITION', '0')
    self.assertEqual(0, ginstall.GetActivePartitionFromHNVRAM('android'))
    self.WriteHnvramAttr('ACTIVATED_KERNEL_NAME', '0')
    self.assertEqual(0, ginstall.GetActivePartitionFromHNVRAM('android'))
    self.WriteHnvramAttr('ACTIVATED_KERNEL_NAME', '1')
    self.assertEqual(0, ginstall.GetActivePartitionFromHNVRAM('android'))
    # 1
    self.WriteHnvramAttr('ANDROID_ACTIVE_PARTITION', '1')
    self.assertEqual(1, ginstall.GetActivePartitionFromHNVRAM('android'))
    self.WriteHnvramAttr('ACTIVATED_KERNEL_NAME', '0')
    self.assertEqual(1, ginstall.GetActivePartitionFromHNVRAM('android'))

  def TestGetPartition(self):
    self.assertEqual(0, ginstall.GetPartition('primary', 'fiberos'))
    self.assertEqual(0, ginstall.GetPartition(0, 'fiberos'))
    self.assertEqual(1, ginstall.GetPartition('secondary', 'fiberos'))
    self.assertEqual(1, ginstall.GetPartition(1, 'fiberos'))
    self.assertEqual(0, ginstall.GetPartition('primary', 'android'))
    self.assertEqual(0, ginstall.GetPartition(0, 'android'))
    self.assertEqual(1, ginstall.GetPartition('secondary', 'android'))
    self.assertEqual(1, ginstall.GetPartition(1, 'android'))

    # other: FiberOS->FiberOS
    self.WriteOsFile('fiberos')
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.none'
    self.assertEqual(1, ginstall.GetPartition('other', 'fiberos'))
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.0'
    self.assertEqual(1, ginstall.GetPartition('other', 'fiberos'))
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.1'
    self.assertEqual(0, ginstall.GetPartition('other', 'fiberos'))
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.prowl.none'
    self.assertEqual(1, ginstall.GetPartition('other', 'fiberos'))
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.prowl.0'
    self.assertEqual(1, ginstall.GetPartition('other', 'fiberos'))
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.prowl.1'
    self.assertEqual(0, ginstall.GetPartition('other', 'fiberos'))

    # other: FiberOS->Android
    self.WriteHnvramAttr('ANDROID_ACTIVE_PARTITION', 'a')
    self.assertEqual(1, ginstall.GetPartition('other', 'android'))
    self.WriteHnvramAttr('ANDROID_ACTIVE_PARTITION', 'b')
    self.assertEqual(0, ginstall.GetPartition('other', 'android'))
    self.WriteHnvramAttr('ANDROID_ACTIVE_PARTITION', 'bla')
    self.assertEqual(1, ginstall.GetPartition('other', 'android'))

    # other: Android->FiberOS
    self.WriteOsFile('android')
    self.WriteHnvramAttr('ACTIVATED_KERNEL_NAME', '0')
    self.assertEqual(1, ginstall.GetPartition('other', 'fiberos'))
    self.WriteHnvramAttr('ACTIVATED_KERNEL_NAME', '1')
    self.assertEqual(0, ginstall.GetPartition('other', 'fiberos'))
    self.WriteHnvramAttr('ACTIVATED_KERNEL_NAME', 'bla')
    self.assertEqual(1, ginstall.GetPartition('other', 'fiberos'))

    # other: Android->Android
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.android.none'
    self.assertEqual(1, ginstall.GetPartition('other', 'android'))
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.android.0'
    self.assertEqual(1, ginstall.GetPartition('other', 'android'))
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.android.1'
    self.assertEqual(0, ginstall.GetPartition('other', 'android'))

    # Test prowl and gfactive
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.prowl.none'
    self.assertEqual(ginstall.GetBootedPartition(), None)
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.prowl.0'
    self.assertEqual(ginstall.GetBootedPartition(), 0)
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline.prowl.1'
    self.assertEqual(ginstall.GetBootedPartition(), 1)

  def testUloaderSigned(self):
    magic_num = 0xDEADBEEF
    timestamp = 0x5627148C
    crc = 0x12345678
    key_len = 0x00000000
    signed_key_type = 0x00000002
    unsigned_key_type = 0x00000000
    image_len = 0x0000A344

    signed_uloader, _ = self._CreateUloader(
        magic_num, timestamp, crc, key_len, signed_key_type, image_len)

    self.assertTrue(ginstall.UloaderSigned(signed_uloader))
    self.assertEqual(signed_uloader.tell(), 0)

    unsigned_uloader, _ = self._CreateUloader(
        magic_num, timestamp, crc, key_len, unsigned_key_type, image_len)

    self.assertFalse(ginstall.UloaderSigned(unsigned_uloader))
    self.assertEqual(unsigned_uloader.tell(), 0)

  def testStripUloader(self):
    magic_num = 0xDEADBEEF
    timestamp = 0x5627148C
    crc = 0x12345678
    key_len = 0x00000000
    key_type = 0x00000002
    image_len = 0x0000A344

    signed_uloader, uloader_data = self._CreateUloader(
        magic_num, timestamp, crc, key_len, key_type, image_len)

    stripped_uloader, _ = ginstall.StripUloader(signed_uloader, 0)
    stripped_uloader_bytes = stripped_uloader.read()

    header = struct.unpack('<IIIIII', stripped_uloader_bytes[:24])
    self.assertEqual(header[0], magic_num)
    self.assertEqual(header[1], timestamp)
    self.assertEqual(header[3], key_len)
    self.assertEqual(header[4], 0)  # key type should have been set to 0
    self.assertEqual(header[5], image_len)

    self.assertEqual(stripped_uloader_bytes[24:56], '\x00' * 32)

    self.assertEqual(stripped_uloader_bytes[56:], uloader_data)

  def testLockUnlockMmcBoot(self):
    ginstall.UnlockMMC('MMCBLK0BOOT0')
    ginstall.UnlockMMC('MMCBLK0BOOT1')
    self.assertEqual(open(ginstall.MMC_RO_LOCK['MMCBLK0BOOT0']).read(), '0')
    self.assertEqual(open(ginstall.MMC_RO_LOCK['MMCBLK0BOOT1']).read(), '0')

    ginstall.LockMMC('MMCBLK0BOOT0')
    self.assertEqual(open(ginstall.MMC_RO_LOCK['MMCBLK0BOOT0']).read(), '1')
    self.assertEqual(open(ginstall.MMC_RO_LOCK['MMCBLK0BOOT1']).read(), '0')

    ginstall.LockMMC('MMCBLK0BOOT1')
    self.assertEqual(open(ginstall.MMC_RO_LOCK['MMCBLK0BOOT1']).read(), '1')
    self.assertEqual(open(ginstall.MMC_RO_LOCK['MMCBLK0BOOT0']).read(), '1')

  def _CreateUloader(self, magic_num, time, crc, key_len, key_type, image_len):
    """Helper method that creates a memory-backed uloader file."""

    uloader_data = os.urandom(image_len)
    if key_type == 0:
      # Unsigned; 32 bytes of padding.
      extra = '\x00' * 32
    elif key_type == 2:
      # Signed; a 256 byte signature.
      extra = os.urandom(256)
    else:
      raise ValueError('Key type must be 0 (unsigned) or 2 (signed)')

    uloader = StringIO.StringIO()
    uloader.write(struct.pack(
        '<IIIIII', magic_num, time, crc, key_len, key_type, image_len))
    uloader.write(extra)
    uloader.write(uloader_data)
    uloader.seek(0)

    return uloader, uloader_data

  def testGetMemTotal(self):
    ginstall.F['MEMINFO'] = 'testdata/proc/meminfo1'
    total = ginstall.GetMemTotal()
    self.assertTrue(total > 4*1e9)
    ginstall.F['MEMINFO'] = 'testdata/proc/meminfo2'
    total = ginstall.GetMemTotal()
    self.assertTrue(total < 4*1e9)

  def testOpenPathOrUrl(self):
    # URL
    two_oh_four = ginstall.OpenPathOrUrl('http://www.gstatic.com/generate_204')
    self.assertEqual(204, two_oh_four.getcode())

    # on-disk file
    on_disk_file = tempfile.NamedTemporaryFile()
    testdata = os.urandom(16)
    on_disk_file.write(testdata)
    on_disk_file.flush()
    self.assertEqual(ginstall.OpenPathOrUrl(on_disk_file.name).read(), testdata)

    # stdin (-)
    self.assertEqual(ginstall.OpenPathOrUrl('-'), sys.stdin)


if __name__ == '__main__':
  unittest.main()
