#!/usr/bin/python
# Copyright 2011 Google Inc. All Rights Reserved.

"""Image installer for GFHD100."""

__author__ = 'dgentry@google.com (Denton Gentry)'

import os
import re
import StringIO
import struct
import subprocess
import sys
import tarfile
import zlib
from Crypto.Hash import SHA512
from Crypto.PublicKey import RSA
from Crypto.Signature import PKCS1_v1_5
import options


optspec = """
ginstall -p <partition>
ginstall -p <partition> -t <tarfile> [options...]
ginstall -p <partition> -k <kernel> -r <rootfs> [options...]
--
t,tar=        tar archive containing kernel and rootfs
k,kernel=     kernel image filename to install
r,rootfs=     rootfs UBI image filename to install
skiploader    skip installing bootloader (dev-only)
loader=       bootloader file to install
loadersig=    bootloader signature filename
manifest=     manifest file
drm=          drm blob filename to install
p,partition=  partition to install to (primary, secondary, or other)
q,quiet       suppress unnecessary output
skiploadersig suppress checking the loader signature
uloader=      microloader file to install
"""


# unit tests can override these with fake versions
BUFSIZE = 64 * 1024
ETCPLATFORM = '/etc/platform'
HNVRAM = 'hnvram'
MTD_PREFIX = '/dev/mtd'
MMCBLK = '/dev/mmcblk0'
PROC_CMDLINE = '/proc/cmdline'
PROC_MTD = '/proc/mtd'
SGDISK = 'sgdisk'
SIGNINGKEY = '/etc/gfiber_public.der'
GZIP_HEADER = '\x1f\x8b\x08'  # encoded as string to ignore endianness


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
  return open(ETCPLATFORM).read().strip()


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
    with open(PROC_CMDLINE) as f:
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
  data = open(PROC_MTD).read().split('\n')
  for line in data:
    fields = line.strip().split()
    if len(fields) >= 4 and fields[3] == quotedname:
      assert fields[0].startswith('mtd')
      assert fields[0].endswith(':')
      return '%s%d' % (MTD_PREFIX, int(fields[0][3:-1]))
  return None  # no match


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

  Returns:
    The mtd of the first name to match, or None of there is no match.
  """
  for name in names:
    mtd = GetMtdDevForNameOrNone(name)
    if mtd: return mtd
  raise KeyError(names)


def GetGptPartitionForName(name):
  """Find the mmcmlk0p# for a named partition.

  From sgdisk -p we have:

  Number  Start (sector)    End (sector)  Size       Code  Name
     1           34816         1083391   512.0 MiB   0700  image0
     2         1083392         2131967   512.0 MiB   0700  image1
     3         2131968         2263039   64.0 MiB    0700  emergency
     4         2263040         2525183   128.0 MiB   8300  config
     5         2525184         6719487   2.0 GiB     8300  user
  """
  cmd = [SGDISK, '-p', MMCBLK]
  devnull = open('/dev/null', 'w')
  try:
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=devnull)
  except OSError:
    return None  # no sgdisk, must not be a platform that supports it
  mmcpart = None
  for line in p.stdout:
    fields = line.strip().split()
    if len(fields) == 7 and fields[6] == name:
      mmcpart = MMCBLK + 'p' + fields[0]
  p.wait()
  return mmcpart


def IsDeviceNoSigning():
  """Returns true if the platform does not handle a kernel header prepended."""
  return False


# TODO(apenwarr): instead of re-reading the source file, use a checksum.
#  This will be especially important later when we want to avoid ever reading
#  the input file twice, so we can stream it from stdin.
def IsIdentical(description, srcfile, dstfile):
  """Compare srcfile and dstfile. Return true if contents are identical."""
  VerbosePrint('Verifying %s.\n', description)
  sbuf = srcfile.read(BUFSIZE)
  dbuf = dstfile.read(len(sbuf))
  if not sbuf:
    raise IOError('IsIdentical: srcfile is empty?')
  if not dbuf:
    raise IOError('IsIdentical: dstfile is empty?')
  while sbuf and dbuf:
    if sbuf != dbuf:
      return False
    sbuf = srcfile.read(BUFSIZE)
    dbuf = dstfile.read(len(sbuf))
    VerbosePrint('.')
  VerbosePrint('\n')
  return True


def GetFileSize(f):
  """Return size of a file like object."""
  current = f.tell()
  f.seek(0, os.SEEK_END)
  size = f.tell()
  f.seek(current, os.SEEK_SET)
  return size


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


def Pad(data, bufsize):
  if len(data) < bufsize:
    return data + '\xff' * (bufsize - len(data))
  else:
    return data


def WriteToFile(srcfile, dstfile):
  """Copy all bytes from srcfile to dstfile."""
  buf = srcfile.read(BUFSIZE)
  totsize = 0
  while buf:
    totsize += len(buf)
    dstfile.write(Pad(buf, BUFSIZE))
    buf = srcfile.read(BUFSIZE)
    VerbosePrint('.')
  dstfile.flush()
  VerbosePrint('\n')
  return totsize


def _CopyAndVerify(description, inf, outf):
  """Copy data from file object inf to file object outf, then verify it."""
  start = inf.tell()
  written = WriteToFile(inf, outf)
  inf.seek(start, os.SEEK_SET)
  outf.seek(0, os.SEEK_SET)
  if not IsIdentical(description, inf, outf):
    raise IOError('Read-after-write verification failed')
  return written


# TODO(apenwarr): consider using the nandwrite command here.
#  Otherwise we won't correctly skip NAND badblocks. However, before we
#  do that we should validate whether the bootloader also handles badblocks
#  (the exact same badblocks as the kernel) or it could maybe make things
#  worse.
def InstallToMtd(f, mtddevname):
  """Write an image to an mtd device."""
  if EraseMtd(mtddevname):
    raise IOError('Flash erase failed.')
  VerbosePrint('Writing to mtd partition %r\n', mtddevname)
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
    ubino: the ubi device number to use.

  Raises:
    IOError: when ubi format fails

  Returns:
    number of bytes written.
  """
  ubino = PickFreeUbi()
  SilentCmd('ubidetach', '-p', mtddevname)
  Cmd('ubiformat', mtddevname, '-y', '-q')
  Cmd('ubiattach', '-p', mtddevname, '-d', str(ubino))
  try:
    Cmd('ubimkvol', '/dev/ubi%d' % ubino, '-N', 'rootfs-prep', '-m')
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
    return open(SIGNINGKEY).read()
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


