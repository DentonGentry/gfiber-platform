#!/usr/bin/python
# Copyright 2012 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""i2c Module.

i2c module is based on i2c-tools smbus module. It provides simple I2C read and
write API for application developers, e.g. diagnostics and tr-069 HAL.
An emulator class is provided for application development and testing on host
machines.
"""

__author__ = 'kedong@google.com (Ke Dong)'

import time
import yaml


class Emulator(object):
  """Emulator is a Mock I2c Helper."""

  def __init__(self, name='testdata/default_i2cmap.yaml'):
    with open(name) as f:
      self.memory = yaml.load(f)

  def Read(self, addr, offset, length):
    """Read data from i2c address."""
    try:
      memory = self.memory[hex(addr)]
    except KeyError:
      raise IOError(19)

    ret = []
    for i in range(0, length):
      ret.append(memory[offset + i])
    return ret

  def Write(self, addr, offset, buf):
    """Write data to i2c address."""
    try:
      memory = self.memory[hex(addr)]
    except KeyError:
      raise IOError(19)

    for i in range(0, len(buf)):
      memory[offset + i] = buf[i]


class Util(object):
  """I2c Helper."""

  BLOCK_SIZE = 32

  # smbus is only imported on the platform that supports it. For host
  # machine environment, usually py-smbus is not installed and Util is not
  # used either. Therefore we hide importing of smbus in Util class.
  # pylint: disable-msg=C6204
  def __init__(self, bus=None, busno=0):
    if bus:
      self.bus = bus
    else:
      try:
        import smbus
      except ImportError, e:
        print 'py-smbus is not included in the site package.'
        raise e
      self.bus = smbus.SMBus(busno)
  # pylint: enable-msg=C6204

  def _GetReadMode(self, addr):
    if self.bus.read_i2c_block_data(addr, 0, 1) is not None:
      return 'block'
    elif self.bus.read_byte(addr) is not None:
      return 'byte'
    return None

  def Read(self, address, offset, length):
    """Read data from i2c address."""
    left = int(length)
    ret = []
    addr = int(address) >> 1
    mode = self._GetReadMode(addr)
    if mode == 'block':
      while left > 0:
        bytes_to_read = Util.BLOCK_SIZE
        if left < Util.BLOCK_SIZE:
          bytes_to_read = left
        time.sleep(0.001)
        buf = self.bus.read_i2c_block_data(addr, offset, bytes_to_read)
        offset += len(buf)
        left -= len(buf)
        ret.extend(buf)
    elif mode == 'byte':
      while left > 0:
        time.sleep(0.001)
        self.bus.write_byte(addr, offset)
        ret.append(self.bus.read_byte(addr))
        offset += 1
        left -= 1
    return ret

  def Write(self, address, offset, buf):
    """Write data to i2c address."""
    # The way smbus writes write_byte does not support batch write. Here we only
    # implement the buffer write in block write. Currently prism does support
    # block write, if we need to have batch byte write, we will need to patch
    # py-smbus implementation.
    addr = int(address) >> 1
    left = len(buf)
    buf_index = 0
    while left > 0:
      bytes_to_write = Util.BLOCK_SIZE
      if left < Util.BLOCK_SIZE:
        bytes_to_write = left
      write_buf = []
      write_buf.extend(buf[buf_index : (buf_index + bytes_to_write)])
      time.sleep(0.001)
      self.bus.write_i2c_block_data(addr, offset, write_buf)
      offset += bytes_to_write
      left -= bytes_to_write
      buf_index += bytes_to_write
