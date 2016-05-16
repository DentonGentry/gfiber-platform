#!/usr/bin/python

"""Tests for the syslogd program."""

import errno
import os
import re
import select
import socket
import subprocess
import tempfile
import time

from wvtest.wvtest import WVFAIL
from wvtest.wvtest import WVPASS
from wvtest.wvtest import wvtest
from wvtest.wvtest import wvtest_main


def ChompLeadingIP(line):
  _, message = re.split(r'^\[.*\]: ', line)
  return message


@wvtest
def TestSyslogd():
  """spin up and test a syslogd server."""
  subprocess.call(['pkill', '-f', 'python syslogd.py'])
  try:
    os.remove('/tmp/syslogd/ready')
  except OSError as e:
    if e.errno != errno.ENOENT: raise

  filters = tempfile.NamedTemporaryFile(bufsize=0, suffix='.conf', delete=False)
  print >>filters, 'PASS'
  filters.close()

  out_r, out_w = os.pipe()
  err_r, err_w = os.pipe()
  subprocess.Popen(['python', 'syslogd.py', '-f', filters.name, '-v'],
                   stdout=out_w, stderr=err_w)

  while True:
    try:
      if 'ready' in os.listdir('/tmp/syslogd'): break
      time.sleep(0.1)
    except OSError as e:
      if e.errno != errno.ENOENT: raise

  def _Read():
    r, unused_w, unused_x = select.select([out_r, err_r], [], [], 30)
    out = ''
    err = ''

    if out_r in r: out = os.read(out_r, 4096)
    if err_r in r: err = os.read(err_r, 4096)

    if out or err:
      return out, err
    else:
      raise Exception('read timed out')

  _Read()  # discard syslogd startup messages

  addr = ('::', 5514)
  s = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)

  s.sendto('a\nErROR: b\nw: c', addr)
  out, err = _Read()
  WVFAIL(out)
  WVPASS(ChompLeadingIP(err).startswith('discarded'))

  s.sendto('a\tb\r\nabba\tbbb\naa\t\tb\tc\n', addr)
  out, err = _Read()
  WVFAIL(out)
  WVPASS(ChompLeadingIP(err).startswith('discarded'))

  s.sendto(''.join(chr(i) for i in range(33)) + '\n', addr)
  out, err = _Read()
  WVFAIL(out)
  WVPASS(ChompLeadingIP(err).startswith('discarded'))

  s.sendto('Test PASSes', addr)
  time.sleep(1)  # make sure both streams update at once
  out, err = _Read()
  WVPASS(ChompLeadingIP(out).startswith('Test PASSes'))

  s.sendto('TooLongToPASS' * 100, addr)
  out, err = _Read()
  WVFAIL(out)
  WVPASS(ChompLeadingIP(err).startswith('discarded'))

  s.sendto('NoMatchFAILS', addr)
  out, err = _Read()
  WVFAIL(out)
  WVPASS(ChompLeadingIP(err).startswith('discarded'))


if __name__ == '__main__':
  wvtest_main()
