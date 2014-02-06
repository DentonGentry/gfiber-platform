#!/usr/bin/python
# Copyright 2012 Google Inc. All Rights Reserved.

"""Sign bootloader/kernel image with RSA signature for secure boot.

The signed image conforms to the header defined in repack.py.
"""

__author__ = 'smcgruer@google.com (Stephen McGruer)'

import math
from optparse import OptionParser
import os
import struct
import subprocess
import sys


def GetArgsFromStdin():
  """Gets the necessary arguments from stdin.

  For compatibility with brcm_sign_enc, more arguments are required
  than are actually used.

  Returns:
    A dictionary containing the necessary arguments to sign the image.
  Raises:
    IOError:  If unable to get an argument, or if it is invalid.
  """

  args = {}

  raw_input('Mode (ignored): ')
  print

  args['image'] = raw_input('Input image file: ')
  if not os.path.isfile(args['image']):
    raise IOError('No such image file "%s"' % args['image'])
  print

  raw_input('Output signature file (ignored): ')
  print
  raw_input('Endianness (ignored): ')
  print

  args['key'] = raw_input('Key file: ')
  if not os.path.isfile(args['key']):
    raise IOError('No such key file "%s"' % args['key'])
  print

  args['out_file'] = raw_input('Output image name: ')
  print

  return args


def SignImage(image_path, key_path, out_path):
  """Signs the image file.

  The generated signature confirms to the specification in repack.py.
  A 16-byte header is prepended onto the image, and the signature is
  appended to the end.

  The signature is generated via a system call to openssl, and is a
  signed SHA1 hash of the image.

  Args:
    image_path: Path to the image that is to be signed.
    key_path: Path to the RSA private key for signing.
    out_path: Path to write the signed image to.
  Raises:
    IOError: If anything goes wrong with the signing.
  """

  cmd = ['openssl', 'dgst', '-sha1', '-sign', key_path, image_path]
  try:
    signature = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    raise IOError(e)

  stat = os.stat(image_path)
  image_len = stat.st_size

  with open(image_path, 'rb') as f:
    image = f.read()

  if len(image) != image_len:
    raise IOError('Unexpected number of bytes read (expected %s, got %s)' %
                  (image_len, len(image)))

  # Based on brcm_sign_enc, the image should be padding to a multiple
  # of 4096.
  padded_len = int(4096 * math.ceil(image_len / 4096.0))
  padding_len = padded_len - image_len
  padding = padding_len * '\x00'

  with open(out_path, 'wb') as f:
    f.write(struct.pack('<IIII', image_len, padded_len, 0, 0))
    f.write(image)
    f.write(padding)
    f.write(signature)


def main():
  parser = OptionParser(description='todo')
  parser.add_option('-i', '--image', dest='image',
                    help='Image to be signed')
  parser.add_option('-k', '--key', dest='key',
                    help='RSA private key')
  parser.add_option('-o', '--out_image', dest='out_file',
                    help='Output filename')

  (opts, args) = parser.parse_args()

  # For compatibility with brcm_sign_enc, we allow all arguments to
  # be passed in via stdin. For sanity, we also allow proper command
  # line flag usage.

  opts_list = [opts.image, opts.key, opts.out_file]
  if any(opts_list) and not all(opts_list):
    print 'Error: If any options are specified, all options must be.'
    return 1

  try:
    if any(opts_list):
      SignImage(opts.image, opts.key, opts.out_file)
    else:
      args = GetArgsFromStdin()
      SignImage(args['image'], args['key'], args['out_file'])
  except IOError as e:
    print 'Error: %s' % e
    return 1

  return 0


if __name__ == '__main__':
  sys.exit(main())