class FileImage(object):
  """A system image packaged as separate kernel, rootfs and loader files."""

  def __init__(self, kernelfile, rootfs, loader, loadersig, manifest, uloader):
    self.kernelfile = kernelfile
    self.rootfs = rootfs
    self.loader = loader
    self.loadersig = loadersig
    self.uloader = uloader
    if manifest:
      self.manifest = ParseManifest(open(manifest))
    else:
      self.manifest = default_manifest_files.copy()
      self.manifest['platforms'] = [GetPlatform()]

  def ManifestVersion(self):
    return self.manifest['installer_version']

  def GetVersion(self):
    return None

  def GetLoader(self):
    if self.loader:
      try:
        return open(self.loader, 'rb')
      except IOError, e:
        raise Fatal(e)
    else:
      return None

  def GetUloader(self):
    if self.uloader:
      try:
        return open(self.uloader, 'rb')
      except IOError, e:
        raise Fatal(e)
    else:
      return None

  def GetKernel(self):
    if self.kernelfile:
      try:
        return open(self.kernelfile, 'rb')
      except IOError, e:
        raise Fatal(e)
    else:
      return None

  def GetRootFs(self):
    if self.rootfs:
      try:
        return open(self.rootfs, 'rb')
      except IOError, e:
        raise Fatal(e)
    else:
      return None

  def GetLoaderSig(self):
    if self.loadersig:
      try:
        return open(self.loadersig, 'rb')
      except IOError, e:
        raise Fatal(e)
    else:
      return None


