#!/usr/bin/python
# Copyright 2012 Google Inc. All Rights Reserved.
"""Generate signature file for serial number.

This module generates the signature file for serial number. Since the
verification block alignment is 1MB, the original file to be signed use the
serial number to start with and the rest of the 1MB block is padded with
character 'G'.

The purpose is that CFE, in RMA recovery mode, will enable the USB driver and
look on the USB storage for a new CFE (presumably an unlocked one) to install.
However it will only install that CFE if there is also a signature file on the
USB that signs this particular device serial number.
"""

__author__ = 'kedong@google.com (Ke Dong)'

import os
import re
import subprocess
import sys
import options


optspec = """
signserial -o <hostdir> -b <bindir> -s <sn1> <sn2> ...
signserial -o <hostdir> -b <bindir> -f <sn file> ...
--
b,bindir=     binary directory
f,filename=   the name of the file that contains serial numbers
o,hostdir=    host directory
s,serialno=   serial number(s)
"""


BUFSIZE = 1024 * 1024
SN_FILE = 'sn.bin'
SN_SIG_FILE = 'sn-signed.bin'
KEY_FILE = 'gfiber_private.pem'


class Error(Exception):
  pass


def Sign(hostdir, key, ifname, ofname):
  """Sign the serial number with production key."""
  p = subprocess.Popen([os.path.join(hostdir, 'usr/bin', 'brcm_sign_enc')],
                       stdin=subprocess.PIPE, shell=False)
  for cmd in ['sign_kernel_file', ifname, ofname, 'l', key, SN_SIG_FILE]:
    p.stdin.write(cmd + '\n')
  p.stdin.close()
  retval = p.wait()
  if retval:
    raise Error('brcm_sign_enc returned exit code %d' % retval)


def GenSnSig(hostdir, bindir, sn, done_list):
  """Generate signature for a serial number."""
  fname = sn + '.sig'
  if done_list.__contains__(fname):
    sys.stderr.write('filename {0} is already used, check if there is any'
                     ' duplicate in serial number {1}.'.format(fname, sn))
    raise Error('Duplicate SN')
  cnt = BUFSIZE - len(sn)
  ifname = os.path.join(bindir, 'signing', SN_FILE)
  ofname = os.path.join(bindir, fname)
  with open(ifname, 'w+b') as sigfile:
    sigfile.write(sn)
    while cnt > 0:
      sigfile.write('G')
      cnt -= 1
  Sign(hostdir, KEY_FILE, ifname, ofname)
  done_list.append(fname)


def GenSnSigs(hostdir, bindir, sns):
  """Generate signature for a group of serial numbers."""
  done_list = []
  olddir = os.getcwd()
  os.chdir(os.path.join(bindir, 'signing'))
  for sn in (re.split('[\ \r\n]+', str(sns))):
    if sn:
      GenSnSig(hostdir, bindir, sn, done_list)
  os.chdir(olddir)


def main():
  o = options.Options(optspec)
  opt, _, _ = o.parse(sys.argv[1:])
  if opt.serialno:
    GenSnSigs(opt.hostdir, opt.bindir, opt.serialno)
  elif opt.filename:
    with open(opt.filename, 'r') as snfile:
      GenSnSigs(opt.hostdir, opt.bindir, snfile.read())


if __name__ == '__main__':
  try:
    sys.exit(main())
  except Error, e:
    sys.stderr.write('%s\n', e)
    sys.exit(1)
