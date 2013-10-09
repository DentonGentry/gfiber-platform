#!/usr/bin/python
# Copyright 2011 Google Inc. All Rights Reserved.

"""Image installer for GFHD100."""

__author__ = 'dgentry@google.com (Denton Gentry)'

import collections
import os
import re
import subprocess
import sys
import tarfile
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
"""


# unit tests can override these with fake versions
BUFSIZE = 256 * 1024
ETCPLATFORM = '/etc/platform'
FLASH_ERASE = 'flash_erase'
HNVRAM = 'hnvram'
MKDOSFS = 'mkdosfs'
MMCBLK = '/dev/mmcblk0'
MOUNT = 'mount'
MTDBLOCK = '/dev/mtdblock{0}'
PROC_CMDLINE = '/proc/cmdline'
PROC_MTD = '/proc/mtd'
SGDISK = 'sgdisk'
SIGNINGKEY = '/etc/gfiber_public.der'
SYS_UBI0 = '/sys/class/ubi/ubi0/mtd_num'
UBIFORMAT = 'ubiformat'
UBIPREFIX = 'ubi'
UMOUNT = 'umount'
ROOTFSUBI_NO = '5'
GZIP_HEADER = '\x1f\x8b\x08'  # encoded as string to ignore endianness


# Verbosity of output
quiet = False

# Partitions supported
gfhd100_partitions = {'primary': 0, 'secondary': 1}

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
  key = RSA.importKey(k.read())
  h = SHA512.new(f.read())
  v = PKCS1_v1_5.new(key)
  return v.verify(h, s.read())


def Log(s, *args):
  sys.stdout.flush()
  if args:
    sys.stderr.write(s % args)
  else:
    sys.stderr.write(s)


def VerbosePrint(s, *args):
  if not quiet:
    Log(s, *args)


def GetPlatform():
  try:
    with open(ETCPLATFORM) as f:
      return f.read().strip()
  except IOError as e:
    raise Fatal(e)


def SetBootPartition(partition):
  extra = 'ubi.mtd=rootfs{0} root=mtdblock:rootfs rootfstype=squashfs'.format(
      partition)
  cmd = [HNVRAM,
         '-w', 'MTD_TYPE_FOR_KERNEL=RAW',
         '-w', 'ACTIVATED_KERNEL_NAME=kernel{0}'.format(partition),
         '-w', 'EXTRA_KERNEL_OPT={0}'.format(extra)]
  devnull = open('/dev/null', 'w')
  return subprocess.call(cmd, stdout=devnull)


def GetBootedPartitionUbi():
  """Get the boot partition from the value in UBI."""
  try:
    f = open(SYS_UBI0)
    line = f.readline().strip()
  except IOError:
    return None
  booted_mtd = 'mtd' + str(int(line))
  for (pname, pnum) in gfhd100_partitions.items():
    rootfs = 'rootfs' + str(pnum)
    mtd = GetMtdDevForName(rootfs)
    if booted_mtd == mtd:
      return pname
  return None


def GetBootedPartitionCmdLine():
  """Get the boot partition by reading the cmdline."""
  try:
    with open(PROC_CMDLINE) as f:
      cmdline = f.read().strip()
  except IOError:
    return None
  for arg in cmdline.split(' '):
    if arg.startswith('root='):
      partition = arg.split('=')[1]
      if partition == 'rootfs0':
        return 'primary'
      elif partition == 'rootfs1':
        return 'secondary'
  return None


def GetBootedPartition():
  """Get the role of partition where the running system is booted from.

  Returns:
    "primary" or "secondary" boot partition, or None if not booted from flash.
  """
  # For devices that have UBI, read the booted partition from ubi
  # otherwise check the kernel command line to see for the root= option
  # passed in from the bootloader.
  if os.path.exists(SYS_UBI0):
    return GetBootedPartitionUbi()
  else:
    return GetBootedPartitionCmdLine()


def GetOtherPartition(partition):
  """Get the role of the other partition.

  Args:
    partition: current parition role.

  Returns:
    The name of the other partition.
    If partion=primary, will return 'secondary'.
  """
  for (pname, _) in gfhd100_partitions.items():
    if pname != partition:
      return pname
  return None


def GetMtdNum(arg):
  """Return the integer number of an mtd device, given its name or number."""
  try:
    return int(arg)
  except ValueError:
    pass
  m = re.match(r'(/dev/){0,1}mtd(\d+)', arg)
  if m:
    return int(m.group(2))
  return False


def GetEraseSize(mtd):
  """Find the erase block size of an mtd device.

  Args:
    mtd: integer number of the MTD device, or its name. Ex: 3, or "mtd3"

  Returns:
    The erase size as an integer, 0 if not found.
  """
  mtd = 'mtd' + str(GetMtdNum(mtd))
  splt = re.compile('[ :]+')
  f = open(PROC_MTD)
  for line in f:
    fields = splt.split(line.strip())
    if len(fields) >= 3 and fields[0] == mtd:
      return int(fields[2], 16)
  return 0


def GetMtdDevForName(name):
  """Find the mtd# for a named partition.

  In /proc/mtd we have:

  dev:    size   erasesize  name
  mtd0: 00200000 00010000 "cfe"
  mtd1: 00200000 00010000 "reserve0"
  mtd2: 10000000 00100000 "kernel0"
  mtd3: 10000000 00100000 "kernel1"

  Args:
    name: the partition to find. For example, "kernel0"

  Returns:
    The mtd device, for example "mtd2"
  """
  splt = re.compile('[ :]+')
  quotedname = '"' + name + '"'
  f = open(PROC_MTD)
  for line in f:
    fields = splt.split(line.strip())
    if len(fields) >= 4 and fields[3] == quotedname:
      return fields[0]
  return None


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


def GetMtdDevForNameList(names):
  """Iterate through partition names and return a device for the first match.

  Args:
    names: List of partitions names.

  Returns:
    The mtd of the first name to match, or None of there is no match.
  """
  for name in names:
    mtd = GetMtdDevForName(name)
    if mtd is not None:
      return mtd
  return None


def RoundTo(orig, mult):
  """Round orig up to a multiple of mult."""
  return ((orig + mult - 1) // mult) * mult


def EraseMtd(mtd):
  """Erase an mtd partition.

  Args:
    mtd: integer number of the MTD device, or its name. Ex: 3, or "mtd3"

  Returns:
    true if successful.
  """
  devmtd = '/dev/mtd' + str(GetMtdNum(mtd))
  cmd = [FLASH_ERASE, '--quiet', devmtd, '0', '0']
  devnull = open('/dev/null', 'w')
  return subprocess.call(cmd, stdout=devnull)


def WriteToFile(srcfile, dstfile):
  """Copy all bytes from srcfile to dstfile."""
  buf = srcfile.read(BUFSIZE)
  totsize = 0
  while buf:
    totsize += len(buf)
    dstfile.write(buf)
    buf = srcfile.read(BUFSIZE)
    VerbosePrint('.')
  return totsize


def IsIdentical(srcfile, dstfile):
  """Compare srcfile and dstfile. Return true if contents are identical."""
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
  return True


def GetFileSize(f):
  """Return size of a file like object."""
  current = f.tell()
  f.seek(0, os.SEEK_END)
  size = f.tell()
  f.seek(current, os.SEEK_SET)
  return size


def InstallToMtd(f, mtd):
  """Write an image to an mtd device."""
  if EraseMtd(mtd):
    raise IOError('Flash erase failed.')
  mtdblockname = MTDBLOCK.format(GetMtdNum(mtd))
  start = f.tell()
  with open(mtdblockname, 'r+b') as mtdfile:
    written = WriteToFile(f, mtdfile)
    f.seek(start, os.SEEK_SET)
    mtdfile.seek(0, os.SEEK_SET)
    if not IsIdentical(f, mtdfile):
      raise IOError('Flash verify failed')
    return written


def InstallToFile(orig_f, destination):
  """Write the file-like object orig_f to file named destination."""
  orig_start = orig_f.tell()
  with open(destination, 'w+b') as dest_f:
    written = WriteToFile(orig_f, dest_f)
    orig_f.seek(orig_start, os.SEEK_SET)
    dest_f.seek(0, os.SEEK_SET)
    if not IsIdentical(orig_f, dest_f):
      raise IOError('Flash verify failed')
    return written


def InstallUbiFileToUbi(f, mtd):
  """Write an image with ubi header to a ubi device.

  Args:
    f: a file-like object holding the image to be installed.
    mtd: the mtd partition to install to.

  Raises:
    IOError: when ubi format fails

  Returns:
    number of bytes written.
  """
  fsize = GetFileSize(f)
  writesize = RoundTo(fsize, GetEraseSize(mtd))
  devmtd = '/dev/mtd' + str(GetMtdNum(mtd))
  cmd = [UBIFORMAT, devmtd, '-f', '-', '-y', '-q', '-S', str(writesize)]
  ub = subprocess.Popen(cmd, stdin=subprocess.PIPE)
  siz = WriteToFile(f, ub.stdin)
  ub.stdin.close()  # send EOF to UBIFORMAT
  rc = ub.wait()
  if rc != 0 or siz != fsize:
    raise IOError('ubi format failed')
  return siz


def UbiCmd(name, args):
  """Wrapper for ubi calls."""
  cmd = collections.deque(args)
  cmd.appendleft(UBIPREFIX + name)
  rc = subprocess.call(cmd)
  if rc != 0:
    raise IOError('ubi ' + name + ' failed')


def InstallRawFileToUbi(f, mtd, ubino):
  """Write an image without ubi header to a ubi device.

  Args:
    f: a file-like object holding the image to be installed.
    mtd: the mtd partition to install to.
    ubino: the ubi device number to attached ubi partition.

  Raises:
    IOError: when ubi format fails

  Returns:
    number of bytes written.
  """
  devmtd = '/dev/mtd' + str(GetMtdNum(mtd))
  if os.path.exists('/dev/ubi' + ubino):
    UbiCmd('detach', ['-d', ubino])
  UbiCmd('format', [devmtd, '-y', '-q'])
  UbiCmd('attach', ['-m', str(GetMtdNum(mtd)), '-d', ubino])
  UbiCmd('mkvol', ['/dev/ubi' + ubino, '-N', 'rootfs-prep', '-m'])
  mtd = GetMtdDevForName('rootfs-prep')
  siz = InstallToMtd(f, mtd)
  UbiCmd('rename', ['/dev/ubi' + ubino, 'rootfs-prep', 'rootfs'])
  UbiCmd('detach', ['-d', ubino])
  return siz


def WriteDrm(opt):
  """Write DRM Keyboxes."""
  Log('DO NOT INTERRUPT OR POWER CYCLE, or you will lose drm capability.\n')
  try:
    drm = open(opt.drm, 'rb')
  except IOError, e:
    raise Fatal(e)
  mtd = GetMtdDevForName('drmregion0')
  VerbosePrint('Writing drm to %r', mtd)
  InstallToMtd(drm, mtd)
  VerbosePrint('\n')

  drm.seek(0)
  mtd = GetMtdDevForName('drmregion1')
  VerbosePrint('Writing drm to %r', mtd)
  InstallToMtd(drm, mtd)
  VerbosePrint('\n')


def GetKey():
  """Return the key to check file signatures."""
  try:
    return open(SIGNINGKEY)
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
  platform = GetPlatform().lower()
  platforms = manifest['platforms']
  for p in platforms:
    if p.lower() == platform:
      return True
  raise Fatal('Package supports %r, but this device is %r'
              % (platforms, platform))


class FileImage(object):
  """A system image packaged as separate kernel, rootfs and loader files."""

  def __init__(self, kernelfile, rootfs, loader, loadersig, manifest):
    self.kernelfile = kernelfile
    self.rootfs = rootfs
    if self.rootfs:
      self.rootfstype = rootfs[7:]
    else:
      self.rootfstype = None
    self.loader = loader
    self.loadersig = loadersig
    self.manifest = manifest

  def ManifestVersion(self):
    manifest = self.GetManifest()
    return manifest['installer_version']

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

  def GetKernel(self):
    if self.kernelfile:
      try:
        return open(self.kernelfile, 'rb')
      except IOError, e:
        raise Fatal(e)
    else:
      return None

  def IsRootFsUbi(self):
    if self.rootfstype[-4:] == '_ubi':
      return True
    return False

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

  def GetManifest(self):
    if self.manifest:
      try:
        return ParseManifest(open(self.manifest, 'r'))
      except IOError, e:
        raise Fatal(e)
    else:
      m = default_manifest_files.copy()
      m['platforms'] = [GetPlatform()]
      return m


class TarImage(object):
  """A system image packaged as a tar file."""

  def __init__(self, tarfilename):
    self.tarfilename = tarfilename
    self.tar_f = tarfile.open(name=tarfilename)
    fnames = self.tar_f.getnames()
    self.rootfstype = None
    for fname in fnames:
      if fname[:7] == 'rootfs.':
        self.rootfstype = fname[7:]
        break

  def ManifestVersion(self):
    manifest = self.GetManifest()
    return int(manifest['installer_version'])

  def GetVersion(self):
    # no point catching this error: if there's no version file, the
    # whole install image is definitely invalid.
    m = self.GetManifest()
    try:
      return m['version']
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

  def IsRootFsUbi(self):
    if self.rootfstype is None or self.ManifestVersion() > 2:
      return False
    if self.rootfstype and self.rootfstype[-4:] == '_ubi':
      return True
    return False

  def GetRootFs(self):
    if self.rootfstype is None:
      return None
    if self.ManifestVersion() > 2:
      filename = 'rootfs.img'
    elif self.IsRootFsUbi():
      filename = 'rootfs.squashfs_ubi'
    else:
      filename = 'rootfs.squashfs'
    try:
      return self.tar_f.extractfile(filename)
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

  def _GetDefaultManifest(self):
    m = default_manifest_v2.copy()
    try:
      version = self.tar_f.extractfile('version').read(4096).strip()
      m['version'] = version
    except KeyError:
      pass
    return m

  def GetManifest(self):
    try:
      f = self.tar_f.extractfile('manifest')
    except KeyError:
      return self._GetDefaultManifest()
    else:
      return ParseManifest(f)


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

  if opt.partition:
    if opt.partition == 'other':
      boot = GetBootedPartition()
      if boot is None:
        # Policy decision: if we're booted from NFS, install to secondary
        partition = 'secondary'
      else:
        partition = GetOtherPartition(boot)
    else:
      partition = opt.partition
    pnum = gfhd100_partitions[partition]
  else:
    partition = None
    pnum = None

  if opt.tar or opt.kernel or opt.rootfs:
    if not partition:
      o.fatal('A --partition option must be provided with -k, -r, or -t')
    if partition not in gfhd100_partitions:
      o.fatal('--partition must be one of: ' + str(gfhd100_partitions.keys()))

  if opt.tar or opt.kernel or opt.rootfs or opt.loader:
    key = GetKey()
    if opt.tar:
      img = TarImage(opt.tar)
      if opt.kernel or opt.rootfs or opt.loader or opt.loadersig:
        o.fatal('--tar option is incompatible with -k, -r, '
                '--loader and --loadersig')
    else:
      img = FileImage(opt.kernel, opt.rootfs, opt.loader, opt.loadersig,
                      opt.manifest)

    # old software versions are incompatible with this version of ginstall.
    # In particular, we want to leave out versions that:
    #  - don't support 1GB NAND layout.
    #  - use pre-ubinized files instead of raw rootfs images.
    ver = img.GetVersion()
    if ver and (
        ver.startswith('bruno-') or
        (ver.startswith('gfibertv-') and ver < 'gfibertv-24')):
      raise Fatal('%r is too old: aborting.\n' % ver)

    manifest = img.GetManifest()
    CheckPlatform(manifest)

    rootfs = img.GetRootFs()
    if rootfs:
      partition_name = 'rootfs' + str(pnum)
      mtd = GetMtdDevForName(partition_name)
      gpt = GetGptPartitionForName(partition_name)
      if mtd:
        if img.IsRootFsUbi():
          Log('Installing ubi-formatted rootfs.\n')
          VerbosePrint('Writing ubi rootfs to %r', mtd)
          InstallUbiFileToUbi(rootfs, mtd)
        else:
          Log('Installing raw rootfs image to ubi partition.\n')
          VerbosePrint('Writing raw rootfs to %r', mtd)
          InstallRawFileToUbi(rootfs, mtd, ROOTFSUBI_NO)
      elif gpt:
        if img.IsRootFsUbi():
          raise Fatal('Cannot install UBI rootfs to non-MTD %s' % gpt)
        VerbosePrint('Writing raw rootfs to %r', gpt)
        InstallToFile(rootfs, gpt)
      VerbosePrint('\n')

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
      partition_name = 'kernel' + str(pnum)
      mtd = GetMtdDevForName('kernel' + str(pnum))
      gpt = GetGptPartitionForName(partition_name)
      if mtd:
        VerbosePrint('Writing kernel to %s' % mtd)
        InstallToMtd(kern, mtd)
      elif gpt:
        VerbosePrint('Writing kernel to %s' % gpt)
        InstallToFile(kern, gpt)
      VerbosePrint('\n')

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
        mtd = GetMtdDevForNameList(['loader', 'cfe'])
        is_loader_current = False
        mtdblockname = MTDBLOCK.format(GetMtdNum(mtd))
        with open(mtdblockname, 'r+b') as mtdfile:
          VerbosePrint('Checking if the loader is up to date.')
          loader.seek(loader_start)
          is_loader_current = IsIdentical(loader, mtdfile)
        VerbosePrint('\n')
        if is_loader_current:
          VerbosePrint('The loader is the latest.\n')
        else:
          loader.seek(loader_start, os.SEEK_SET)
          Log('DO NOT INTERRUPT OR POWER CYCLE, or you will brick the unit.\n')
          VerbosePrint('Writing loader to %r', mtd)
          InstallToMtd(loader, mtd)
          VerbosePrint('\n')

  if partition:
    VerbosePrint('Setting boot partition to kernel%d\n', pnum)
    SetBootPartition(pnum)

  return 0


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
