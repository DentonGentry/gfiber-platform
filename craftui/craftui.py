#! /usr/bin/python
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

"""Chimera craft UI.  Code lifted from catawampus diag and tech UI."""

__author__ = 'edjames@google.com (Ed James)'

import getopt
import json
import os
import re
import subprocess
import sys
import urllib2
import tornado.ioloop
import tornado.web


class ConfigError(Exception):
  """Configuration errors to pass to browser."""

  def __init__(self, message):
    super(ConfigError, self).__init__(message)


class Validator(object):
  """Validate the user value and convert to safe config value."""
  pattern = r'^(.*)$'
  example = 'any string'

  def __init__(self):
    self.Reset()

  def Reset(self):
    self.fields = ()
    self.config = ''

  def Validate(self, value):
    self.Reset()
    self.value = value
    m = re.search(self.pattern, value)
    if not m:
      raise ConfigError('value "%s" does not match pattern "%s", eg: "%s"' %
                        (value, self.pattern, self.example))
    self.fields = m.groups()
    self.config = self.fields[0]


class VInt(Validator):
  """Validate as integer."""
  pattern = r'^(\d+)$'
  example = '123'


class VRange(VInt):
  """Validate as integer in a range."""

  def __init__(self, low, high):
    super(VRange, self).__init__()
    self.low = low
    self.high = high

  def Validate(self, value):
    super(VRange, self).Validate(value)
    self.CheckInRange(int(self.config), self.low, self.high)

  @staticmethod
  def CheckInRange(num, low, high):
    if num < low or num > high:
      raise ConfigError('number %d is out of range %d-%d' % (num, low, high))


class VSlash(Validator):
  """Validate as slash notation (eg 192.168.1.1/24)."""
  pattern = r'^((\d+).(\d+).(\d+).(\d+)/(\d+))$'
  example = '192.168.1.1/24'

  def __init__(self):
    super(VSlash, self).__init__()

  def Validate(self, value):
    super(VSlash, self).Validate(value)
    mask = int(self.fields[5])
    VRange.CheckInRange(mask, 0, 32)
    for dotted_quad_part in self.fields[1:4]:
      num = int(dotted_quad_part)
      VRange.CheckInRange(num, 0, 255)


class VVlan(VRange):
  """Validate as vlan."""

  def __init__(self):
    super(VVlan, self).__init__(0, 4095)


class VFreqHi(VRange):
  """Validate as Hi E-Band frequency."""

  def __init__(self):
    super(VFreqHi, self).__init__(82000000, 85000000)


class VFreqLo(VRange):
  """Validate as Low E-Band frequency."""

  def __init__(self):
    super(VFreqLo, self).__init__(72000000, 75000000)


class VPower(VRange):
  """Validate as PA power level."""

  def __init__(self):
    super(VPower, self).__init__(0, 2000000)       # TODO(edjames)


class VDict(Validator):
  """Validate as member of dict."""
  dict = {}

  def Validate(self, value):
    super(VDict, self).Validate(value)
    if value not in self.dict:
      keys = self.dict.keys()
      raise ConfigError('value "%s" must be one of "%s"' % (value, keys))
    self.config = self.dict[value]


class VTx(VDict):
  """Validate: tx/rx."""
  dict = {'tx': 'tx', 'rx': 'rx'}


class VTrueFalse(VDict):
  """Validate as true or false."""
  dict = {'true': 'true', 'false': 'false'}


class Config(object):
  """Configure the device after validation."""

  def __init__(self, validator):
    self.validator = validator()

  def Validate(self, value):
    self.validator.Validate(value)

  def Configure(self):
    raise Exception('override Config.Configure')

  @staticmethod
  def Run(command):
    """Run a command."""
    print 'running: %s' % command
    try:
      subprocess.check_output(command)
    except subprocess.CalledProcessError as e:
      print 'Run: ', str(e)
      raise ConfigError('command failed with %d' % e.returncode)


class PtpConfig(Config):
  """Configure using ptp-config."""

  def __init__(self, validator, key):
    super(PtpConfig, self).__init__(validator)
    self.key = key

  def Configure(self):
    Config.Run(['ptp-config', '-s', self.key, self.validator.config])


class PtpActivate(Config):
  """Configure using ptp-config."""

  def __init__(self, validator, key):
    super(PtpActivate, self).__init__(validator)
    self.key = key

  def Configure(self):
    Config.Run(['ptp-config', '-i', self.key])


