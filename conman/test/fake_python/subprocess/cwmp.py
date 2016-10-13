#!/usr/bin/python

"""Fake catawampus implementation."""

import logging
import os

import connection_check


logger = logging.getLogger('subprocess.cwmp')

CONMAN_CONFIG_PATH = None
CWMP_PATH = None
CONFIG = {}
ACCESS_POINT = {}
ACS_SESSION_FAILS = False


def call(command, env=None):
  if command == 'wakeup':
    if not CONMAN_CONFIG_PATH:
      raise ValueError('Call subprocess.set_conman_paths before calling '
                       '"cwmp wakeup".')

    write_acscontact()

    if ACS_SESSION_FAILS:
      return 0, ''

    if ((env and 'write_now_testonly' in env) or
        [result for result in connection_check.RESULTS.itervalues()
         if result in ('restricted', 'succeed')]):
      for band in ('2.4', '5'):
        if CONFIG.get(band, None):
          write_wlan_config(band)
        else:
          delete_wlan_config(band)
          disable_access_point(band)

        if ACCESS_POINT.get(band, False):
          enable_access_point(band)
        else:
          disable_access_point(band)

      logger.debug('Fake ACS session completing')
      write_acsconnected()
    else:
      logger.debug('ACS session failed due to no working connections')

    return 0, ''

  raise ValueError('Fake cwmp only supports "wakeup" command.')


def wlan_config_filename(band):
  return os.path.join(CONMAN_CONFIG_PATH, 'command.%s' % band)


def access_point_filename(band):
  return os.path.join(CONMAN_CONFIG_PATH, 'access_point.%s' % band)


def write_wlan_config(band):
  final_filename = wlan_config_filename(band)
  logger.debug('Writing config for band %s: %s', band, final_filename)
  # We don't care which writes are atomic, as long as some but not all are.
  # Making it depend on band achieves this.
  atomic = band == '2.4'
  filename = final_filename + ('.tmp' if atomic else '')
  with open(filename, 'w') as f:
    f.write('\n'.join(['env', 'WIFI_PSK=%s' % CONFIG[band]['psk'],
                       'wifi', 'set', '--band', band,
                       '--ssid', CONFIG[band]['ssid']]))
    logger.debug(  'wrote to filename %s', filename)
  if atomic:
    logger.debug(  'moving from %s to %s', filename, final_filename)
    os.rename(filename, final_filename)


def enable_access_point(band):
  logger.debug('Enabling AP for band %s', band)
  open(access_point_filename(band), 'w')


def delete_wlan_config(band):
  config_filename = wlan_config_filename(band)
  if os.path.exists(config_filename):
    logger.debug('Deleting config for band %s', band)
    os.unlink(config_filename)


def disable_access_point(band):
  ap_filename = access_point_filename(band)
  if os.path.isfile(ap_filename):
    logger.debug('Disabling AP for band %s', band)
    os.unlink(ap_filename)


def write_acscontact():
  logger.debug('ACS session started')
  open(os.path.join(CWMP_PATH, 'acscontact'), 'w')


def write_acsconnected():
  logger.debug('ACS session completed')
  open(os.path.join(CWMP_PATH, 'acsconnected'), 'w')


def mock(band, access_point=None, delete_config=False, ssid=None, psk=None,
         write_now=False, acs_session_fails=None):
  """Mock the config written by catawampus.

  Args:
    band:  The band for which things are being mocked.
    access_point:  Set to True or False to enable/disable the AP.
    delete_config:  Set to True to delete the config.
    ssid:  If updating config, the ssid to use.  psk must also be set.
    psk:  If updating config, the psk to use.  ssid must also be set.
    write_now:  If updating config, write it immediately.

  Raises:
    ValueError:  If invalid values are specified.
  """
  if acs_session_fails is not None:
    global ACS_SESSION_FAILS
    ACS_SESSION_FAILS = acs_session_fails

  if access_point is not None:
    if access_point not in (True, False):
      raise ValueError('access_point should only be mocked as True/False')
    ACCESS_POINT[band] = access_point
    logger.debug('AP mocked %s', access_point)

  if delete_config:
    logger.debug('Config mock removed for band %s', band)
    CONFIG[band] = None
  elif ssid and psk:
    logger.debug('Config mock updated for band %s', band)
    CONFIG[band] = {'ssid': ssid, 'psk': psk}
  elif ssid or psk:
    raise ValueError('Cannot set only one of ssid (%r) and psk (%r).',
                     ssid, psk)

  if write_now:
    call('wakeup', env={'write_now_testonly': True})


