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

"""Image installer for Google Fiber CPE devices."""

__author__ = 'dgentry@google.com (Denton Gentry)'

import glob
import hashlib
import os
import re
import StringIO
import struct
import subprocess
import sys
import tarfile
import urllib2
import zlib
from Crypto.Hash import SHA512
from Crypto.PublicKey import RSA
from Crypto.Signature import PKCS1_v1_5
import options


optspec = """
ginstall -p <partition>
ginstall -p <partition> -t <tarfile> [options...]
--
t,tar=        *.gi file to install, from a local path or uri://path
skiploader    skip installing bootloader (dev-only)
manifest=     manifest file
drm=          drm blob filename to install
p,partition=  partition to install to (primary, secondary, or other)
q,quiet       suppress unnecessary output
skiploadersig suppress checking the loader signature
b,basepath=   for tests, prepend a path to all files accessed.
"""


# unit tests can override these with fake versions
BUFSIZE = 4 * 1024            # 64k causes b/14299411
GZIP_HEADER = '\x1f\x8b\x08'  # encoded as string to ignore endianness
HNVRAM = 'hnvram'
NANDDUMP = ['nanddump']
SGDISK = 'sgdisk'


F = {
    'ETCPLATFORM': '/etc/platform',
    'ETCVERSION': '/etc/version',
    'DEV': '/dev',
    'MMCBLK0': '/dev/mmcblk0',
    'MTD_PREFIX': '/dev/mtd',
    'PROC_CMDLINE': '/proc/cmdline',
    'PROC_MTD': '/proc/mtd',
    'SECUREBOOT': '/tmp/gpio/ledcontrol/secure_boot',
    'SIGNINGKEY': '/etc/gfiber_public.der',
    'SYSCLASSMTD': '/sys/class/mtd',
    'SYSBLOCK': '/sys/block',
}


# Verbosity of output
quiet = False

default_manifest_v2 = {
    'installer_version': '2',
    'platforms': ['GFHD100', 'GFMS100'],
    'image_type': 'unlocked'
}

default_manifest_files = {
    'installer_version': '2',
    'image_type': 'unlocked'
}


class Fatal(Exception):
  """An exception that we print as just an error, with no backtrace."""
  pass


def Verify(f, s, k):
  key = RSA.importKey(k)
  h = SHA512.new(f.read())
  v = PKCS1_v1_5.new(key)
  return v.verify(h, s.read())


def Log(s, *args):
  sys.stdout.flush()
  if args:
    sys.stderr.write(s % args)
  else:
    sys.stderr.write(str(s))


def VerbosePrint(s, *args):
  if not quiet:
    Log(s, *args)


def GetPlatform():
  return open(F['ETCPLATFORM']).read().strip()


def GetVersion():
  return open(F['ETCVERSION']).read().strip()


def GetInternalHarddisk():
  for blkdev in sorted(glob.glob(F['SYSBLOCK'] + '/sd?')):
    dev_path = os.path.realpath(blkdev + '/device')
    if dev_path.find('usb') == -1:
      return os.path.join(F['DEV'], os.path.basename(blkdev))

  return None


def SetBootPartition(partition):
  VerbosePrint('Setting boot partition to kernel%d\n', partition)
  cmd = [HNVRAM, '-q', '-w', 'ACTIVATED_KERNEL_NAME=kernel%d' % partition]
  return subprocess.call(cmd)


def GetBootedPartition():
  """Get the role of partition where the running system is booted from.

  Returns:
    0 or 1 for rootfs0 and rootfs1, or None if not booted from flash.
  """
  try:
    with open(F['PROC_CMDLINE']) as f:
      cmdline = f.read().strip()
  except IOError:
    return None
  for arg in cmdline.split(' '):
    if arg.startswith('root='):
      partition = arg.split('=')[1]
      if partition == 'rootfs0':
        return 0
      elif partition == 'rootfs1':
        return 1
  return None


def PickFreeUbi():
  for i in range(32):
    if not os.path.exists('/dev/ubi%d' % i):
      return i
  raise Fatal('no free /dev/ubi devices found')


