#!/usr/bin/python
"""readubootver: read U-Boot version."""

import struct
import sys


MTD_FILE = '/proc/mtd'
CMDLINE_FILE = '/proc/cmdline'
UBOOT_MAGIC = 0x27051956


def ReadFile(name):
  try:
    with open(name) as f:
      return f.read()
  except IOError:
    print 'Failed to open file: ' + name
    sys.exit(1)


def GetRootPartition():
  """Identify the root partition from the kernel command line."""
  cmdline = ReadFile(CMDLINE_FILE)
  for opt in cmdline.split():
    if opt.startswith('root='):
      rootfs = opt.split('=')[1]
      if rootfs == 'rootfs0':
        return 'kernel0'
      if rootfs == 'rootfs1':
        return 'kernel1'
      print 'Unknown rootfs=' + rootfs
      sys.exit(1)
  return None


def GetBootMtds():
  """Get /dev file names for bootable MTDs."""
  all_mtds = ReadFile(MTD_FILE)
  boot_mtds = dict()
  for line in all_mtds.split('\n'):
    if not line.startswith('mtd'):
      continue
    if line.find('"kernel0"') > 0:
      boot_mtds['kernel0'] = line.split(':')[0]
    if line.find('"kernel1"') > 0:
      boot_mtds['kernel1'] = line.split(':')[0]
  return boot_mtds


def ReadUbootHeader(device):
  """Read U-Boot header from a /dev/mtd* device file."""
  try:
    with open(device, 'rb') as f:
      chunk = f.read(7*4 + 4 + 32)
  except IOError:
    print 'Can\'t read from device: ' + device
    return None

  vals = struct.unpack('>' + 'L'*7 + 'B'*4 + '32s', chunk)
  if vals[0] != UBOOT_MAGIC:
    print '%s: does not appear to be a valid uboot image' + device
    return None

  header = {
      'magic': vals[0],
      'size': vals[3],
      'dcrc': vals[6],
      'name': vals[11].rstrip('\x00'),
      }
  return header


def main():
  rootpart = GetRootPartition()
  if not rootpart:
    print 'Can\'t find the root parition.'
    sys.exit(1)

  mtd_devices = GetBootMtds()
  booted_mtd = GetRootPartition()

  for name, mtd in mtd_devices.iteritems():
    header = ReadUbootHeader('/dev/' + mtd)
    if not header:
      continue

    if name == booted_mtd:
      print 'BOOTED'
    else:
      print 'ALTERNATE'
    print 'mtd: %s' % mtd
    print 'size: %d' % header['size']
    print 'name: %s' % header['name']
    print ''

if __name__ == '__main__':
  main()
