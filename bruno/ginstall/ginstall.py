#!/usr/bin/python
# Copyright 2011 Google Inc. All Rights Reserved.

"""Image installer for GFHD100.
"""

__author__ = 'dgentry@google.com (Denton Gentry)'

import optparse
import os
import re
import subprocess
import sys
import tarfile


# unit tests can override these with fake versions
FLASH_ERASE = "/usr/sbin/flash_erase"
PROC_MTD = "/proc/mtd"
MTDBLOCK = "/dev/mtdblock{0}"
UBIFORMAT = "/usr/sbin/ubiformat"
BUFSIZE = 256 * 1024

# Verbosity of output
quiet = False

def verbose_print(string):
  if not quiet:
    sys.stdout.write(string)
    sys.stdout.flush()


def get_mtd_num(arg):
  """Return the integer number of an mtd device, given its name or number."""
  try:
    return int(arg)
  except ValueError:
    pass
  m = re.match(r"(/dev/){0,1}mtd(\d+)", arg)
  if m:
    return int(m.group(2))
  return False


def get_erase_size(mtd):
  """Find the erase block size of an mtd device.

  Args:
    mtd - integer number of the MTD device, or its name. Ex: 3, or "mtd3"

  Returns:
    The erase size as an integer, 0 if not found.
  """
  mtd = "mtd" + str(get_mtd_num(mtd))
  splt = re.compile('[ :]+')
  f = open(PROC_MTD)
  for line in f:
    fields = splt.split(line)
    if len(fields) >= 2 and fields[0].strip() == mtd:
      return int(fields[2], 16)
  return 0


def round_to(orig, mult):
  """Round orig up to a multiple of mult."""
  return ((orig + mult - 1) // mult) * mult


def erase_mtd(mtd):
  """Erase an mtd partition.

  Args:
    mtd: integer number of the MTD device, or its name. Ex: 3, or "mtd3"

  Returns:
    true if successful.
  """
  devmtd = "/dev/mtd" + str(get_mtd_num(mtd))
  cmd = [FLASH_ERASE, "--quiet", devmtd, "0", "0"]
  devnull = open('/dev/null', 'w')
  return subprocess.call(cmd, stdout=devnull, stderr=devnull)


def write_to_file(srcfile, dstfile):
  """Copy all bytes from srcfile to dstfile."""
  buf = srcfile.read(BUFSIZE)
  totsize = 0
  while buf:
    totsize += len(buf)
    dstfile.write(buf)
    buf = srcfile.read(BUFSIZE)
    verbose_print('.')
  return totsize


def read_and_verify(srcfile, dstfile):
  """Read srcfile and dstfile. Return true if contents are identical."""
  sbuf = srcfile.read(BUFSIZE)
  dbuf = dstfile.read(len(sbuf))
  while sbuf and dbuf:
    if sbuf != dbuf:
      return False
    sbuf = srcfile.read(BUFSIZE)
    dbuf = dstfile.read(len(sbuf))
    verbose_print('.')
  return True


def get_file_size(f):
  """Return size of a file like object."""
  current = f.tell()
  f.seek(0, os.SEEK_END)
  size = f.tell()
  f.seek(current, os.SEEK_SET)
  return size


def install_to_mtd(f, mtd):
  """Write an image to an mtd device."""
  if erase_mtd(mtd):
    raise IOError("Flash erase failed.")
  mtdblockname = MTDBLOCK.format(get_mtd_num(mtd))
  with open(mtdblockname, "r+b") as mtdfile:
    written = write_to_file(f, mtdfile)
    f.seek(0, os.SEEK_SET)
    mtdfile.seek(0, os.SEEK_SET)
    if not read_and_verify(f, mtdfile):
      raise IOError("Flash verify failed")
    return written


def install_to_ubi(f, mtd):
  """Write an image to a ubi device.

  Args:
    f - a file-like object holding the image to be installed.
    mtd - the mtd partition to install to.

  Returns: number of bytes written.
  """
  fsize = get_file_size(f)
  writesize = round_to(fsize, get_erase_size(mtd))
  devmtd = "/dev/mtd" + str(get_mtd_num(mtd))
  cmd = [UBIFORMAT, devmtd, "-f", "-", "-y", "-q", "-S", str(writesize)]
  null = open('/dev/null', 'w')
  ub = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=null, stderr=null)
  siz = write_to_file(f, ub.stdin)
  ub.stdin.close()  # send EOF to UBIFORMAT
  rc = ub.wait()
  if rc != 0 or siz != fsize:
    raise IOError("ubi format failed")
  return siz