def GetMtdDevForNameOrNone(partname):
  """Find the mtd# for a named partition.

  In /proc/mtd we have:

  dev:    size   erasesize  name
  mtd0: 00200000 00010000 "cfe"
  mtd1: 00200000 00010000 "reserve0"
  mtd2: 10000000 00100000 "kernel0"
  mtd3: 10000000 00100000 "kernel1"

  Args:
    partname: the partition to find. For example, "kernel0"

  Returns:
    The mtd device, for example "mtd2"
  """
  quotedname = '"%s"' % partname
  # read the whole file at once to avoid race conditions in case it changes
  data = open(F['PROC_MTD']).read().split('\n')
  for line in data:
    fields = line.strip().split()
    if len(fields) >= 4 and fields[3] == quotedname:
      assert fields[0].startswith('mtd')
      assert fields[0].endswith(':')
      return '%s%d' % (F['MTD_PREFIX'], int(fields[0][3:-1]))
  return None  # no match


def IsMtdNand(mtddevname):
  mtddevname = re.sub(r'^' + F['MTD_PREFIX'], 'mtd', mtddevname)
  path = F['SYSCLASSMTD'] + '/{0}/type'.format(mtddevname)
  data = open(path).read()
  return 'nand' in data


def GetMtdDevForName(partname):
  """Like GetMtdDevForNameOrNone, but raises an exception on failure."""
  mtd = GetMtdDevForNameOrNone(partname)
  if not mtd:
    raise KeyError(partname)
  return mtd


def GetMtdDevForNameList(names):
  """Find the first mtd partition with any of the given names.

  Args:
    names: List of partition names.

  Raises:
    KeyError: when mtd partition cannot be found

  Returns:
    The mtd of the first name to match, or None of there is no match.
  """
  for name in names:
    mtd = GetMtdDevForNameOrNone(name)
    if mtd: return mtd
  raise KeyError(names)


def GetGptPartitionForName(blk_dev, name):
  """Find the device node for a named partition.

  From sgdisk -p we have:

  Number  Start (sector)    End (sector)  Size       Code  Name
     1           34816         1083391   512.0 MiB   0700  image0
     2         1083392         2131967   512.0 MiB   0700  image1
     3         2131968         2263039   64.0 MiB    0700  emergency
     4         2263040         2525183   128.0 MiB   8300  config
     5         2525184         6719487   2.0 GiB     8300  user

  Args:
    blk_dev: block device to search, like /dev/mmcblk0
    name: Name of partition to look for

  Returns:
    Device file of named partition
  """
  cmd = [SGDISK, '-p', blk_dev]
  devnull = open('/dev/null', 'w')
  try:
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=devnull)
  except OSError:
    return None  # no sgdisk, must not be a platform that supports it
  infix = ''
  if 'mmcblk' in blk_dev:
    infix = 'p'
  part = None
  for line in p.stdout:
    fields = line.strip().split()
    if len(fields) == 7 and fields[6] == name:
      part = blk_dev + infix + fields[0]
  p.wait()
  return part


def IsIdentical(description, srcfile, dstfile):
  """Compare srcfile and dstfile. Return true if contents are identical."""
  VerbosePrint('Verifying %s.\n', description)
  progress = ProgressBar()
  sbuf = srcfile.read(BUFSIZE)
  dbuf = dstfile.read(len(sbuf))
  if not sbuf:
    raise IOError('IsIdentical: srcfile is empty?')
  if not dbuf:
    raise IOError('IsIdentical: dstfile is empty?')
  while sbuf:
    if sbuf != dbuf:
      return False
    sbuf = srcfile.read(BUFSIZE)
    dbuf = dstfile.read(len(sbuf))
    progress.MadeProgress(len(sbuf))
  progress.Done()
  return True


