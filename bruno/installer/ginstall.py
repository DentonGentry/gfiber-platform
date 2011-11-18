#!/usr/bin/python
# Copyright 2011 Google Inc. All Rights Reserved.

"""Image installer for GFHD100.
"""

__author__ = 'dgentry@google.com (Denton Gentry)'

import optparse
import os
import re
import stat
import subprocess
import tarfile


# unit tests can override these with fake versions
FLASH_ERASE = "/usr/sbin/flash_erase"
PROC_MTD = "/proc/mtd"
MTDBLOCK = "/dev/mtdblock{0}"
UBIFORMAT = "/usr/sbin/ubiformat"


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
  bsiz = 256 * 1024
  buf = srcfile.read(bsiz)
  totsize = 0
  while buf:
    totsize += len(buf)
    dstfile.write(buf)
    buf = srcfile.read(bsiz)
  return totsize


def get_file_size(f):
  """Return size of a file like object."""
  current = f.tell()
  f.seek(0, os.SEEK_END)
  size = f.tell()
  f.seek(current, os.SEEK_SET)
  return size


def install_to_mtd(f, mtd):
  if erase_mtd(mtd):
    raise IOError("Flash erase failed.")
  mtdblockname = MTDBLOCK.format(get_mtd_num(mtd))
  with open(mtdblockname, "wb") as mtdfile:
    return write_to_file(f, mtdfile)


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

  def __del__(self):
    self.tar_f.close()
    self.tar_f = None

  def GetKernel(self):
    try:
      return self.tar_f.extractfile("vmlinuz")
    except KeyError:
      try:
        return self.tar_f.extractfile("vmlinux")
      except KeyError:
        return None

  def GetRootFs(self):
    try:
      return self.tar_f.extractfile("rootfs.ubi")
    except KeyError:
      return None


def main():
  parser = optparse.OptionParser()
  parser.add_option('--tar', dest='tarfile',
                    help='tar archive containing kernel and rootfs',
                    default=None)
  parser.add_option('--kernel', dest='kernfile',
                    help='kernel image to install',
                    default=None)
  parser.add_option('--rootfs', dest='rootfsfile',
                    help='rootfs UBI image to install',
                    default=None)
  args = parser.parse_args()
  if (args.tarfile):
    img = TarImage(args.tarfile)
    if args.kernfile or args.rootfsfile:
      print("--tar option provided, ignoring --kernel and --rootfs")
  else:
    img = FileImage(args.kernfile, args.rootfsfile)


if __name__ == '__main__':
  main()
