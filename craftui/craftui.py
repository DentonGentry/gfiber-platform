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

import base64
import getopt
import json
import os
import re
import StringIO
import subprocess
import sys
import urllib2
import digest
import png
import tornado.httpserver
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
    super(VPower, self).__init__(0, 2000)


class VGain(VRange):
  """Validate as gain level."""

  def __init__(self):
    super(VGain, self).__init__(0, 63)


class VGainIndex(VRange):
  """Validate as gain index."""

  def __init__(self):
    super(VGainIndex, self).__init__(1, 5)


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


class VPassword(Validator):
  """Validate as base64 encoded and reasonable length."""
  example = '******'

  def Validate(self, value):
    # value is { admin: admin_pw, new: new_pw, confirm: confirm_pw }
    # passwords are in base64
    super(VPassword, self).Validate(value['new'])

    # TODO(edjames) ascii decodes legally; how to check it's really base64?
    try:
      current = base64.b64decode(self.admin)
      admin_pw = base64.b64decode(value['admin'])
      new_pw = base64.b64decode(value['new'])
    except TypeError:
      raise ConfigError('passwords must be base64 encoded')

    # verify correct admin pw is passed, confirm matches
    if current != admin_pw:
      raise ConfigError('admin password is incorrect')
    if value['new'] != value['confirm']:
      raise ConfigError('new password does not match confirm password')
    if len(new_pw) < 5 or len(new_pw) > 16:
      raise ConfigError('passwords should be 5-16 characters')


class Config(object):
  """Configure the device after validation."""

  def __init__(self, validator):
    self.validator = validator()

  def Validate(self, value):
    self.validator.Validate(value)

  def Configure(self):
    raise Exception('override Config.Configure')

  def SetUI(self, ui):
    self.ui = ui

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


class PtpPassword(PtpConfig):
  """Configure a password (need password_admin setting)."""

  def Validate(self, value):
    admin = self.ui.ReadFile('%s/config/settings/password_admin' % self.ui.sim)
    self.validator.admin = admin
    super(PtpPassword, self).Validate(value)


class PtpActivate(Config):
  """Configure using ptp-config."""

  def __init__(self, validator, key):
    super(PtpActivate, self).__init__(validator)
    self.key = key

  def Configure(self):
    Config.Run(['ptp-config', '-i', self.key])


class Glaukus(Config):
  """Configure using glaukus json api."""
  baseurl = 'http://localhost:8080'

  def __init__(self, validator, api, fmt):
    super(Glaukus, self).__init__(validator)
    self.api = api
    self.fmt = fmt

  def CallJson(self, url, payload):
    """Handle a JSON request to glaukusd."""
    print 'Glaukus: ', url, payload

    try:
      fd = urllib2.urlopen(url, payload, timeout=2)
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

  def Configure(self):
    url = self.baseurl + self.api
    payload = self.fmt % self.validator.config
    self.CallJson(url, payload)


class GlaukusACM(Glaukus):
  """Configure glaukus ACM."""

  def __init__(self, validator):
    super(GlaukusACM, self).__init__(validator, '/unused', 'unused')

  def Configure(self):
    enable = self.validator.config
    if enable is 'true':
      url = self.baseurl + '/api/modem/acm'
      payload = '{"rxSensorsEnabled":true,"txSwitchEnabled":true}'
      self.CallJson(url, payload)
    else:
      url = self.baseurl + '/api/modem/acm'
      payload = '{"rxSensorsEnabled":false,"txSwitchEnabled":false}'
      self.CallJson(url, payload)

      url = self.baseurl + '/api/modem/acm/profile'
      payload = '{"profileIndex":0,"isLocal":true}'
      self.CallJson(url, payload)


class Reboot(Config):
  """Reboot."""

  def Configure(self):
    if self.validator.config == 'true':
      cmd = '(sleep 5; reboot)&'
      os.system(cmd)


class FactoryReset(Config):
  """Factory Reset."""

  def Configure(self):
    if self.validator.config == 'true':
      cmd = 'zap --i-really-mean-it --erase-backups && ((sleep 5; reboot) &)'
      os.system(cmd)


