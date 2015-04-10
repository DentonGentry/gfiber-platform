#!/usr/bin/python
# Copyright 2015 Google Inc. All Rights Reserved.
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

"""Helper function to send a UDP packet to a specified IP address/port."""

__author__ = 'hunguyen@google.com (Huy Nguyen)'

import argparse
import socket


def GetArgs():
  """Parses and returns arguments passed in."""
  parser = argparse.ArgumentParser(prog='udp_send')
  parser.add_argument('--hostname', help='IP address to send.', required=True)
  parser.add_argument('--port', help='Port number to send.', required=True)
  parser.add_argument('--data', help='Data to send.', required=True)
  args = parser.parse_args()
  return str(args.hostname), int(args.port), str(args.data)


def main():
  hostname, port, data = GetArgs()
  sock = socket.socket(socket.AF_INET,  # Internet
                       socket.SOCK_DGRAM)  # UDP
  sock.sendto(data, (hostname, port))


if __name__ == '__main__':
  main()
