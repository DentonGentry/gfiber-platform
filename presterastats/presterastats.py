#!/usr/bin/python
# Copyright 2016 Google Inc. All Rights Reserved.
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

"""Retrieve packet statistics from cpss, emit in JSON format."""

__author__ = 'poist@google.com (Gregory Poist)'

import itertools
import json
import os
import signal
import subprocess
import sys
import textwrap
import threading

import options


optspec = """
presterastats [options]
--
ports=           prestera ports to collect [0/0,0/4,0/24,0/25]
timeout=         seconds to wait for cpss response [5]
"""


class PresteraStats(object):
  """Class wrapping a cpss command to request stats."""

  def __init__(self, ports, timeout):
    self.ports = ports
    self.timeout = timeout

  def WriteToStderr(self, msg, is_json=False):
    """Write a message to stderr."""
    if is_json:
      # Make the json easier to parse from the logs.
      json_data = json.loads(msg)
      json_str = json.dumps(json_data, sort_keys=True, indent=2,
                            separators=(',', ': '))
      # Logging pretty-printed json is like logging one huge line. Logos is
      # configured to limit lines to 768 characters. Split the logged output at
      # half of that to make sure logos doesn't clip our output.
      sys.stderr.write('\n'.join(textwrap.wrap(json_str, width=384)))
      sys.stderr.flush()
    else:
      sys.stderr.write(msg)
      sys.stderr.flush()

  def StartCpssSubprocess(self):
    """Start execution of the cpss_cmd sub-process."""
    return subprocess.Popen(['cpss_cmd'],
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            preexec_fn=os.setsid)

  def GetMibStats(self):
    """Extract statistics from cpss_cmd output."""
    result = None
    proc = self.StartCpssSubprocess()
    if not proc:
      self.WriteToStderr('Failed to start subprocess.')
      return
    kill_proc = lambda p: os.killpg(os.getpgid(p.pid), signal.SIGTERM)
    timer = threading.Timer(self.timeout, kill_proc, [proc])
    cpss_cmd_prefix = 'do show interfaces mac json-counters ethernet '
    try:
      timer.start()
      cpssout, _ = proc.communicate(input=cpss_cmd_prefix + self.ports + '\n')

      # itertools magic to take only the lines between JSONSTART and JSONEND.
      it = itertools.dropwhile(lambda line: line.strip() != 'JSONSTART',
                               cpssout.splitlines())
      it = itertools.islice(it, 1, None)
      it = itertools.takewhile(lambda line: line.strip() != 'JSONEND', it)

      # smack itertools iterable down to a string
      result = ''.join(it)
    finally:
      timer.cancel()

    if result:
      return json.loads(result)


def main():
  o = options.Options(optspec)
  (opt, unused_flags, unused_extra) = o.parse(sys.argv[1:])
  prestera = PresteraStats(opt.ports, opt.timeout)
  results = prestera.GetMibStats()

  if results:
    print json.dumps(results, sort_keys=True,
                     indent=2, separators=(',', ': '))
    sys.exit(0)

  sys.exit(1)


if __name__ == '__main__':
  main()
