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
    self.script_out = self.tmpdir + '/out'
    self.old_path = os.environ['PATH']
    self.old_bufsize = ginstall.BUFSIZE
    self.old_files = ginstall.F
    os.environ['GINSTALL_OUT_FILE'] = self.script_out
    os.environ['GINSTALL_TEST_FAIL'] = ''
    os.environ['PATH'] = 'testdata/bin:' + self.old_path
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

  def tearDown(self):
    os.environ['PATH'] = self.old_path
    shutil.rmtree(self.tmpdir, ignore_errors=True)
    ginstall.F = self.old_files

  def WriteVersionFile(self, version):
    """Create a fake /etc/version file in /tmp."""
    filename = self.tmpdir + '/version'
    open(filename, 'w').write(version)
    ginstall.F['ETCVERSION'] = filename

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

  def testLockExceptions(self):
    lock_prefix = '/tmp/ginstall_test_lock'
    lock = ginstall.PidLock(lock_prefix)
    def lockFailure():
      with lock:
        with ginstall.PidLock(lock_prefix):
          pass
    self.assertRaises(ginstall.LockException, lockFailure)
    # Asserts no exceptions happen during normal usage.
    with lock:
      pass

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
    ginstall.SetBootPartition(0)
    ginstall.SetBootPartition(1)
    out = open(self.script_out).read().splitlines()
    self.assertEqual(out[0], 'hnvram -q -w ACTIVATED_KERNEL_NAME=kernel0')
    self.assertEqual(out[1], 'hnvram -q -w ACTIVATED_KERNEL_NAME=kernel1')

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

  def testGetBootedFromCmdLine(self):
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline1'
    self.assertEqual(ginstall.GetBootedPartition(), None)
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline2'
    self.assertEqual(ginstall.GetBootedPartition(), 0)
    ginstall.F['PROC_CMDLINE'] = 'testdata/proc/cmdline3'
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

if __name__ == '__main__':
  unittest.main()