def MatchesHash(description, dstfile, size, sha1):
  """Calculate SHA-1 hash of dstfile and compare with expected value."""
  VerbosePrint('Verifying %s.\n', description)
  progress = ProgressBar()
  m = hashlib.sha1()
  while size > 0:
    dbuf = dstfile.read(min(BUFSIZE, size))
    m.update(dbuf)
    size -= len(dbuf)
    progress.MadeProgress(len(dbuf))
  progress.Done()
  result = (m.hexdigest() == sha1)
  if not result:
    VerbosePrint('SHA1 hashes do not match. Expected: %s Actual: %s\n'
                 % (sha1, m.hexdigest()))
  return result


def SilentCmd(name, *args):
  """Wrapper for program calls that doesn't print or check errors."""
  null = open('/dev/null', 'w')
  cmd = [name] + list(args)
  subprocess.call(cmd, stderr=null)


def Cmd(name, *args):
  """Wrapper for program calls."""
  cmd = [name] + list(args)
  VerbosePrint('%s\n' % cmd)
  rc = subprocess.call(cmd)
  if rc != 0:
    raise IOError('Error: %r' % (cmd,))


def EraseMtd(mtddevname):
  """Erase an mtd partition."""
  VerbosePrint('Erasing flash partition %r\n', mtddevname)
  cmd = ['flash_erase', '--quiet', mtddevname, '0', '0']
  return subprocess.call(cmd)


def Nandwrite(f, mtddevname):
  """Write file to NAND flash using nandwrite."""
  cmd = ['nandwrite', '--quiet', '--markbad', mtddevname]
  VerbosePrint('%s\n' % cmd)
  p = subprocess.Popen(cmd, stdin=subprocess.PIPE)
  (written, written_sha) = WriteToFile(f, p.stdin)
  p.stdin.close()
  p.wait()
  return (written, written_sha)


def Pad(data, bufsize):
  if len(data) < bufsize:
    return data + '\xff' * (bufsize - len(data))
  else:
    return data


def WriteToFile(srcfile, dstfile):
  """Copy all bytes from srcfile to dstfile."""
  progress = ProgressBar()
  buf = srcfile.read(BUFSIZE)
  totsize = 0
  m = hashlib.sha1()
  while buf:
    totsize += len(buf)
    m.update(buf)
    dstfile.write(Pad(buf, BUFSIZE))
    buf = srcfile.read(BUFSIZE)
    progress.MadeProgress(len(buf))
  dstfile.flush()
  progress.Done()
  return (totsize, m.hexdigest())


def _CopyAndVerify(description, inf, outf):
  """Copy data from file object inf to file object outf, then verify it."""
  (written, written_sha) = WriteToFile(inf.filelike, outf)
  outf.seek(0, os.SEEK_SET)
  if inf.secure_hash and inf.secure_hash != written_sha:
    raise IOError('written-hash-verification-failed')
  if not MatchesHash(description, outf, written, written_sha):
    raise IOError('Read-and-hash-after-write verification failed')
  return written


def _CopyAndVerifyNand(inf, mtddevname):
  """Copy data from file object f to NAND flash mtddevname, then verify it."""
  VerbosePrint('Writing to NAND partition %r\n', mtddevname)
  (written, written_sha) = Nandwrite(inf.filelike, mtddevname)
  if inf.secure_hash and inf.secure_hash != written_sha:
    raise IOError('written-hash-verification-failed')
  length = '--length=%d' % written
  cmd = NANDDUMP + ['--bb=skipbad', length, '--quiet', mtddevname]
  VerbosePrint('%s\n' % cmd)
  p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
  if not MatchesHash(mtddevname, p.stdout, written, written_sha):
    raise IOError('Read-and-hash-after-write verification failed')
  while p.stdout.read(BUFSIZE):
    pass
  if p.wait() != 0:
    raise IOError('Read-after-write verification failed. '
                  'nanddump return non-zero')