class FileImage(object):
  """A system image packaged as separate kernel and rootfs files."""
  def __init__(self, kernelfile, rootfsfile):
    self.kernelfile = kernelfile
    self.rootfsfile = rootfsfile

  def GetKernel(self):
    return open(self.kernelfile, "rb")

  def GetRootFs(self):
    return open(self.rootfsfile, "rb")


class TarImage(object):
  """A system image packaged as a tar file."""
  def __init__(self, tarfilename):
    self.tarfilename = tarfilename
    self.tar_f = tarfile.open(name=tarfilename)

  def GetKernel(self):
    try:
      return self.tar_f.extractfile("vmlinuz.ubi")
    except KeyError:
      try:
        return self.tar_f.extractfile("vmlinux.ubi")
      except KeyError:
        return None

  def GetRootFs(self):
    try:
      return self.tar_f.extractfile("rootfs.squashfs_ubi")
    except KeyError:
      try:
        return self.tar_f.extractfile("rootfs.ubi")
      except KeyError:
        return None


gfhd100_partitions = {"primary": ("mtd3", "mtd5"),
                      "secondary": ("mtd4", "mtd6")}

def main():
  parser = optparse.OptionParser()
  parser.add_option('-t', '--tar', dest='tarfile',
                    help='tar archive containing kernel and rootfs',
                    default=None)
  parser.add_option('-k', '--kernel', dest='kernfile',
                    help='kernel image to install',
                    default=None)
  parser.add_option('-r', '--rootfs', dest='rootfsfile',
                    help='rootfs UBI image to install',
                    default=None)
  parser.add_option('-p', '--partition', dest='partition', metavar="PART",
                    type="string", action="store",
                    help='primary or secondary image partition',
                    default=None)
  parser.add_option('-q', '--quiet', dest='quiet', action='store_true',
                    help="suppress unnecessary output.",
                    default=False)

  (options, args) = parser.parse_args()
  quiet = options.quiet
  if options.tarfile:
    img = TarImage(options.tarfile)
    if options.kernfile or options.rootfsfile:
      print("--tar option provided, ignoring --kernel and --rootfs")
  else:
    img = FileImage(options.kernfile, options.rootfsfile)
  if not options.partition:
    print("A --partition option must be provided.")
    return 1
  if options.partition not in gfhd100_partitions:
    print("--partition must be one of: " + str(gfhd100_partitions.keys()))

  mtds = gfhd100_partitions[options.partition]
  kern = img.GetKernel()
  if kern:
    verbose_print("Writing kernel to {0}".format(mtds[0]))
    install_to_ubi(kern, mtds[0])
    verbose_print("\n")

  rootfs = img.GetRootFs()
  if rootfs:
    verbose_print("Writing rootfs to {0}".format(mtds[1]))
    install_to_ubi(rootfs, mtds[1])
    verbose_print("\n")

  # TODO(dgentry) set NVRAM variables automatically
  print("To use this SW image, variables must be manually set in the bootloader.")
  print('setenv -p BOOT "FLASH"')
  print('setenv -p MTD_TYPE_FOR_KERNEL "mtd"')
  if options.partition == "primary":
    print('setenv -p ACTIVATED_KERNEL_NUM 0')
    print('setenv -p KERNEL_OPT "ubi.mtd=5 root=ubi0:rootfs rootfstype=ubifs" (plus any ip= options)')
  else:
    print('setenv -p ACTIVATED_KERNEL_NUM 1')
    print('setenv -p KERNEL_OPT "ubi.mtd=6 root=ubi0:rootfs rootfstype=ubifs" (plus any ip= options)')
  return 0


if __name__ == '__main__':
  sys.exit(main())
