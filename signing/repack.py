#!/usr/bin/python
# Copyright 2012 Google Inc. All Rights Reserved.

"""Repackage image for signing check.

The image is packaged as follows
Signing will add 8 bytes header (header size and signature offset).
The info is free format. Now it is only a string to hold verity table.
|-------------------| <=== Byte 0
| header size       | (integer)
|-------------------| <=== Byte 4
| signature offset  | (integer) pointing to the signature below.
|-------------------| <=== Byte 8
| padding           | Padding
|-------------------| <=== Byte 16
| info (e.g. verity | (4080 bytes)
|  table)           |
|-------------------| <=== Byte 4096
| vmlinuz           | this block is padded to align with 4096 Byte block.
|-------------------| <=== Byte N x 4096
| verity hash table |
|-------------------|
| signature         |
|-------------------|
"""

__author__ = 'kedong@google.com (Ke Dong)'

import binascii
import optparse
import os
import re
import shutil
import struct
import subprocess
import sys
import OpenSSL  #gpylint: disable-msg=F0401

BLOCK_SIZE = 4096
VERITY_START = '[VERITY-START]'
VERITY_STOP = '[VERITY-STOP]'
quiet = False


def GetRandom():
  """Generate a random hexstring of 64 bytes."""
  return binascii.hexlify(OpenSSL.rand.bytes(32))


def CheckOutput(args, **kwargs):
  nkwargs = dict(stdout=subprocess.PIPE)
  nkwargs.update(kwargs)
  p = subprocess.Popen(args, **nkwargs)
  data = p.stdout.read()
  retval = p.wait()
  if retval:
    raise Exception('%r returned exit code %d' % (args, retval))
  return data.strip()


def GenerateVerityTable(hostdir, bindir, rootfs):
  """Generate the verity table."""
  return CheckOutput(
      [os.path.join(hostdir, 'usr/bin', 'verity'), 'mode=create',
       'alg=sha256', 'payload=' + os.path.join(bindir, rootfs),
       'salt=' + GetRandom(), 'hashtree=' + os.path.join(bindir, 'hash.bin')])


def CeilingBlock(size):
  """Calculate the number of blocks based on the size in bytes."""
  return (size + BLOCK_SIZE -1) / BLOCK_SIZE


def UpdateVerityTable(table, hash_offset):
  """Update the verity table with correct hash table offset.

  Args:
    table:  the verity hash table content
    hash_offset: the hash table offset in sector (each sector has 512 bytes)

  Returns:
    updated verity hash table content
  """
  find_offset = re.compile(r'hashstart=(\d+)')
  return find_offset.sub(r'hashstart=' + str(hash_offset), table)


def FakeSign(fname):
  """Sign the image with valid header but no signature.

  Args:
    fname:  the path to the image file to be signed.
  """
  size = os.stat(fname).st_size
  with open(fname, 'r+b') as f:
    c = f.read()
    f.seek(0)
    f.write(struct.pack('I', size))
    f.write(struct.pack('I', 0x90091efb))
    f.write(struct.pack('I', 0x0))
    f.write(struct.pack('I', 0x0))
    f.write(c)


def RealSign(hostdir, key, ifname, ofname):
  """Sign the image with production key."""
  p = subprocess.Popen([os.path.join(hostdir, 'usr/bin', 'brcm_sign_enc')],
                       stdin=subprocess.PIPE, shell=False)
  for cmd in ['sign_kernel_file', ifname, ofname + '-sig.bin', 'l', key,
              ofname]:
    p.stdin.write(cmd + '\n')
  p.stdin.close()
  retval = p.wait()
  if retval:
    raise Exception('brcm_sign_enc returned exit code %d' % retval)


def PackVerity(kname, vname, info):
  """Pack verity information in the final image.

  Args:
    kname:  the path to the kernel file.
    vname:  the path to the verity hash table file.
    info:   the original verity table.
  """
  with open(kname, 'r+b') as f:
    c = f.read()
    padding_size = CeilingBlock(c.__len__())*BLOCK_SIZE - c.__len__()
    # offset is in number of sectors
    offset = (1 + CeilingBlock(c.__len__())) << 3
    f.seek(0)
    verity_table = UpdateVerityTable(info, offset)
    if not quiet:
      print verity_table
    f.write('%-4080.4080s' % (VERITY_START + verity_table + VERITY_STOP))
    f.write(c)
    if padding_size > 0:
      f.write('\0'*padding_size)
    with open(vname, 'rb') as v:
      f.write(v.read())


def main():
  global quiet  #gpylint: disable-msg=W0603
  parser = optparse.OptionParser()
  parser.add_option('-r', '--rootfs', dest='rootfs',
                    help='rootfs file name',
                    default='rootfs.squashfs')
  parser.add_option('-k', '--kernel', dest='kernel',
                    help='kernel file name',
                    default='vmlinuz')
  parser.add_option('-o', '--hostdir', dest='hostdir',
                    help='host directory',
                    default=None)
  parser.add_option('-b', '--bindir', dest='bindir',
                    help='binary directory',
                    default=None)
  parser.add_option('-s', '--sign', dest='sign',
                    action='store_true',
                    help='sign image with production key',
                    default=False)
  parser.add_option('-q', '--quiet', dest='quiet',
                    action='store_true',
                    help='suppress print',
                    default=False)
  (options, _) = parser.parse_args()
  quiet = options.quiet
  verity_table = GenerateVerityTable(options.hostdir, options.bindir,
                                     options.rootfs)
  PackVerity(os.path.join(options.bindir, options.kernel),
             os.path.join(options.bindir, 'hash.bin'),
             verity_table)
  if options.sign:
    olddir = os.getcwd()
    ifname = os.path.join(options.bindir, 'signing', options.kernel)
    ofname = os.path.join(options.bindir, options.kernel)
    shutil.copy(ofname, ifname)
    os.chdir(os.path.join(options.bindir, 'signing'))
    RealSign(options.hostdir, 'gfiber_private.pem', ifname, ofname)
    os.chdir(olddir)
  else:
    FakeSign(os.path.join(options.bindir, options.kernel))


if __name__ == '__main__':
  sys.exit(main())
