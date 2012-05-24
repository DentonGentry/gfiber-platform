#!/usr/bin/python
# Copyright 2011 Google Inc. All Rights Reserved.

"""Image installer for GFHD100."""

__author__ = 'dgentry@google.com (Denton Gentry)'

import optparse
import os
import re
import subprocess
import sys
import tarfile


# unit tests can override these with fake versions
BUFSIZE = 256 * 1024
FLASH_ERASE = '/usr/sbin/flash_erase'
HNVRAM = '/usr/bin/hnvram'
MTDBLOCK = '/dev/mtdblock{0}'
PROC_MTD = '/proc/mtd'
SYS_UBI0 = '/sys/class/ubi/ubi0/mtd_num'
UBIFORMAT = '/usr/sbin/ubiformat'

# Verbosity of output
quiet = False

# Partitions supported
gfhd100_partitions = {'primary': 0, 'secondary': 1}


def VerbosePrint(string):
  if not quiet:
    sys.stdout.write(string)
    sys.stdout.flush()


def SetBootPartition(partition):
  extra = 'ubi.mtd=rootfs{0} root=mtdblock:rootfs rootfstype=squashfs'.format(
      partition)
  cmd = [HNVRAM,
         '-w', 'MTD_TYPE_FOR_KERNEL=RAW',
         '-w', 'ACTIVATED_KERNEL_NAME=kernel{0}'.format(partition),
         '-w', 'EXTRA_KERNEL_OPT={0}'.format(extra)]
  devnull = open('/dev/null', 'w')
  return subprocess.call(cmd, stdout=devnull)


def GetBootedPartition():
  """Get the role of partition where the running system is booted from.

  Returns:
    "primary" or "secondary" boot partition, or None if not booted from flash.
  """
  try:
    f = open(SYS_UBI0)
    line = f.readline().strip()
  except IOError:
    return None
  booted_mtd = 'mtd' + str(int(line))
  for (pname, pnum) in gfhd100_partitions.items():
    rootfs = 'rootfs' + str(pnum)
    mtd = GetMtdDevForPartition(rootfs)
    if booted_mtd == mtd:
      return pname
  return None


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


def GetMtdDevForPartition(name):
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
  with open(mtdblockname, 'r+b') as mtdfile:
    written = WriteToFile(f, mtdfile)
    f.seek(0, os.SEEK_SET)
    mtdfile.seek(0, os.SEEK_SET)
    if not IsIdentical(f, mtdfile):
      raise IOError('Flash verify failed')
    return written


