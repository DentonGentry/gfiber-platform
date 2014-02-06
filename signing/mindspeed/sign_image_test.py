#!/usr/bin/python
# Copyright 2012 Google Inc. All Rights Reserved.

"""Tests for sign_image."""

__author__ = 'smcgruer@google.com (Stephen McGruer)'

import math
import os
import subprocess
import tempfile
import unittest

import sign_image


class SignImageTests(unittest.TestCase):

  def setUp(self):
    self.original_raw_input = raw_input
    self.original_check_output = subprocess.check_output

  def tearDown(self):
    sign_image.raw_input = self.original_raw_input
    sign_image.subprocess.check_output = self.original_check_output

  def testGetArgsFromStdin(self):
    image_file = tempfile.NamedTemporaryFile()
    key_file = tempfile.NamedTemporaryFile()

    input_vals = ['mode', image_file.name, 'sig_file', 'l', key_file.name,
                  'image-signed.img']
    MockRawInput(input_vals)

    args = sign_image.GetArgsFromStdin()

    self.assertEqual(args['image'], image_file.name)
    self.assertEqual(args['key'], key_file.name)
    self.assertEqual(args['out_file'], 'image-signed.img')

  def testSignImageCorrectOutput(self):
    image_file = tempfile.NamedTemporaryFile()
    key_file = tempfile.NamedTemporaryFile()
    out_file = tempfile.NamedTemporaryFile()

    image_size = 41371

    # Create some random data to serve as our image.
    original_image_data = os.urandom(image_size)
    image_file.write(original_image_data)
    image_file.seek(0)

    # Mock out the call to Openssl to return a known signature.
    expected_sig = ''.join([chr(i) for i in range(255, -1, -1)])
    sign_image.subprocess.check_output = lambda _, stderr=None: expected_sig

    sign_image.SignImage(image_file.name, key_file.name, out_file.name)

    # The output image includes a 16-byte header, the original image data, some
    # padding, and the 256-byte signature.
    padded_image_size = int(math.ceil(image_size / 4096.0)) * 4096
    expected_output_size = padded_image_size + 272
    self.assertEqual(os.stat(out_file.name).st_size, expected_output_size)

    header_string = out_file.read(16)
    header_bytes = [ord(byte) for byte in header_string]
    expected_header_bytes = [
        0x9B, 0xA1, 0x00, 0x00,  # Image size = 41371 = 0xA19B
        0x00, 0xB0, 0x00, 0x00,  # Offset = 45056 = 0xB000
        0x00, 0x00, 0x00, 0x00,  # Padding
        0x00, 0x00, 0x00, 0x00   # Padding
    ]
    self.assertEqual(header_bytes, expected_header_bytes)

    image_data = out_file.read(image_size)
    self.assertEqual(image_data, original_image_data)

    padding_size = padded_image_size - image_size
    expected_padding = '\x00' * padding_size
    padding_bytes = out_file.read(padding_size)
    self.assertEqual(padding_bytes, expected_padding)

    sig = out_file.read(256)
    self.assertEqual(sig, expected_sig)

  def testSignImageCorrectCall(self):
    image_file = tempfile.NamedTemporaryFile()
    key_file = tempfile.NamedTemporaryFile()
    out_file = tempfile.NamedTemporaryFile()

    expected_cmd = ['openssl', 'dgst', '-sha1', '-sign', key_file.name,
                    image_file.name]
    openssl_called = [False]

    # Disable 'Unused argument stderr'.
    # pylint: disable=W0613
    def MyCheckOutput(cmd, stderr=None):
      self.assertEqual(cmd, expected_cmd)
      openssl_called[0] = True

      return '\xFF' * 256

    sign_image.subprocess.check_output = MyCheckOutput

    sign_image.SignImage(image_file.name, key_file.name, out_file.name)

    self.assertTrue(openssl_called[0])

  def testSignImageHandlesOpensslFailure(self):
    # Disable 'Unused argument stderr'.
    # pylint: disable=W0613
    def MyCheckOutput(cmd, stderr=None):
      raise subprocess.CalledProcessError(-1, cmd, None)

    sign_image.subprocess.check_output = MyCheckOutput

    try:
      sign_image.SignImage(None, None, None)
      self.fail()
    except IOError as e:
      # Expected - as long as it was caused by a subprocess error.
      self.assertEqual(len(e.args), 1)
      self.assertTrue(isinstance(e.args[0], subprocess.CalledProcessError))


def MockRawInput(return_vals):
  """Mocks out the raw_input function in sign_image.

  Args:
    return_vals: An array of values that raw_input will return in order
  """

  def YieldValuesGenerator(values):
    for val in values:
      yield val

  generator = YieldValuesGenerator(return_vals)

  def FakeRawInput(_):
    for val in generator:
      return val

  sign_image.raw_input = FakeRawInput


if __name__ == '__main__':
  unittest.main()

