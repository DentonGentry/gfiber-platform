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
import sys
import urllib2
import tornado.ioloop
import tornado.web


class CraftUI(object):
  """A web server that configures and displays Chimera data."""

  def __init__(self, wwwroot, port, sim):
    """initialize."""
    self.wwwroot = wwwroot
    self.port = port
    self.sim = sim
    self.data = {}
    self.data['refreshCount'] = 0

  def ReadFile(self, filepath):
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

  def GetPlatformData(self):
    """Get platform data, return a json string."""
    data = self.data
    sim = self.sim

    if data['refreshCount'] == 0:
      data['version'] = self.ReadFile(sim + '/etc/version')
      data['platform'] = self.ReadFile(sim + '/etc/platform')
      data['softwaredate'] = self.ReadFile(sim + '/etc/softwaredate')
    data['refreshCount'] += 1
    data['uptime'] = self.ReadFile(sim + '/proc/uptime')
    data['ledstate'] = self.ReadFile(sim + '/tmp/gpio/ledstate')
    cs = '/config/settings/'
    data['craft_ipaddr'] = self.ReadFile(sim + cs + 'craft_ipaddr')
    data['local_ipaddr'] = self.ReadFile(sim + cs + 'local_ipaddr')
    data['peer_ipaddr'] = self.ReadFile(sim + cs + 'peer_ipaddr')
    data['vlan_inband'] = self.ReadFile(sim + cs + 'vlan_inband')
    data['vlan_peer'] = self.ReadFile(sim + cs + 'vlan_peer')
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
    """Displays the UI."""

    def get(self):
      ui = self.settings['ui']
      print 'GET craft HTML page'
      self.render(ui.wwwroot + '/index.thtml', peerurl='http://TODO')

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

  def RunUI(self):
    """Create the web server and run forever."""
    handlers = [
        (r'/', CraftUI.MainHandler),
        (r'/content.json', CraftUI.JsonHandler),
        (r'/restart', CraftUI.RestartHandler),
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