class TarImage(object):
  """A system image packaged as a tar file."""

  def __init__(self, tarfilename):
    self.tarfilename = tarfilename
    self.tar_f = tarfile.open(name=tarfilename)
    fnames = self.tar_f.getnames()
    self.rootfs = None

    for fname in fnames:
      if fname.startswith('rootfs.'):
        self.rootfs = fname
        break

    try:
      f = self.tar_f.extractfile('manifest')
    except KeyError:
      # No manifest; it must be an old-style installer.
      # Generate an auto-manifest compatible with older files.
      self.manifest = default_manifest_v2.copy()
      try:
        self.manifest['version'] = (
            self.tar_f.extractfile('version').read(4096).strip())
      except KeyError:
        pass
    else:
      self.manifest = ParseManifest(f)

  def ManifestVersion(self):
    return int(self.manifest['installer_version'])

  def GetVersion(self):
    try:
      return self.manifest['version']
    except KeyError:
      raise Fatal('Fatal: image file has no version field')

  def GetKernel(self):
    # TV boxes use a raw vmlinu* file, the gflt* install a uImage to
    # the kernel partition.
    if self.ManifestVersion() > 2:
      kernel_names = ['kernel.img']
    else:
      kernel_names = ['vmlinuz', 'vmlinux', 'uImage']
    for name in kernel_names:
      try:
        return self.tar_f.extractfile(name)
      except KeyError:
        pass
    return None

  def GetRootFs(self):
    if not self.rootfs:
      return None
    try:
      return self.tar_f.extractfile(self.rootfs)
    except KeyError:
      return None

  def GetLoader(self):
    try:
      filename = 'loader.img' if self.ManifestVersion() > 2 else 'loader.bin'
      return self.tar_f.extractfile(filename)
    except KeyError:
      return None

  def GetLoaderSig(self):
    try:
      return self.tar_f.extractfile('loader.sig')
    except KeyError:
      return None

  def GetUloader(self):
    try:
      # Image versions prior to v3 never included a uloader.
      return self.tar_f.extractfile('uloader.img')
    except KeyError:
      return None


def WriteLoaderToMtd(loader, loader_start, mtd, description):
  is_loader_current = False
  with open(mtd, 'rb') as mtdfile:
    VerbosePrint('Checking if the %s is up to date.\n', description)
    loader.seek(loader_start)
    is_loader_current = IsIdentical(description, loader, mtdfile)
  if is_loader_current:
    VerbosePrint('The %s is the latest.\n', description)
  else:
    loader.seek(loader_start, os.SEEK_SET)
    Log('DO NOT INTERRUPT OR POWER CYCLE, or you will brick the unit.\n')
    VerbosePrint('Writing to %r\n', mtd)
    InstallToMtd(loader, mtd)