def InstallToMtd(f, mtddevname):
  """Write an image to an mtd device."""
  if not isinstance(f, FileWithSecureHash):
    f = FileWithSecureHash(f, None)
  if EraseMtd(mtddevname):
    raise IOError('Flash erase failed.')
  VerbosePrint('Writing to mtd partition %r\n', mtddevname)
  # TODO(danielmentz): _CopyAndVerifyNand is based on the external tool
  # nandwrite which can handle bad erase blocks i.e. it skips them. The
  # bootloader (CFE) on the bruno platform (CPE 1.0) cannot handle bad blocks
  # at the moment so let's keep on using _CopyAndVerify on this platform.
  # _CopyAndVerify will throw an exception during the verification step which
  # is more desirable than installing a bad kernel image and depending on CFE
  # to fall back to the other kernel partition.
  if IsMtdNand(mtddevname) and GetPlatform().startswith('GFRG2'):
    return _CopyAndVerifyNand(f, mtddevname)
  else:
    return _CopyAndVerify(mtddevname, f, open(mtddevname, 'r+b'))


def InstallToFile(f, outfilename):
  """Write the file-like object f to file named outfilename."""
  VerbosePrint('Writing to raw file %r\n', outfilename)
  return _CopyAndVerify(outfilename, f, open(outfilename, 'w+b'))


def InstallRawFileToUbi(f, mtddevname):
  """Write an image without its own ubi header to a ubi device.

  Args:
    f: a file-like object holding the image to be installed.
    mtddevname: the device filename of the mtd partition to install to.

  Raises:
    IOError: when ubi format fails

  Returns:
    number of bytes written.
  """
  ubino = PickFreeUbi()
  SilentCmd('ubidetach', '-p', mtddevname)
  Cmd('ubiformat', '-y', '-q', mtddevname)
  Cmd('ubiattach', '-p', mtddevname, '-d', str(ubino))
  try:
    Cmd('ubimkvol', '-N', 'rootfs-prep', '-m', '/dev/ubi%d' % ubino)
    newmtd = GetMtdDevForName('rootfs-prep')
    siz = InstallToMtd(f, newmtd)
    Cmd('ubirename', '/dev/ubi%d' % ubino, 'rootfs-prep', 'rootfs')
  finally:
    SilentCmd('ubidetach', '-d', str(ubino))
  return siz


def WriteDrm(opt):
  """Write DRM Keyboxes."""
  Log('DO NOT INTERRUPT OR POWER CYCLE, or you will lose drm capability.\n')
  drm = open(opt.drm, 'rb')
  mtddevname = GetMtdDevForName('drmregion0')
  VerbosePrint('Writing drm to %r\n', mtddevname)
  InstallToMtd(drm, mtddevname)

  drm.seek(0)
  mtddevname = GetMtdDevForName('drmregion1')
  VerbosePrint('Writing drm to %r\n', mtddevname)
  InstallToMtd(drm, mtddevname)


def GetKey():
  """Return the key to check file signatures."""
  try:
    return open(F['SIGNINGKEY']).read()
  except IOError, e:
    raise Fatal(e)


def ParseManifest(f):
  """Parse a ginstall image manifest.

  Example:
    installer_version: 99
    image_type: fake
    platforms: [ GFHD100, GFMS100 ]
  Args:
    f: a file-like object for the manifest file
  Returns:
    a dict of the fields in the manifest.
  """
  result = {}
  for line in f:
    fields = line.split(':', 1)
    if len(fields) == 2:
      key = fields[0].strip()
      val = fields[1].strip()
      if val.startswith('['):  # [ GFHD100, GFMS100 ]
        val = re.sub(r'[\[\],\s]', r' ', val).split()
      result[key] = val
  return result


def CheckPlatform(manifest):
  platform = GetPlatform()
  platforms = manifest['platforms']
  for p in platforms:
    if p.lower() == platform.lower():
      return True
  raise Fatal('Package supports %r, but this device is %r'
              % (platforms, platform))


def CheckVeryOldVersion(manifest):
  """Check for old software versions (prior to 2/2013 or so).

  Old software versions are incompatible with this version of ginstall.
  In particular, we want to leave out versions that:
   - don't support 1GB NAND layout.
   - use pre-ubinized files instead of raw rootfs images.

  Args:
    manifest: the dictionary of manifest contents.

  Raises:
    Fatal: if the version is incompatible.
  """

  ver = manifest.get('version', '')
  if not ver:
    raise Fatal('unable to determine image version: %r' % manifest)
  if ver and (
      ver.startswith('bruno-') or
      (ver.startswith('gfibertv-') and ver < 'gfibertv-24')):
    raise Fatal('%r is too old: aborting.\n' % ver)