class Glaukus(Config):
  """Configure using glaukus json api."""

  def __init__(self, validator, api, fmt):
    super(Glaukus, self).__init__(validator)
    self.api = api
    self.fmt = fmt

  def Configure(self):
    """Handle a JSON request to glaukusd."""
    url = 'http://localhost:8080' + self.api
    payload = self.fmt % self.validator.config
    # TODO(edjames)
    print 'Glaukus: ', url, payload
    try:
      fd = urllib2.urlopen(url, payload)
    except urllib2.URLError as ex:
      print 'Connection to %s failed: %s' % (url, ex.reason)
      raise ConfigError('failed to contact glaukus')
    response = fd.read()
    j = json.loads(response)
    print j
    if j['code'] != 'SUCCESS':
      if j['message']:
        raise ConfigError(j.message)
      raise ConfigError('failed to configure glaukus')


class Reboot(Config):
  """Reboot."""

  def Configure(self):
    if self.validator.config == 'true':
      Config.Run(['reboot'])


class CraftUI(object):
  """A web server that configures and displays Chimera data."""

  handlers = {
      'craft_ipaddr': PtpConfig(VSlash, 'craft_ipaddr'),
      'link_ipaddr': PtpConfig(VSlash, 'local_ipaddr'),
      'peer_ipaddr': PtpConfig(VSlash, 'peer_ipaddr'),

      'vlan_inband': PtpConfig(VVlan, 'vlan_inband'),
      'vlan_peer': PtpConfig(VVlan, 'vlan_peer'),

      'craft_ipaddr_activate': PtpActivate(VTrueFalse, 'craft_ipaddr'),
      'link_ipaddr_activate': PtpActivate(VTrueFalse, 'local_ipaddr'),
      'peer_ipaddr_activate': PtpActivate(VTrueFalse, 'peer_ipaddr'),
      'vlan_inband_activate': PtpActivate(VTrueFalse, 'vlan_inband'),
      'vlan_peer_activate': PtpActivate(VTrueFalse, 'vlan_peer'),

      'freq_hi': Glaukus(VFreqHi, '/api/radio/frequency', '{"hiFrequency":%s}'),
      'freq_lo': Glaukus(VFreqLo, '/api/radio/frequency', '{"loFrequency":%s}'),
      'mode_hi': Glaukus(VTx, '/api/radio/hiTransceiver/mode', '%s'),
      'tx_powerlevel': Glaukus(VPower, '/api/radio/tx/paPowerSet', '%s'),
      'tx_on': Glaukus(VTrueFalse, '/api/radio/paLnaPowerEnabled', '%s'),

      'reboot': Reboot(VTrueFalse)
  }
  ifmap = {
      'craft0': 'craft',
      'br0': 'bridge',
      'eth1.outofband': 'outofband',
      'eth1.inband': 'inband',
      'eth1.peer': 'link',
  }
  ifvlan = [
      'eth1.outofband',
      'eth1.inband',
      'eth1.peer'
  ]
  stats = [
      'multicast',
      'collisions',
      'rx_bytes',
      'rx_packets',
      'rx_errors',
      'rx_dropped',
      'tx_bytes',
      'tx_packets',
      'tx_errors',
      'tx_dropped'
  ]

  def __init__(self, wwwroot, port, sim):
    """initialize."""
    self.wwwroot = wwwroot
    self.port = port
    self.sim = sim
    self.data = {}
    self.data['refreshCount'] = 0

  def ApplyChanges(self, changes):
    """Apply changes to system."""
    if 'config' not in changes:
      raise ConfigError('missing required config array')
    conf = changes['config']
    try:
      # dry run to validate all
      for c in conf:
        for k, v in c.items():
          if k not in self.handlers:
            raise ConfigError('unknown key "%s"' % k)
          h = self.handlers[k]
          h.Validate(v)
      # do it again for real
      for c in conf:
        for k, v in c.items():
          h = self.handlers[k]
          h.Validate(v)
          h.Configure()
    except ConfigError as e:
      raise ConfigError('key "%s": %s' % (k, e))

  def ReadFile(self, filepath):
    """cat file."""
    text = ''
    try:
      with open(filepath) as fd:
        text = fd.read().rstrip()
    except IOError as e:
      text = 'ReadFile failed: %s: %s' % (filepath, e.strerror)

    return text

  def GetData(self):
    """Get system data, return a json string."""
    pj = self.GetPlatformData()
    mj = self.GetModemData()
    rj = self.GetRadioData()
    js = '{"platform":' + pj + ',"modem":' + mj + ',"radio":' + rj + '}'
    return js

  def AddIpAddr(self, data):
    """Run ip addr and parse results."""
    ipaddr = ''
    try:
      ipaddr = subprocess.check_output(['ip', '-o', 'addr'])
    except subprocess.CalledProcessError as e:
      print 'warning: "ip -o addr" failed: ', e
    v = {}
    for line in ipaddr.splitlines():
      f = line.split()
      ifname = re.sub(r'[@:].*', '', f[1])
      m = re.search(r'scope (global|link)', line)
      scope = m.group(1) if m else 'noscope'
      v[ifname + ':' + f[2] + ':' + scope] = f[3]
      m = re.search(r'link/ether (\S+)', line)
      if m:
        mac = m.group(1)
        v[ifname + ':' + 'mac'] = mac
    for ifname, uiname in self.ifmap.items():
      mac = v.get(ifname + ':mac')
      data[uiname + '_mac'] = mac if mac else 'unknown'
      for inet in ('inet', 'inet6'):
        kglobal = ifname + ':' + inet + ':' + 'global'
        vdata = v.get(kglobal, 'unknown')
        kdata = 'active_' + uiname + '_' + inet
        data[kdata] = vdata

  def AddInterfaceStats(self, data):
    """Get if stats."""
    for ifname, uiname in self.ifmap.items():
      d = self.sim + '/sys/class/net/' + ifname + '/statistics/'
      for stat in self.stats:
        k = uiname + '_' + stat
        data[k] = self.ReadFile(d + stat)

  def AddSwitchStats(self, data):
    """Run presterastats and send json."""
    stats = ''
    try:
      stats = subprocess.check_output(['presterastats'])
    except subprocess.CalledProcessError as e:
      print 'warning: "presterastats" failed: ', e
    data['switch'] = json.loads(stats)['port-interface-statistics']

  def AddVlans(self, data):
    """Run ip -d link and parse results for vlans."""
    iplink = ''
    try:
      iplink = subprocess.check_output(['ip', '-o', '-d', 'link'])
    except subprocess.CalledProcessError as e:
      print 'warning: "ip -o -d link" failed: ', e
    v = {}
    for line in iplink.splitlines():
      m = re.search(r'^\d+: ([\w\.]+)@\w+: .* vlan id (\w+)', line)
      if m:
        v[m.group(1)] = m.group(2)
    for ifname in self.ifvlan:
      uiname = self.ifmap[ifname]
      vdata = v.get(ifname, 'unknown')
      kdata = 'active_' + uiname + '_vlan'
      data[kdata] = vdata

  def GetPlatformData(self):
    """Get platform data, return a json string."""
    data = self.data
    sim = self.sim

    if data['refreshCount'] == 0:
      data['serialno'] = self.ReadFile(sim + '/etc/serial')
      data['version'] = self.ReadFile(sim + '/etc/version')
      data['platform'] = self.ReadFile(sim + '/etc/platform')
      data['softwaredate'] = self.ReadFile(sim + '/etc/softwaredate')
    data['refreshCount'] += 1
    data['uptime'] = self.ReadFile(sim + '/proc/uptime')
    data['ledstate'] = self.ReadFile(sim + '/tmp/gpio/ledstate')
    cs = '/config/settings/'
    data['craft_ipaddr'] = self.ReadFile(sim + cs + 'craft_ipaddr')
    data['link_ipaddr'] = self.ReadFile(sim + cs + 'local_ipaddr')
    data['peer_ipaddr'] = self.ReadFile(sim + cs + 'peer_ipaddr')
    data['vlan_inband'] = self.ReadFile(sim + cs + 'vlan_inband')
    data['vlan_outofband'] = self.ReadFile(sim + cs + 'vlan_outofband')
    data['vlan_link'] = self.ReadFile(sim + cs + 'vlan_peer')
    self.AddIpAddr(data)
    self.AddInterfaceStats(data)
    self.AddSwitchStats(data)
    self.AddVlans(data)
    return json.dumps(data)

  def GetModemData(self):
    """Get modem data, return a json string."""
    response = '{}'
    if self.sim:
      response = self.ReadFile(self.sim + '/tmp/glaukus/modem.json')
    else:
      try:
        url = 'http://localhost:8080/api/modem'
        handle = urllib2.urlopen(url, timeout=2)
        response = handle.read()
      except urllib2.URLError as ex:
        print 'Connection to %s failed: %s' % (url, ex.reason)
    return response

  def GetRadioData(self):
    """Get radio data, return a json string."""
    response = '{}'
    if self.sim:
      response = self.ReadFile(self.sim + '/tmp/glaukus/radio.json')
    else:
      try:
        url = 'http://localhost:8080/api/radio'
        handle = urllib2.urlopen(url, timeout=2)
        response = handle.read()
      except urllib2.URLError as ex:
        print 'Connection to %s failed: %s' % (url, ex.reason)
    return response

  class MainHandler(tornado.web.RequestHandler):
    """Displays the Craft UI."""

    def get(self):
      ui = self.settings['ui']
      print 'GET craft HTML page'
      self.render(ui.wwwroot + '/index.thtml', peerurl='/?peer=1')

  class ConfigHandler(tornado.web.RequestHandler):
    """Displays the Config page."""

    def get(self):
      ui = self.settings['ui']
      print 'GET config HTML page'
      self.render(ui.wwwroot + '/config.thtml', peerurl='/config/?peer=1')

  class RestartHandler(tornado.web.RequestHandler):
    """Restart the box."""

    def get(self):
      print 'displaying restart interstitial screen'
      self.render('restarting.html')

    def post(self):
      print 'user requested restart'
      self.redirect('/restart')
      os.system('(sleep 5; reboot) &')

  class JsonHandler(tornado.web.RequestHandler):
    """Provides JSON-formatted content to be displayed in the UI."""

    @tornado.web.asynchronous
    def get(self):
      ui = self.settings['ui']
      print 'GET JSON data for craft page'
      jsonstring = ui.GetData()
      self.set_header('Content-Type', 'application/json')
      self.write(jsonstring)
      self.finish()

    def post(self):
      print 'POST JSON data for craft page'
      request = self.request.body
      result = {}
      result['error'] = 0
      result['errorstring'] = ''
      try:
        try:
          json_args = json.loads(request)
          request = json.dumps(json_args)
        except ValueError as e:
          print e
          raise ConfigError('json format error')
        ui = self.settings['ui']
        ui.ApplyChanges(json_args)
      except ConfigError as e:
        print e
        result['error'] += 1
        result['errorstring'] += str(e)

      response = json.dumps(result)
      print 'request: ', request
      print 'response: ', response
      self.set_header('Content-Type', 'application/json')
      self.write(response)
      self.finish()

  def RunUI(self):
    """Create the web server and run forever."""
    handlers = [
        (r'/', self.MainHandler),
        (r'/config', self.ConfigHandler),
        (r'/content.json', self.JsonHandler),
        (r'/restart', self.RestartHandler),
        (r'/static/([^/]*)$', tornado.web.StaticFileHandler,
         {'path': self.wwwroot + '/static'}),
    ]
    app = tornado.web.Application(handlers)
    app.settings['ui'] = self
    app.listen(self.port)
    ioloop = tornado.ioloop.IOLoop.instance()
    ioloop.start()


def Usage():
  """Show usage."""
  print 'Usage: % [-p)ort 80] [-d)ir web] [-s)im top]'
  print '\tUse -s to provide an alternate rootfs'


def main():
  www = '/usr/craftui/www'
  port = 80
  sim = ''
  try:
    opts, args = getopt.getopt(sys.argv[1:], 's:p:w:',
                               ['sim=', 'port=', 'www='])
  except getopt.GetoptError as err:
    # print help information and exit:
    print str(err)
    Usage()
    sys.exit(1)
  for o, a in opts:
    if o in ('-s', '--sim'):
      sim = a
    elif o in ('-p', '--port'):
      port = int(a)
    elif o in ('-w', '--www'):
      www = a
    else:
      assert False, 'unhandled option'
      Usage()
      sys.exit(1)
  if args:
    assert False, 'extra args'
    Usage()
    sys.exit(1)
  craftui = CraftUI(www, port, sim)
  craftui.RunUI()


if __name__ == '__main__':
  main()