def main():
  global quiet  # gpylint: disable-msg=global-statement
  o = options.Options(optspec)
  opt, unused_flags, unused_extra = o.parse(sys.argv[1:])

  if not (opt.drm or opt.kernel or opt.rootfs or opt.loader or opt.tar or
          opt.partition):
    o.fatal('Expected at least one of -p, -k, -r, -t, --loader, or --drm')

  quiet = opt.quiet

  if opt.drm:
    WriteDrm(opt)

  if (opt.kernel or opt.rootfs or opt.tar) and not opt.partition:
    # default to the safe option if not given
    opt.partition = 'other'

  if opt.partition == 'other':
    boot = GetBootedPartition()
    assert boot in [None, 0, 1]
    if boot is None:
      # Policy decision: if we're booted from NFS, install to secondary
      partition = 1
    else:
      partition = boot ^ 1
  elif opt.partition in ['primary', 0]:
    partition = 0
  elif opt.partition in ['secondary', 1]:
    partition = 1
  elif opt.partition:
    o.fatal('--partition must be one of: primary, secondary, other')
  elif opt.tar or opt.kernel or opt.rootfs:
    o.fatal('A --partition option must be provided with -k, -r, or -t')
  else:
    partition = None

  if opt.tar or opt.kernel or opt.rootfs or opt.loader:
    key = GetKey()
    if opt.tar:
      img = TarImage(opt.tar)
      if opt.kernel or opt.rootfs or opt.loader or opt.loadersig:
        o.fatal('--tar option is incompatible with -k, -r, '
                '--loader and --loadersig')
    else:
      img = FileImage(opt.kernel, opt.rootfs, opt.loader, opt.loadersig,
                      opt.manifest, opt.uloader)

    # old software versions are incompatible with this version of ginstall.
    # In particular, we want to leave out versions that:
    #  - don't support 1GB NAND layout.
    #  - use pre-ubinized files instead of raw rootfs images.
    ver = img.GetVersion()
    if ver and (
        ver.startswith('bruno-') or
        (ver.startswith('gfibertv-') and ver < 'gfibertv-24')):
      raise Fatal('%r is too old: aborting.\n' % ver)

    manifest = img.manifest
    CheckPlatform(manifest)

    rootfs = img.GetRootFs()
    if rootfs:
      partition_name = 'rootfs%d' % partition
      mtd = GetMtdDevForNameOrNone(partition_name)
      gpt = GetGptPartitionForName(partition_name)
      if mtd:
        Log('Installing raw rootfs image to ubi partition %r\n' % mtd)
        VerbosePrint('Writing raw rootfs to %r\n', mtd)
        InstallRawFileToUbi(rootfs, mtd)
      elif gpt:
        VerbosePrint('Writing raw rootfs to %r\n', gpt)
        InstallToFile(rootfs, gpt)
      else:
        raise Fatal('no partition named %r is available' % partition_name)

    kern = img.GetKernel()
    if kern:
      if IsDeviceNoSigning():
        buf = kern.read(4100)
        if buf[0:3] != GZIP_HEADER and buf[4096:4099] == GZIP_HEADER:
          VerbosePrint('Incompatible device: removing kernel signing.\n')
          kern.seek(4096)
        elif buf[0:3] == GZIP_HEADER:
          kern.seek(0)
        else:
          raise Fatal('Incompatible device: unrecognized kernel format')
      partition_name = 'kernel%d' % partition
      mtd = GetMtdDevForNameOrNone(partition_name)
      gpt = GetGptPartitionForName(partition_name)
      if mtd:
        VerbosePrint('Writing kernel to %r\n' % mtd)
        InstallToMtd(kern, mtd)
      elif gpt:
        VerbosePrint('Writing kernel to %r\n' % gpt)
        InstallToFile(kern, gpt)
      else:
        raise Fatal('no partition named %r is available' % partition_name)

    loader = img.GetLoader()
    if loader:
      loader_start = loader.tell()
      if opt.skiploader:
        VerbosePrint('Skipping loader installation.\n')
      else:
        # TODO(jnewlin): Temporary hackage.  v3 of ginstall will have a
        # signature over the entire file as opposed to just on the loader and
        # we can drop this loader signature.  For now allow a command line
        # opt to disable signature checking.
        if not opt.skiploadersig:
          loadersig = img.GetLoaderSig()
          if not loadersig:
            raise Fatal('Loader signature file is missing; try --loadersig')
          if not Verify(loader, loadersig, key):
            raise Fatal('Loader signing check failed.')
        installed = False
        for i in ['cfe', 'loader', 'loader0', 'loader1']:
          mtd = GetMtdDevForNameOrNone(i)
          if mtd:
            WriteLoaderToMtd(loader, loader_start, mtd, 'loader')
            installed = True
        if not installed:
          raise Fatal('no loader partition is available')

    uloader = img.GetUloader()
    if uloader:
      uloader_start = uloader.tell()
      if opt.skiploader:
        VerbosePrint('Skipping uloader installation.\n')
      else:
        mtd = GetMtdDevForNameOrNone('uloader')
        if mtd:
          uloader_signed = UloaderSigned(uloader)
          device_secure = DeviceIsSecure(mtd)
          if uloader_signed and not device_secure:
            VerbosePrint('Signed uloader but unsecure box; stripping sig.\n')
            uloader, uloader_start = StripUloader(uloader, uloader_start)
          elif not uloader_signed and device_secure:
            raise Fatal('Unable to install unsigned uloader on secure device.')
          WriteLoaderToMtd(uloader, uloader_start, mtd, 'uloader')

  if partition is not None:
    SetBootPartition(partition)

  return 0


def DeviceIsSecure(uloader_mtddevname):
  """Determines whether the gfrg200 device is secure.

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


if __name__ == '__main__':
  try:
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
    sys.exit(main())
  except Fatal, e:
    Log('%s\n', e)
    sys.exit(1)