def CheckManifestVersion(manifest):
  v = int(manifest['installer_version'])
  if v >= 2 and v <= 4:
    return True
  else:
    raise Fatal('Incompatible manifest version: "%s"' % v)


def ParseVersionString(ver):
  """Extract major and minor revision number from version string.

  Args:
    ver: Version string
  Returns:
    A tuple (major, minor) or None if string cannot be parsed. Return 0 for
    minor if minor revision number cannot be parsed.

  Example:
    'abc-<x>.<y>junk' -> (x,y)
    'gfrg200-39-pre1-60-g2841888-da' -> (39,0)
    'gfrg200-38.6a3-ap' -> (38.6)
    'gfrg200-38-pre2-125-g403f9a3-da' -> (38,0)
  """
  m = re.match(r'[^-]+-(\d+)(?:\.(\d+))?', ver)
  if not m:
    return None
  return (int(m.group(1)), int(m.group(2)) if m.group(2) else 0)


def CheckMinimumVersion(manifest):
  """Ensure that running version meets minimum_version as specified in manifest.

  Args:
    manifest: Manifest from .gi file
  Raises:
    Fatal: when image should not be installed, or minimum_version field cannot
           be parsed
  Returns:
    True if minimum version requirement is met.

  """
  minimum_version = manifest.get('minimum_version')
  if not minimum_version: return True
  our_version = GetVersion()
  min_version = ParseVersionString(minimum_version)
  if not min_version:
    raise Fatal('Cannot parse minimum_version field "%s" in manifest' %
                minimum_version)
  if ParseVersionString(our_version) >= min_version:
    return True
  raise Fatal('Package requires minimum version %s, but we are running %s'
              % (minimum_version, our_version))


def CheckMisc(manifest):
  """Miscellaneous sanity checks.

  Args:
    manifest: the manifest from an image file
  Raises:
    Fatal: when image should not be installed
  """
  version = manifest.get('version', '')
  if (GetPlatform() == 'GFHD200' and BroadcomDeviceIsSecure() and
      (ParseVersionString(version) < (38, 11) or
       version.startswith('gftv200-39-pre0') or
       version.startswith('gftv200-39-pre1'))):
    raise Fatal('Refusing to install gftv200-38.10 and before, and '
                'gftv200-39-pre1 and before.')


class ProgressBar(object):
  """Progress bar that prints one dot per 1MB."""

  def __init__(self):
    self.bytes = 0

  def MadeProgress(self, b):
    self.bytes += b
    dotsize = 1024 * 1024
    if self.bytes > dotsize:
      VerbosePrint('.')
      self.bytes -= dotsize

  def Done(self):
    VerbosePrint('\n')


class FileWithSecureHash(object):
  """A file-like object paired with a SHA-1 hash."""

  def __init__(self, filelike, secure_hash):
    self.filelike = filelike
    self.secure_hash = secure_hash


def WriteLoaderToMtd(loader, loader_start, mtd, description):
  is_loader_current = False
  with open(mtd, 'rb') as mtdfile:
    VerbosePrint('Checking if the %s is up to date.\n', description)
    loader.filelike.seek(loader_start)
    is_loader_current = IsIdentical(description, loader.filelike, mtdfile)
  if is_loader_current:
    VerbosePrint('The %s is the latest.\n', description)
  else:
    loader.filelike.seek(loader_start, os.SEEK_SET)
    Log('DO NOT INTERRUPT OR POWER CYCLE, or you will brick the unit.\n')
    VerbosePrint('Writing to %r\n', mtd)
    InstallToMtd(loader, mtd)


