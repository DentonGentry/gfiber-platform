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
"""Tests for i2c."""

__author__ = 'kedong@google.com (Ke Dong)'

import unittest
import mox
import i2c


# The smbus is not installed on the host machine.
# pylint: disable-msg=C6409
class SMBus(object):

  def read_i2c_block_data(self, addr, offset, length):
    pass

  def write_i2c_block_data(self, addr, offset, length):
    pass

  def read_byte(self, addr):
    pass

  def write_byte(self, addr, cmd):
    pass
# pylint: enable-msg=C6409


class I2cUtilUnitTest(unittest.TestCase):

  def setUp(self):
    self.smbus_mocker = mox.Mox()
    self.address = 0xA0
    self.i2c_addr = self.address >> 1

  def testReadOneBlockShort(self):
    buf = [1, 2, 3]
    bus = self.smbus_mocker.CreateMock(SMBus)
    bus.read_i2c_block_data(self.i2c_addr, 0, 1).AndReturn([0])
    bus.read_i2c_block_data(self.i2c_addr, 40, 3).AndReturn(buf)
    self.smbus_mocker.ReplayAll()
    helper = i2c.Util(bus)
    data = helper.Read(self.address, 40, 3)
    self.smbus_mocker.VerifyAll()
    self.assertEquals(buf, data)

  def testReadOneBlock(self):
    buf = [3]*32
    bus = self.smbus_mocker.CreateMock(SMBus)
    bus.read_i2c_block_data(self.i2c_addr, 0, 1).AndReturn([0])
    bus.read_i2c_block_data(self.i2c_addr, 20, 32).AndReturn(buf)
    self.smbus_mocker.ReplayAll()
    helper = i2c.Util(bus)
    data = helper.Read(self.address, 20, 32)
    self.smbus_mocker.VerifyAll()
    self.assertEquals(buf, data)

  def testReadThreeBlocks(self):
    buf = [3]*96
    result = [3]*32
    bus = self.smbus_mocker.CreateMock(SMBus)
    bus.read_i2c_block_data(self.i2c_addr, 0, 1).AndReturn([0])
    bus.read_i2c_block_data(self.i2c_addr, 10, 32).AndReturn(result)
    bus.read_i2c_block_data(self.i2c_addr, 42, 32).AndReturn(result)
    bus.read_i2c_block_data(self.i2c_addr, 74, 32).AndReturn(result)
    self.smbus_mocker.ReplayAll()
    helper = i2c.Util(bus)
    data = helper.Read(self.address, 10, 96)
    self.smbus_mocker.VerifyAll()
    self.assertEquals(buf, data)

  def testReadThreeBlocksMore(self):
    buf = [3]*100
    result = [3]*32
    bus = self.smbus_mocker.CreateMock(SMBus)
    bus.read_i2c_block_data(self.i2c_addr, 0, 1).AndReturn([0])
    bus.read_i2c_block_data(self.i2c_addr, 10, 32).AndReturn(result)
    bus.read_i2c_block_data(self.i2c_addr, 42, 32).AndReturn(result)
    bus.read_i2c_block_data(self.i2c_addr, 74, 32).AndReturn(result)
    result = [3]*4
    bus.read_i2c_block_data(self.i2c_addr, 106, 4).AndReturn(result)
    self.smbus_mocker.ReplayAll()
    helper = i2c.Util(bus)
    data = helper.Read(self.address, 10, 100)
    self.smbus_mocker.VerifyAll()
    self.assertEquals(buf, data)

  def testReadByte(self):
    bus = self.smbus_mocker.CreateMock(SMBus)
    bus.read_i2c_block_data(self.i2c_addr, 0, 1).AndReturn(None)
    bus.read_byte(self.i2c_addr).AndReturn(0)
    bus.write_byte(self.i2c_addr, 10).AndReturn(None)
    bus.read_byte(self.i2c_addr).AndReturn(1)
    bus.write_byte(self.i2c_addr, 11).AndReturn(None)
    bus.read_byte(self.i2c_addr).AndReturn(2)
    bus.write_byte(self.i2c_addr, 12).AndReturn(None)
    bus.read_byte(self.i2c_addr).AndReturn(3)
    self.smbus_mocker.ReplayAll()
    helper = i2c.Util(bus)
    data = helper.Read(self.address, 10, 3)
    self.smbus_mocker.VerifyAll()
    self.assertEquals([1, 2, 3], data)

  def testWriteOneBlockShort(self):
    buf = [1, 2, 3]
    i2c_buf = []
    i2c_buf.extend(buf)
    bus = self.smbus_mocker.CreateMock(SMBus)
    bus.write_i2c_block_data(self.i2c_addr, 40, i2c_buf).AndReturn(None)
    self.smbus_mocker.ReplayAll()
    helper = i2c.Util(bus)
    helper.Write(self.address, 40, buf)
    self.smbus_mocker.VerifyAll()

  def testWriteOneBlock(self):
    buf = [3]*32
    i2c_buf = []
    i2c_buf.extend(buf)
    bus = self.smbus_mocker.CreateMock(SMBus)
    bus.write_i2c_block_data(self.i2c_addr, 20, i2c_buf).AndReturn(None)
    self.smbus_mocker.ReplayAll()
    helper = i2c.Util(bus)
    helper.Write(self.address, 20, buf)
    self.smbus_mocker.VerifyAll()

  def testWriteThreeBlocks(self):
    buf = [3]*96
    i2c_buf = []
    i2c_buf.extend([3]*32)
    bus = self.smbus_mocker.CreateMock(SMBus)
    bus.write_i2c_block_data(self.i2c_addr, 10, i2c_buf).AndReturn(None)
    bus.write_i2c_block_data(self.i2c_addr, 42, i2c_buf).AndReturn(None)
    bus.write_i2c_block_data(self.i2c_addr, 74, i2c_buf).AndReturn(None)
    self.smbus_mocker.ReplayAll()
    helper = i2c.Util(bus)
    helper.Write(self.address, 10, buf)
    self.smbus_mocker.VerifyAll()

  def testWriteThreeBlocksMore(self):
    buf = [3]*100
    i2c_buf = []
    i2c_buf.extend([3]*32)
    bus = self.smbus_mocker.CreateMock(SMBus)
    bus.write_i2c_block_data(self.i2c_addr, 10, i2c_buf).AndReturn(None)
    bus.write_i2c_block_data(self.i2c_addr, 42, i2c_buf).AndReturn(None)
    bus.write_i2c_block_data(self.i2c_addr, 74, i2c_buf).AndReturn(None)
    i2c_buf = []
    i2c_buf.extend([3]*4)
    bus.write_i2c_block_data(self.i2c_addr, 106, i2c_buf).AndReturn(None)
    self.smbus_mocker.ReplayAll()
    helper = i2c.Util(bus)
    helper.Write(self.address, 10, buf)
    self.smbus_mocker.VerifyAll()


if __name__ == '__main__':
  unittest.main()