def InstallToUbi(f, mtd):
  """Write an image to a ubi device.

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
  null = open('/dev/null', 'w')
  ub = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=null)
  siz = WriteToFile(f, ub.stdin)
  ub.stdin.close()  # send EOF to UBIFORMAT
  rc = ub.wait()
  if rc != 0 or siz != fsize:
    raise IOError('ubi format failed')
  return siz


class FileImage(object):
  """A system image packaged as separate kernel, rootfs and loader files."""

  def __init__(self, kernelfile, rootfs, loader):
    self.kernelfile = kernelfile
    self.rootfs = rootfs
    self.loader = loader

  def GetLoader(self):
    if self.loader:
      try:
        return open(self.loader, 'rb')
      except IOError:
        print 'unable to open loader file %s' % self.loader
        return None
    else:
      return None

  def GetKernel(self):
    if self.kernelfile:
      try:
        return open(self.kernelfile, 'rb')
      except IOError:
        print 'unable to open kernel file %s' % self.kernelfile
        return None
    else:
      return None

  def GetRootFs(self):
    if self.rootfs:
      try:
        return open(self.rootfs, 'rb')
      except IOError:
        print 'unable to open rootfs file %s' % self.rootfs
        return None
    else:
      return None


class TarImage(object):
  """A system image packaged as a tar file."""

  def __init__(self, tarfilename):
    self.tarfilename = tarfilename
    self.tar_f = tarfile.open(name=tarfilename)

  def GetKernel(self):
    try:
      return self.tar_f.extractfile('vmlinuz')
    except KeyError:
      try:
        return self.tar_f.extractfile('vmlinux')
      except KeyError:
        return None

  def GetRootFs(self):
    try:
      return self.tar_f.extractfile('rootfs.squashfs_ubi')
    except KeyError:
      return None

  def GetLoader(self):
    try:
      return self.tar_f.extractfile('loader.bin')
    except KeyError:
      return None


def main():
  global quiet  #gpylint: disable-msg=W0603
  parser = optparse.OptionParser()
  parser.add_option('-t', '--tar', dest='tar',
                    help='tar archive containing kernel and rootfs',
                    default=None)
  parser.add_option('-k', '--kernel', dest='kern',
                    help='kernel image to install',
                    default=None)
  parser.add_option('-r', '--rootfs', dest='rootfs',
                    help='rootfs UBI image to install',
                    default=None)
  parser.add_option('--loader', dest='loader',
                    help='bootloader to install',
                    default=None)
  parser.add_option('--drm', dest='drmfile',
                    help='drm blob to install',
                    default=None)
  parser.add_option('-p', '--partition', dest='partition', metavar='PART',
                    type='string', action='store',
                    help='primary or secondary image partition, or "other"',
                    default=None)
  parser.add_option('-q', '--quiet', dest='quiet', action='store_true',
                    help='suppress unnecessary output.',
                    default=False)

  (options, _) = parser.parse_args()
  quiet = options.quiet
  if options.drmfile:
    print 'DO NOT INTERRUPT OR POWER CYCLE, or you will lose drm capability.'
    try:
      drm = open(options.drmfile, 'rb')
    except IOError:
      print 'unable to open drm file %s' % options.drmfile
      return 1
    mtd = GetMtdDevForPartition('drmregion0')
    VerbosePrint('Writing drm to {0}'.format(mtd))
    InstallToMtd(drm, mtd)
    VerbosePrint('\n')

    drm.seek(0)
    mtd = GetMtdDevForPartition('drmregion1')
    VerbosePrint('Writing drm to {0}'.format(mtd))
    InstallToMtd(drm, mtd)
    VerbosePrint('\n')

  if options.partition:
    if options.partition == 'other':
      boot = GetBootedPartition()
      if boot is None:
        # Policy decision: if we're booted from NFS, install to secondary
        partition = 'secondary'
      else:
        partition = GetOtherPartition(boot)
    else:
      partition = options.partition
  else:
    partition = None

  if options.tar or options.kern or options.rootfs:
    if not partition:
      print 'A --partition option must be provided.'
      return 1
    if partition not in gfhd100_partitions:
      print '--partition must be one of: ' + str(gfhd100_partitions.keys())
      return 1

  if options.tar or options.kern or options.rootfs or options.loader:
    if options.tar:
      img = TarImage(options.tar)
      if options.kern or options.rootfs or options.loader:
        print '--tar option provided, ignoring --kernel, --rootfs and --loader'
    else:
      img = FileImage(options.kern, options.rootfs, options.loader)

    pnum = gfhd100_partitions[partition]
    rootfs = img.GetRootFs()
    if rootfs:
      mtd = GetMtdDevForPartition('rootfs' + str(pnum))
      VerbosePrint('Writing rootfs to {0}'.format(mtd))
      InstallToUbi(rootfs, mtd)
      VerbosePrint('\n')

    kern = img.GetKernel()
    if kern:
      mtd = GetMtdDevForPartition('kernel' + str(pnum))
      VerbosePrint('Writing kernel to {0}'.format(mtd))
      InstallToMtd(kern, mtd)
      VerbosePrint('\n')

    loader = img.GetLoader()
    if loader:
      mtd = GetMtdDevForPartition('cfe')
      is_loader_current = False
      mtdblockname = MTDBLOCK.format(GetMtdNum(mtd))
      with open(mtdblockname, 'r+b') as mtdfile:
        VerbosePrint('Checking if the loader is up to date')
        is_loader_current = IsIdentical(loader, mtdfile)
      VerbosePrint('\n')
      if is_loader_current:
        VerbosePrint('The loader is the latest.\n')
      else:
        loader.seek(0, os.SEEK_SET)
        print 'DO NOT INTERRUPT OR POWER CYCLE, or you will brick the unit.'
        VerbosePrint('Writing loader to {0}'.format(mtd))
        InstallToMtd(loader, mtd)
        VerbosePrint('\n')

  if partition:
    pnum = gfhd100_partitions[partition]
    VerbosePrint('Setting boot partition to kernel{0}\n'.format(pnum))
    SetBootPartition(pnum)

  return 0


if __name__ == '__main__':
  sys.exit(main())