def LogCallerInfo():
  """Log the call sequence leading to ginstall."""
  try:
    p = subprocess.Popen(['psback'], stdout=subprocess.PIPE)
    psback = p.stdout.readline().strip()
    p.wait()
    p = subprocess.Popen(['logos', 'ginstall'], stdin=subprocess.PIPE)
    p.stdin.write('args: %r\ncalled by: %s\n' % (sys.argv, psback))
    p.stdin.close()
    p.wait()
  except OSError:
    Log('W: psback/logos unavailable for tracing.\n')


def GetPartition(opt):
  """Return the partiton to install to, given the command line options."""
  if opt.partition == 'other':
    boot = GetBootedPartition()
    assert boot in [None, 0, 1]
    if boot is None:
      # Policy decision: if we're booted from NFS, install to secondary
      return 1
    else:
      return boot ^ 1
  elif opt.partition in ['primary', 0]:
    return 0
  elif opt.partition in ['secondary', 1]:
    return 1
  elif opt.partition:
    raise Fatal('--partition must be one of: primary, secondary, other')
  elif opt.tar:
    raise Fatal('A --partition option must be provided with --tar')
  else:
    return None


def InstallKernel(kern, partition):
  """Install a kernel file.

  Args:
    kern: a FileWithSecureHash object.
    partition: the partition to install to, 0 or 1.
  Raises:
    Fatal: if install fails
  """

  partition_name = 'kernel%d' % partition
  mtd = GetMtdDevForNameOrNone(partition_name)
  gpt = GetGptPartitionForName(F['MMCBLK0'], partition_name)
  if mtd:
    VerbosePrint('Writing kernel to %r\n' % mtd)
    InstallToMtd(kern, mtd)
  elif gpt:
    VerbosePrint('Writing kernel to %r\n' % gpt)
    InstallToFile(kern, gpt)
  else:
    raise Fatal('no partition named %r is available' % partition_name)


def InstallRootfs(rootfs, partition):
  """Install a rootfs file.

  Args:
    rootfs: a FileWithSecureHash object.
    partition: the partition to install to, 0 or 1.
  Raises:
    Fatal: if install fails
  """

  partition_name = 'rootfs%d' % partition
  mtd = GetMtdDevForNameOrNone(partition_name)
  if GetPlatform().startswith('GFSC'):
    hdd = GetInternalHarddisk()
    if hdd:
      gpt = GetGptPartitionForName(hdd, partition_name)
      if gpt:
        mtd = None
  else:
    gpt = GetGptPartitionForName(F['MMCBLK0'], partition_name)
  if mtd:
    if GetPlatform().startswith('GFMN'):
      VerbosePrint('Writing rootfs to %r\n' % mtd)
      InstallToMtd(rootfs, mtd)
    else:
      Log('Installing raw rootfs image to ubi partition %r\n' % mtd)
      VerbosePrint('Writing raw rootfs to %r\n', mtd)
      InstallRawFileToUbi(rootfs, mtd)
  elif gpt:
    VerbosePrint('Writing raw rootfs to %r\n', gpt)
    InstallToFile(rootfs, gpt)
  else:
    raise Fatal('no partition named %r is available' % partition_name)


def InstallLoader(loader):
  """Install a bootloader.

  Args:
    loader: a FileWithSecureHash object. This will generally point
      to a StringIO buffer.

  Raises:
    Fatal: if install fails
  """

  loader_start = loader.filelike.tell()
  installed = False
  for i in ['cfe', 'loader', 'loader0', 'loader1']:
    mtd = GetMtdDevForNameOrNone(i)
    if mtd:
      WriteLoaderToMtd(loader, loader_start, mtd, 'loader')
      installed = True
  if not installed:
    raise Fatal('no loader partition is available')


def InstallUloader(uloader):
  """Install a microloader.

  Args:
    uloader: a FileWithSecureHash object. This will generally point
      to a StringIO buffer.

  Raises:
    Fatal: if install fails
  """

  uloader_start = uloader.filelike.tell()
  mtd = GetMtdDevForNameOrNone('uloader')
  if mtd:
    uloader_signed = UloaderSigned(uloader.filelike)
    device_secure = C2kDeviceIsSecure(mtd)
    if uloader_signed and not device_secure:
      VerbosePrint('Signed uloader but unsecure box; stripping sig.\n')
      uloader, uloader_start = StripUloader(uloader.filelike,
                                            uloader_start)
      uloader = FileWithSecureHash(uloader, None)
    elif not uloader_signed and device_secure:
      raise Fatal('Unable to install unsigned uloader on secure device.')
    WriteLoaderToMtd(uloader, uloader_start, mtd, 'uloader')