class CraftUI(object):
  """A web server that configures and displays Chimera data."""

  handlers = {
      'password_admin': PtpPassword(VPassword, 'password_admin'),
      'password_guest': PtpPassword(VPassword, 'password_guest'),

      'craft_ipaddr': PtpConfig(VSlash, 'craft_ipaddr'),
      'link_ipaddr': PtpConfig(VSlash, 'local_ipaddr'),
      'peer_ipaddr': PtpConfig(VSlash, 'peer_ipaddr'),

      'vlan_inband': PtpConfig(VVlan, 'vlan_inband'),
      'vlan_ooband': PtpConfig(VVlan, 'vlan_ooband'),
      'vlan_peer': PtpConfig(VVlan, 'vlan_peer'),

      'craft_ipaddr_activate': PtpActivate(VTrueFalse, 'craft_ipaddr'),
      'link_ipaddr_activate': PtpActivate(VTrueFalse, 'local_ipaddr'),
      'peer_ipaddr_activate': PtpActivate(VTrueFalse, 'peer_ipaddr'),

      'vlan_inband_activate': PtpActivate(VTrueFalse, 'vlan_inband'),
      'vlan_ooband_activate': PtpActivate(VTrueFalse, 'vlan_ooband'),
      'vlan_peer_activate': PtpActivate(VTrueFalse, 'vlan_peer'),

      'freq_hi': Glaukus(VFreqHi, '/api/radio/frequency', '{"hiFrequency":%s}'),
      'freq_lo': Glaukus(VFreqLo, '/api/radio/frequency', '{"loFrequency":%s}'),
      'mode_hi': Glaukus(VTx, '/api/radio/hiTransceiver/mode', '%s'),
      'tx_powerlevel': Glaukus(VPower, '/api/radio/tx/paPowerSet', '%s'),
      'tx_gain': Glaukus(VGain, '/api/radio/tx/vgaGain', '%s'),
      'rx_gainindex': Glaukus(VGainIndex, '/api/radio/rx/agcDigitalGainIndex',
                              '%s'),
      'palna_on': Glaukus(VTrueFalse, '/api/radio/paLnaPowerEnabled', '%s'),

      'acm_on': GlaukusACM(VTrueFalse),

      'reboot': Reboot(VTrueFalse),
      'factory_reset': FactoryReset(VTrueFalse)
  }
  ifmap = {
      'craft0': 'craft',
      'br0': 'bridge',
      'sw0.ooband': 'ooband',
      'sw0.inband': 'inband',
      'sw0.peer': 'link',
  }
  ifvlan = [
      'sw0.ooband',
      'sw0.inband',
      'sw0.peer'
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

  def __init__(self, wwwroot, http_port, https_port, sim):
    """Initialize."""
    self.wwwroot = wwwroot
    self.http_port = http_port
    self.https_port = https_port
    self.sim = sim
    self.data = {}
    self.data['refreshCount'] = 0
    platform = self.ReadFile(sim + '/etc/platform')
    serial = self.ReadFile(sim + '/etc/serial')
    self.realm = '%s-%s' % (platform, serial)

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
          h.SetUI(self)
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

  def GetValue(self, data, path):
    """Walk down the dicts to get a value."""
    keys = path.split('/')
    v = data
    for key in keys:
      if v:
        v = v.get(key, None)
    return v

  def AddLeds(self, data):
    """Add status leds to data."""
    red = 'red.gif'
    green = 'green.gif'
    leds = {
        'ACS': red,
        'Switch': red,
        'Modem': red,
        'Radio': red,
        'RSSI': red,
        'MSE': red,
        'Peer': red
    }
    if self.GetValue(data, 'platform/ledstate') is 'ACSCONTACT':
      leds['ACS'] = green
    if self.GetValue(data, 'platform/cpss_ready'):
      leds['Switch'] = green
    if self.GetValue(data, 'modem/status/acquireStatus') == 1:
      leds['Modem'] = green
    if self.GetValue(data, 'radio/paLnaPowerEnabled'):
      leds['Radio'] = green
    rssi = self.GetValue(data, 'radio/rx/rssi')
    if rssi >= 1500 and rssi <= 2000:
      leds['RSSI'] = green
    mse = self.GetValue(data, 'modem/status/normalizedMse')
    if mse is not None and mse <= -180:
      leds['MSE'] = green
    if self.GetValue(data, 'platform/peer_up'):
      leds['Peer'] = green
    data['leds'] = leds

  def GetData(self):
    """Get system data, return a json string."""
    data = {}
    data['platform'] = self.GetPlatformData()
    data['modem'] = self.GetModemData()
    data['radio'] = self.GetRadioData()
    self.AddLeds(data)
    js = json.dumps(data)
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
    try:
      data['switch'] = json.loads(stats)['port-interface-statistics']
    except ValueError as e:
      print 'warning: "presterastats" json parse failed: ', e

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
    """Get platform data."""
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
    data['cpu_temperature'] = self.ReadFile(sim + '/tmp/gpio/cpu_temperature')
    data['peer_up'] = os.path.exists(sim + '/tmp/peer-up')
    data['cpss_ready'] = os.path.exists(sim + '/tmp/cpss_ready')
    cs = '/fiber/config/settings/'
    data['craft_ipaddr'] = self.ReadFile(sim + cs + 'craft_ipaddr')
    data['link_ipaddr'] = self.ReadFile(sim + cs + 'local_ipaddr')
    data['peer_ipaddr'] = self.ReadFile(sim + cs + 'peer_ipaddr')
    data['vlan_inband'] = self.ReadFile(sim + cs + 'vlan_inband')
    data['vlan_ooband'] = self.ReadFile(sim + cs + 'vlan_ooband')
    data['vlan_link'] = self.ReadFile(sim + cs + 'vlan_peer')
    self.AddIpAddr(data)
    self.AddInterfaceStats(data)
    self.AddSwitchStats(data)
    self.AddVlans(data)
    return data

  def GetModemData(self):
    """Get modem data."""
    data = {}
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
    try:
      data = json.loads(response)
    except ValueError as e:
      print 'json format error: %s' % e
    return data

  def GetRadioData(self):
    """Get radio data, return a json string."""
    data = {}
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
    try:
      data = json.loads(response)
    except ValueError as e:
      print 'json format error: %s' % e
    return data

  def GetIQPNG(self, path):
    """Get IQ points and render as PNG."""
    response = '[0,0]'
    if self.sim:
      response = self.ReadFile(self.sim + '/tmp/glaukus/' + path + '.json')
    else:
      try:
        url = 'http://localhost:8080/api/modem/iq/' + path
        handle = urllib2.urlopen(url, timeout=2)
        response = handle.read()
      except urllib2.URLError as ex:
        print 'Connection to %s failed: %s' % (url, ex.reason)

    coords = [0, 0]
    try:
      coords = json.loads(response)
    except ValueError as e:
      print 'json format error: %s' % e

    # owh is original width/height of data (-1200 to 1200)
    owh = (2400, 2400)
    # wh is display size (400x400)
    wh = (400, 400)

    w = png.Writer(size=wh, greyscale=True, bitdepth=1)
    scanline = int((wh[0] + 7) / 8)
    rows = [scanline*[0] for i in xrange(0, wh[1])]
    for i in xrange(0, len(coords) / 2):
      # data is a series of x,y,x,y,x,y...
      xy = (coords[i*2], coords[i*2+1])
      # transform and scale data to display
      sxy = (int((xy[0] + owh[0]/2 + .5) * wh[0] / owh[0]),
             int((xy[1] + owh[1]/2 + .5) * wh[1] / owh[1]))
      if sxy[0] < 0 or sxy[0] >= wh[0] or sxy[1] < 0 or sxy[1] >= wh[1]:
        continue
      # set a pixel in the PNG
      pos = int(sxy[0] / 8)
      shift = sxy[0] % 8
      rows[sxy[1]][pos] |= 1 << (7 - shift)
    f = StringIO.StringIO()
    w.write_packed(f, rows)
    image = f.getvalue()
    f.close()
    return image

  def GetUserCreds(self, user):
    """Create a dict with the requested password."""
    if user not in ('admin', 'guest'):
      return None
    b64 = self.ReadFile('%s/config/settings/password_%s' % (self.sim, user))
    pw = base64.b64decode(b64)
    return {'auth_username': user, 'auth_password': pw}

  def GetAdminCreds(self, user):
    if user != 'admin':
      return None
    return self.GetUserCreds(user)

  def Authenticate(self, request):
    """Check if user is authenticated (sends challenge if not)."""
    if not request.get_authenticated_user(self.GetUserCreds, self.realm):
      return False
    return True

  def AuthenticateAdmin(self, request):
    """Check if user is authenticated (sends challenge if not)."""
    if not request.get_authenticated_user(self.GetAdminCreds, self.realm):
      return False
    return True

  class CraftHandler(digest.DigestAuthMixin, tornado.web.RequestHandler):
    """Common class to add args to html template."""
    auth = 'unset'

    def IsProxy(self):
      """Check if this request was proxied, (ie, we are the peer)."""
      return self.request.headers.get('craftui-proxy', 0) == '1'

    def IsPeer(self):
      """Check args to see if this is a request for the peer."""
      return self.get_argument('peer', default='0') == '1'

    def IsHttps(self):
      """See if https:// was used."""
      return (self.request.protocol == 'https' or
              self.request.headers.get('craftui-https', 0) == '1')

    def TemplateArgs(self):
      """Build template args to dynamically adjust html file."""
      is_https = self.IsHttps()
      is_proxy = self.IsProxy()

      peer_arg = '?peer=1'

      args = {}
      args['hidden_on_https'] = 'hidden' if is_https else ''
      args['hidden_on_peer'] = 'hidden' if is_proxy else ''
      args['shown_on_peer'] = 'hidden' if not is_proxy else ''
      args['peer_arg'] = peer_arg
      args['peer_arg_on_peer'] = peer_arg if is_proxy else ''
      return args

    def TryProxy(self):
      """Check if we should proxy this request to the peer."""
      if not self.IsPeer() or self.IsProxy():
        return False
      self.Proxy()
      return True

    class ErrorHandler(urllib2.HTTPDefaultErrorHandler):
      """Catch the error, don't raise exception."""
      error = {}

      def http_error_default(self, req, fd, code, msg, hdrs):
        self.error = {
            'request': req,
            'fd': fd,
            'code': code,
            'msg': msg,
            'hdrs': hdrs
        }

    def Proxy(self):
      """Proxy to the peer."""
      ui = self.settings['ui']
      r = self.request
      cs = '/config/settings/'
      peer_ipaddr = ui.ReadFile(ui.sim + cs + 'peer_ipaddr')
      peer_ipaddr = re.sub(r'/\d+$', '', peer_ipaddr)
      if ui.sim:
        peer_ipaddr = 'localhost:8890'
      url = 'http://' + peer_ipaddr + r.uri
      print 'proxy: ', url

      eh = self.ErrorHandler()
      opener = urllib2.build_opener(eh)

      body = None
      if r.method == 'POST':
        body = '' if r.body is None else r.body
      req = urllib2.Request(url, body, r.headers)
      req.add_header('CraftUI-Proxy', 1)
      req.add_header('CraftUI-Https', int(self.IsHttps()))
      fd = opener.open(req, timeout=2)
      if eh.error:
        fd = eh.error['fd']
        self.set_status(eh.error['code'])
        hdrs = eh.error['hdrs']
        for h in hdrs:
          v = hdrs.get(h)
          self.set_header(h, v)

      response = fd.read()
      if response:
        self.write(response)
      self.finish()

    def Authenticated(self):
      """Authenticate the user per the required auth type."""
      ui = self.settings['ui']
      if self.auth == 'any':
        if not ui.Authenticate(self):
          return False
      elif self.auth == 'admin':
        if not ui.AuthenticateAdmin(self):
          return False
      elif self.auth != 'none':
        raise Exception('unknown authentication type "%s"' % self.auth)
      return True

    def get(self):
      if self.TryProxy():
        return
      if not self.Authenticated():
        return
      ui = self.settings['ui']
      path = ui.wwwroot + '/' + self.page + '.thtml'
      print '%s %s page (%s)' % (self.request.method, self.page, ui.sim)
      self.render(path, **self.TemplateArgs())

  class WelcomeHandler(CraftHandler):
    page = 'welcome'
    auth = 'none'

  class StatusHandler(CraftHandler):
    page = 'status'
    auth = 'any'

  class ConfigHandler(CraftHandler):
    page = 'config'
    auth = 'admin'

  class JsonHandler(CraftHandler):
    """Provides JSON-formatted content to be displayed in the UI."""
    page = 'json'

    def get(self):
      if self.TryProxy():
        return
      self.auth = 'any'
      if not self.Authenticated():
        return
      ui = self.settings['ui']
      print '%s %s page (%s)' % (self.request.method, self.page, ui.sim)
      jsonstring = ui.GetData()
      self.set_header('Content-Type', 'application/json')
      self.write(jsonstring)
      self.finish()

    def post(self):
      if self.TryProxy():
        return
      self.auth = 'admin'
      if not self.Authenticated():
        return
      ui = self.settings['ui']
      print '%s %s page (%s)' % (self.request.method, self.page, ui.sim)
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

  class PNGHandler(CraftHandler):
    """Returns a PNG showing plotted IQ values."""
    baseurl = 'http://localhost:8080/api/modem/iq/'
    auth = 'any'
    page = 'IQ'
    path = None

    def get(self):
      if self.TryProxy():
        return
      if not self.Authenticated():
        return
      ui = self.settings['ui']
      print '%s %s page (%s)' % (self.request.method, self.page, ui.sim)

      image = ui.GetIQPNG(self.path)
      self.set_header('Content-Type', 'image/png')
      self.write(image)
      self.finish()

  class RXSlicerPNGHandler(PNGHandler):
    path = 'rxslicer'

  def RunUI(self):
    """Create the http redirect and https web server and run forever."""
    sim = self.sim

    craftui_handlers = [
        (r'^/$', self.WelcomeHandler),
        (r'^/status/?$', self.StatusHandler),
        (r'^/config/?$', self.ConfigHandler),
        (r'^/content.json/?$', self.JsonHandler),
        (r'^/rxslicer.png$', self.RXSlicerPNGHandler),
        (r'^/static/([^/]*)$', tornado.web.StaticFileHandler,
         {'path': self.wwwroot + '/static'}),
    ]

    http_app = tornado.web.Application(craftui_handlers)
    http_app.settings['ui'] = self
    http_app.listen(self.http_port)

    certfile = sim + '/tmp/ssl/certs/craftui.pem'
    keyfile = sim + '/tmp/ssl/private/craftui.key'
    # use the device cert if signed one is not available
    if not os.path.exists(certfile) or not os.path.exists(keyfile):
      certfile = sim + '/tmp/ssl/certs/device.pem'
      keyfile = sim + '/tmp/ssl/private/device.key'

    print 'certfile=', certfile
    print 'keyfile=', keyfile

    https_app = tornado.web.Application(craftui_handlers)
    https_app.settings['ui'] = self
    https_server = tornado.httpserver.HTTPServer(https_app, ssl_options={
        'certfile': certfile, 'keyfile': keyfile})
    https_server.listen(self.https_port)

    ioloop = tornado.ioloop.IOLoop.instance()
    ioloop.start()


def Usage():
  """Show usage."""
  print 'Usage: % [-p)ort 80] [-d)ir web] [-s)im top]'
  print '\tUse -s to provide an alternate rootfs'


def main():
  www = '/usr/craftui/www'
  http_port = 80
  https_port = 443
  sim = ''
  try:
    opts, args = getopt.getopt(sys.argv[1:], 's:p:P:w:S',
                               ['sim=', 'http-port=', 'https-port=', 'www='])
  except getopt.GetoptError as err:
    # print help information and exit:
    print str(err)
    Usage()
    sys.exit(1)
  for o, a in opts:
    if o in ('-s', '--sim'):
      sim = a
    elif o in ('-p', '--http-port'):
      http_port = int(a)
    elif o in ('-P', '--https-port'):
      https_port = int(a)
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
  craftui = CraftUI(www, http_port, https_port, sim)
  craftui.RunUI()


if __name__ == '__main__':
  main()
