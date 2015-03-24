#!/usr/bin/python -S

"""Utils related to automatic channel selection."""

import random
import subprocess
import time

import utils


MODES = {
    '24MAIN_20': '2412 2432 2462',
    '24MAIN_40': '2412',
    '24MAIN_80': '',
    '24OVERLAP_20': '2412 2417 2422 2427 2432 2437 2442 2447 2452 2457 2462',
    '24OVERLAP_40': '2412',
    '24OVERLAP_80': '',
    '5LOW_20': '5180 5200 5220 5240',
    '5LOW_40': '5180 5220',
    '5LOW_80': '5180',
    '5HIGH_20': '5745 5765 5785 5805 5825',
    '5HIGH_40': '5745 5785',
    '5HIGH_80': '5745',
    '5DFS_20': '5260 5280 5300 5320 5500 5520 5540 5560 5580 5660 5680 5700',
    '5DFS_40': '5260 5300 5500 5540 5660',
    '5DFS_80': '5260 5500',
}


def get_permitted_frequencies(band, autotype, width):
  try:
    return {
        ('2.4', 'LOW', '20'): MODES['24MAIN_20'],
        ('2.4', 'LOW', '40'): MODES['24MAIN_40'],
        ('2.4', 'HIGH', '20'): MODES['24MAIN_20'],
        ('2.4', 'HIGH', '40'): MODES['24MAIN_40'],
        ('2.4', 'NONDFS', '20'): MODES['24MAIN_20'],
        ('2.4', 'NONDFS', '40'): MODES['24MAIN_40'],
        ('2.4', 'ANY', '20'): MODES['24MAIN_20'],
        ('2.4', 'ANY', '40'): MODES['24MAIN_40'],
        ('2.4', 'OVERLAP', '20'): MODES['24OVERLAP_20'],
        ('2.4', 'OVERLAP', '40'): MODES['24OVERLAP_40'],
        ('5', 'LOW', '20'): MODES['5LOW_20'],
        ('5', 'LOW', '40'): MODES['5LOW_40'],
        ('5', 'LOW', '80'): MODES['5LOW_80'],
        ('5', 'HIGH', '20'): MODES['5HIGH_20'],
        ('5', 'HIGH', '40'): MODES['5HIGH_40'],
        ('5', 'HIGH', '80'): MODES['5HIGH_80'],
        ('5', 'DFS', '20'): MODES['5DFS_20'],
        ('5', 'DFS', '40'): MODES['5DFS_40'],
        ('5', 'DFS', '80'): MODES['5DFS_80'],
        ('5', 'NONDFS', '20'): ' '.join((MODES['5LOW_20'], MODES['5HIGH_20'])),
        ('5', 'NONDFS', '40'): ' '.join((MODES['5LOW_40'], MODES['5HIGH_40'])),
        ('5', 'NONDFS', '80'): ' '.join((MODES['5LOW_80'], MODES['5HIGH_80'])),
        ('5', 'ANY', '20'): ' '.join((MODES['5LOW_20'], MODES['5HIGH_20'],
                                      MODES['5DFS_20'])),
        ('5', 'ANY', '40'): ' '.join((MODES['5LOW_40'], MODES['5HIGH_40'],
                                      MODES['5DFS_40'])),
        ('5', 'ANY', '80'): ' '.join((MODES['5LOW_80'], MODES['5HIGH_80'],
                                      MODES['5DFS_80'])),
    }[(band, autotype, width)]
  except KeyError:
    raise ValueError('Unknown autochannel type: band=%s autotype=%s width=%s'
                     % (band, autotype, width))


def scan(interface, band, autotype, width):
  """Do an autochannel scan and return the recommended channel.

  Args:
    interface: The interface on which to scan.
    band: The band on which to scan.
    autotype: Determines permitted frequencies.  See get_permitted_frequencies
      for valid values.
    width: Determines permitted frequencies.  See get_permitted_frequencies for
      valid values.

  Returns:
    The channel to use, or None if no recommendation can be made.
  """
  utils.log('Doing autochannel scan.')

  permitted_frequencies = get_permitted_frequencies(band, autotype, width)

  subprocess.call(['ip', 'link', 'set', interface, 'up'])

  # TODO(apenwarr): we really want to clear any old survey results first. But
  #  there seems to be no iw command for that yet...
  # TODO(apenwarr): This only scans each channel for 100ms. Ideally it should
  #  scan for longer, to get a better activity sample. It would also be nice to
  #  continue scanning in the background while hostapd is running, using 'iw
  #  offchannel'. Retry this a few times if it fails, just in case there was a
  #  scan already in progress started somewhere else (eg. from waveguide).
  for _ in range(9):
    if utils.subprocess_quiet(['iw', 'dev', interface, 'scan', 'passive'],
                              no_stdout=True):
      break
    time.sleep(0.5)

  # TODO(apenwarr): this algorithm doesn't deal with overlapping channels. Just
  #  because channel 1 looks good doesn't mean we should use it; activity in
  #  overlapping channels could destroy performance.  In fact, overlapping
  #  channel activity is much worse than activity on the main channel.  Also, if
  #  using 40 MHz or 80 MHz channel width, we should count activity in all the
  #  20 MHz sub-channels separately, and choose the least-active sub-channel as
  #  the primary.
  best_frequency = best_noise = best_ratio = frequency = None
  for tokens in utils.subprocess_line_tokens(
      ['iw', 'dev', interface, 'survey', 'dump']):
    # TODO(apenwarr): Randomize the order of channels. Otherwise when channels
    #  are all about equally good, we would always choose exactly the same
    #  channel, which might be bad in the case of hidden nodes.
    if len(tokens) >= 2 and tokens[0] == 'frequency:':
      frequency = tokens[1]
      noise = active = busy = None
    elif len(tokens) >= 2 and tokens[0] == 'noise:':
      noise = int(tokens[1])
    elif len(tokens) >= 4 and ' '.join(tokens[:3]) == 'channel active time:':
      active = int(tokens[3])
    elif len(tokens) >= 4 and ' '.join(tokens[:3]) == 'channel receive time:':
      busy = int(tokens[3])
      ratio = (active + 1) * 1000 / (busy + 1)

      if frequency not in permitted_frequencies.split():
        continue

      # some radios support both bands, but we only want to match channels on
      # the band we have chosen.
      if band[0] != frequency[0]:
        continue

      utils.log('freq=%s ratio=%s noise=%s', frequency, ratio, noise)

      if best_noise is None or best_noise - 15 > noise or best_ratio < ratio:
        best_frequency, best_ratio, best_noise = frequency, ratio, noise

  if not best_frequency:
    utils.log('autoscan did not find any channel, picking random channel')
    utils.log('permitted frequencies: %s', permitted_frequencies)
    if not permitted_frequencies:
      utils.log('no default channel: type=%s band=%s width=%s',
                autotype, band, width)
      return None
    best_frequency = random.choice(permitted_frequencies.split())

  utils.log('autofreq=%s', best_frequency)

  for tokens in utils.subprocess_line_tokens(['iw', 'phy']):
    if len(tokens) >= 4 and tokens[2] == 'MHz':
      frequency = tokens[1]
      if frequency == best_frequency:
        channel = tokens[3].strip('[]')
        break

  if not channel:
    utils.log('No channel number matched freq=%s', best_frequency)
    return None

  utils.log('autochannel=%s', channel)

  return channel