def InstallImage(f, partition, skiploader=False, skiploadersig=False):
  """Install an image.

  Args:
    f: a file-like objected expected to provide a stream in tar format
    partition: integer 0 or 1 of the partition to install into
    skiploader: skip installation of a bootloader
    skiploadersig: skip checking of bootloader signature

  Raises:
    Fatal: if install fails
  """

  tar = tarfile.open(mode='r|*', fileobj=f)
  first = tar.next()

  if first.name == 'version':
    # ginstall v2
    manifest = default_manifest_v2.copy()
    manifest['version'] = str(tar.extractfile(first).read(4096)).strip()
  elif first.name == 'manifest':
    # ginstall v3
    manifest = ParseManifest(tar.extractfile(first))
  elif first.name == 'MANIFEST':
    # ginstall v4
    manifest = ParseManifest(tar.extractfile(first))
  else:
    # something else
    raise Fatal('Unknown image format, first file is: %s' % first.name)

  CheckPlatform(manifest)
  CheckManifestVersion(manifest)
  CheckVeryOldVersion(manifest)
  CheckMinimumVersion(manifest)
  CheckMisc(manifest)

  uloader = loader = None
  uloadersig = FileWithSecureHash(StringIO.StringIO(''), 'badsig')
  loadersig = FileWithSecureHash(StringIO.StringIO(''), 'badsig')

  for ti in tar:
    secure_hash = manifest.get('%s-sha1' % ti.name)
    if ti.name in ['version', 'manifest', 'MANIFEST']:
      # already processed
      pass
    elif ti.name in ['kernel.img', 'vmlinuz', 'vmlinux', 'uImage']:
      fh = FileWithSecureHash(tar.extractfile(ti), secure_hash)
      InstallKernel(fh, partition)
    elif ti.name.startswith('rootfs.'):
      fh = FileWithSecureHash(tar.extractfile(ti), secure_hash)
      InstallRootfs(fh, partition)
    elif ti.name in ['loader.img', 'loader.bin']:
      buf = StringIO.StringIO(tar.extractfile(ti).read())
      loader = FileWithSecureHash(buf, secure_hash)
    elif ti.name == 'loader.sig':
      buf = StringIO.StringIO(tar.extractfile(ti).read())
      loadersig = FileWithSecureHash(buf, secure_hash)
    elif ti.name == 'uloader.img':
      buf = StringIO.StringIO(tar.extractfile(ti).read())
      uloader = FileWithSecureHash(buf, secure_hash)
    elif ti.name == 'uloader.sig':
      buf = StringIO.StringIO(tar.extractfile(ti).read())
      uloadersig = FileWithSecureHash(buf, secure_hash)
    else:
      print 'Unknown install file %s' % ti.name

  if skiploadersig:
    loadersig = uloadersig = None

  key = GetKey()
  if loadersig and loader and not skiploader:
    if not Verify(loader.filelike, loadersig.filelike, key):
      raise Fatal('Loader signing check failed.')
    loader.filelike.seek(0, os.SEEK_SET)
  if uloadersig and uloader and not skiploader:
    if not Verify(uloader.filelike, uloadersig.filelike, key):
      raise Fatal('Uloader signing check failed.')
    uloader.filelike.seek(0, os.SEEK_SET)

  if loader:
    if skiploader:
      VerbosePrint('Skipping loader installation.\n')
    else:
      InstallLoader(loader)

  if uloader:
    if skiploader:
      VerbosePrint('Skipping uloader installation.\n')
    else:
      InstallUloader(uloader)


def OpenPathOrUrl(path):
  """Try to open path as a URL and as a local file."""
  try:
    return urllib2.urlopen(path)
  except ValueError:
    pass

  try:
    return open(path)
  except ValueError:
    pass

  raise Fatal('--tar=%s is not a valid path.' % path)


def main():
  global quiet  # gpylint: disable-msg=global-statement
  LogCallerInfo()
  o = options.Options(optspec)
  opt, unused_flags, unused_extra = o.parse(sys.argv[1:])

  if not (opt.drm or opt.tar or opt.partition):
    o.fatal('Expected at least one of --partition, --tar, or --drm')

  quiet = opt.quiet

  if opt.basepath:
    # Standalone test harness can pass in a fake root path.
    AddBasePath(opt.basepath)

  if opt.drm:
    WriteDrm(opt)

  if opt.tar and not opt.partition:
    # default to the safe option if not given
    opt.partition = 'other'

  partition = GetPartition(opt)
  if opt.tar:
    f = OpenPathOrUrl(opt.tar)
    InstallImage(f, partition, skiploader=opt.skiploader,
                 skiploadersig=opt.skiploadersig)

  if partition is not None:
    return SetBootPartition(partition)

  return 0


def BroadcomDeviceIsSecure():
  """Determines whether a Broadcom device is secure."""
  return os.path.isfile(F['SECUREBOOT'])


def C2kDeviceIsSecure(uloader_mtddevname):
  """Determines whether a Mindspeed C2k device verifies uloader signature.

  Currently this is done by examining the currently installed uloader.

  Args:
    uloader_mtddevname: Name of the mtd device containing the installed uloader

  Returns:
    True if the device is insecure, False otherwise
  """
  # TODO(smcgruer): Also check the OTP, raise exception if they differ.

  with open(uloader_mtddevname, 'r+b') as installed_uloader:
    return UloaderSigned(installed_uloader)


def UloaderSigned(uloader_file):
  """Determines if the given uloader file is signed or unsigned.

  The file's current location will be saved and restored when the
  function exits.

  Args:
    uloader_file: A file object containing the uloader to be checked.

  Returns:
    True if the passed uloader is signed, false otherwise.
  """

  current_loc = uloader_file.tell()

  # The simplest check for a signed uloader is to examine byte 16 (zero-indexed)
  # of the header, which indicates the key type.

  uloader_file.seek(0)
  header = uloader_file.read(20)
  uloader_file.seek(current_loc)

  return header[16] == '\x02'


def StripUloader(uloader, uloader_start):
  """Strips a signed uLoader, allowing it to be installed on an insecure device.

  IMPORTANT: This method will close the given uloader file. A new, memory-backed
  file is returned in its place.

  Args:
    uloader: A signed uloader file.
    uloader_start: The start offset of the given uLoader file.

  Returns:
    A tuple (uloader, uloader_start), containing the stripped uloader file and
    its start position.
  """

  uloader.seek(uloader_start)
  uloader_data = uloader.read()
  uloader.close()

  # The signed header includes 24 bytes of metadata and a 256 byte hash.
  header = list(uloader_data[:280])

  # Magic number and timestamp.
  new_header = header[:8]

  # CRC (initialized to 0s), embedded key length, and key type.
  new_header += '\x00' * 12

  # Image length.
  new_header += header[20:24]

  # Padding.
  new_header += '\x00' * 32

  # Calculate a CRC for the new header.
  new_header_string = ''.join(new_header)
  crc = zlib.crc32(new_header_string) & 0xFFFFFFFF
  new_header[8:12] = struct.pack('<I', crc)

  new_uloader = StringIO.StringIO()
  new_uloader.write(''.join(new_header))
  new_uloader.write(uloader_data[280:])
  new_uloader.seek(0)

  return new_uloader, new_uloader.tell()


def AddBasePath(path):
  """For tests, prepend a path to all files."""
  for (k, v) in F.iteritems():
    F[k] = path + v


if __name__ == '__main__':
  try:
    sys.exit(main())
  except Fatal, e:
    Log('%s\n', e)
    sys.exit(1)
