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
#
# pylint:disable=invalid-name

"""Wifi auto channel selection algorithm for waveguide."""

from collections import namedtuple
import random
import time


LAST_SEEN_THRESHOLD = 600   # maximum seconds before ignoring another AP


# TODO(apenwarr): consider including non-US channel pairs.

# On 2.4 GHz, we can use any 40MHz-wide span, although most of them
# are partially overlapping with each other.  Thus, we split them into
# a set of "main" options (strictly non-overlapping) and "all"
# (including overlapping) options.
C_24MAIN = [
    # [MHz]        # [Channel]
    (2412, 2432),  # 1, 5
    (2462,),       # 11
]
C_24ANY = [
    (2412, 2432),  # 1, 5
    (2417, 2437),  # 2, 6
    (2422, 2442),  # 3, 7
    (2427, 2447),  # 4, 8
    (2432, 2452),  # 5, 9
    (2437, 2457),  # 6, 10
    (2442, 2462),  # 7, 11
]

# On 5 GHz, only certain combinations are available.  This avoids the problem
# of partial overlap, although it leaves some channels stranded, never
# to be used as 40 or 80 MHz channels since they have no partners.

# Low-power channels.  These have trouble making it through walls.
# TODO(apenwarr): new FCC rules allow these to transmit at high power.
#   When those rules kick in, we then have to figure out whether the AP and
#   client devices are *able* to use high power on these channels.  For
#   now, we punt on the whole thing.  Just treat them as strictly low power.
C_5LOW = [
    (5180, 5200, 5220, 5240),  # 36, 40, 44, 48
]

# High-power channels.  Roughly comparable in range to 2.4 GHz.
C_5HIGH = [
    (5745, 5765, 5785, 5805),  # 149, 153, 157, 161
    (5825,),                   # 165
]

# DFS channels are only usable if your AP chipset+driver support radar
# detection.  Also, hostapd might randomly change the channel away from what
# we selected, if it (thinks it) detects radar using the channel.
# This probably reveals driver bugs, plus makes channel selection harder,
# since the "least used" channel might be one where everyone has already
# detected radar and moved away.
C_5DFS = [
    (5260, 5280, 5300, 5320),  # 52, 56, 60, 64
    (5500, 5520, 5540, 5560),  # 100, 104, 108, 112
    (5580,),                   # 116
    (5660, 5680),              # 132, 136
    (5700,),                   # 140
]


# Some meaningful combinations.
C_5NONDFS = C_5LOW + C_5HIGH
C_5ANY = C_5NONDFS + C_5DFS
C_ALL = C_24ANY + C_5ANY


def Overlaps20(f1, f2):
  """Returns nonzero if f1 and f2 are in the same 20 MHz channel.

  Args:
    f1: the first frequency to compare.
    f2: the second frequency to compare.

  Returns:
    0: if f1 and f2 are non-overlapping.
    1: if f1 == f2 (perfect overlap).
    >1: if f1 and f2 are partially overlapping.
  """
  if f1 == f2:
    return 1
  elif abs(f1 - f2) < 20:
    return 10
  else:
    return 0


def _OverlapsInGroup(f1, f2, group):
  if f1 not in group:
    return 0
  else:
    return max(Overlaps20(f2, i) for i in group)


def Overlaps40(f1, f2):
  """Returns nonzero if f2 is in a 40 MHz channel defined by f1."""
  # TODO(apenwarr): slightly wrong in 2.4 GHz spectrum.
  #   There's more than one possible 40 MHz channel for most
  #   frequencies (HT40+ and HT40-).  This will return nonzero if
  #   *any* of them match, which is too pessimistic.
  for group in C_ALL:
    # Check each 40 MHz subset of the given channel group
    for i in xrange(0, len(group), 2):
      v = _OverlapsInGroup(f1, f2, group[i:i+2])
      if v: return v
  return 0


def Overlaps80(f1, f2):
  """Returns nonzero if f2 is in an 80 MHz channel defined by f1."""
  for group in C_ALL:
    v = _OverlapsInGroup(f1, f2, group)
    if v: return v
  return 0


def _LegalCombos(allowed_freqs, candidates):
  for group in candidates:
    if all(f in allowed_freqs for f in group):
      yield group
    for i in range(0, len(group), 2):
      if all(f in allowed_freqs for f in group[i:i+2]):
        yield group[i:i+2]
    for f in group:
      if f in allowed_freqs:
        yield (f,)


def LegalCombos(allowed_freqs, candidates):
  """Yields the channel combo candidates that contain only allowed freqs."""
  already = set()
  for i in _LegalCombos(allowed_freqs, candidates):
    if i not in already:
      already.add(i)
      yield i


ChannelActivity = namedtuple(
    'ChannelActivity',
    'primary_freq,group,count_20,count_40,count_80,busy_20,busy_40,busy_80')


def ChannelActivityList(state, candidates, airtime_threshold_ms):
  """For each candidate, yields information about activity/overlap.

  Args:
    state: a wgdata.State representing the AP's view of the world.
    candidates: the output of LegalCombos().
    airtime_threshold_ms: the minimum time a channel must have been observed
      before it can have nonzero activity.

  Yields:
    A ChannelActivity namedtuple:
    (primary_freq,                 # primary channel freq
     group,                        # element of candidates[]
     count_20, count_40, count_80, # number of other APs in 20/40/80 MHz
     busy_20, busy_40, busy_80)    # fraction of airtime used in 20/40/80 MHz

    Note: busy_* is set to zero if the total observed time is less
    than airtime_threshold_ms (milliseconds), to ensure a channel
    isn't unfairly penalized for being busy during one or two short
    samples.  waveguide generally scans channels for ~100ms at a time,
    so a threshold of ~1000ms would be 10 complete scan periods, which
    should be a much better measure than simply watching a channel for
    1000 consecutive milliseconds.
  """
  now = time.time()
  for group in candidates:
    for primary_freq in group:
      count_20 = count_40 = count_80 = 0
      busy_20 = busy_40 = busy_80 = 0.0
      for bss in state.seen_bss:
        if bss.last_seen < now - LAST_SEEN_THRESHOLD:
          continue
        # A "full overlap" (another AP on a 20 MHz multiple away from this
        # one) is not too bad, because 802.11 has the ability to reserve
        # the secondary channel explicitly before using it.  But
        # experimentally, partial overlap is much worse.  OverlapsXX() return
        # a weight of 1 for full overlap, or >1 for partial overlap.
        count_20 += Overlaps20(bss.freq, primary_freq)
        count_40 += Overlaps40(bss.freq, primary_freq)
        count_80 += Overlaps80(bss.freq, primary_freq)
      for channel in state.channel_survey:
        if (not channel.observed_ms or
            channel.observed_ms < airtime_threshold_ms):
          busy = 0.0
        else:
          busy = channel.busy_ms * 1.0 / channel.observed_ms
        # Note: these busy fraction calculations are incorrect, because
        # we don't know which overlapping subchannels were active at the
        # same time. For simplicity, we simply sum the activity of
        # all overlapping channels.  Thus, the activity fraction for 40/80 MHz
        # could be >1.0 on any band, and on 2.4 GHz (where multiple 5 MHz
        # channels can overlap) even 20 MHz could be >1.0.  Strictly
        # speaking, this formula is nonsense, but it does accomplish
        # the main goals:
        #   - penalizes wide channels that have multiple active subchannels
        #     more than ones with few active subchannels.
        #   - even further penalizes channels where there's activity
        #     on more than one partially overlapping channel.
        busy_20 += busy * Overlaps20(channel.freq, primary_freq)
        busy_40 += busy * Overlaps40(channel.freq, primary_freq)
        busy_80 += busy * Overlaps80(channel.freq, primary_freq)
      yield ChannelActivity(primary_freq, group,
                            count_20, count_40, count_80,
                            busy_20, busy_40, busy_80)


def SoloChooseChannel(state, candidates, use_primary_spreading):
  """Basic single-AP channel selection.

  This simple algorithm ignores waveguide peers and uses only the channel
  scan information visible from the given AP.  Because it doesn't attempt
  to coordinate channel selection, it could lead to oscillation if several
  devices are using the same method and recalculate their channels
  periodically.

  Args:
    state: the wgdata.State object representing the state of this AP.
      (You could provide the State object learned from a waveguide peer
       to calculate which channel it would auto-choose when using this
       algorithm.)
    candidates: the output of LegalCombos().
    use_primary_spreading: if true, try to equalize the use of different
      primary channels within a 40 or 80 MHz band.  (Testing seems to show
      this gives best results.) If false, choose the most popular primary
      channel and use that (common folklore says this is best).
  Returns:
    A ChannelActivity object representing the best choice of channel.
    (See ChannelActivityList for interpretation of fields.)
  Raises:
    ValueError: if no valid channel selection candidates are provided.
  """
  if not candidates:
    raise ValueError('no channel selection candidates provided')
  activities = list(ChannelActivityList(state, candidates,
                                        airtime_threshold_ms=1000))

  # TODO(apenwarr): consider auto-narrowing channel width in some cases.
  #  That may or may not make things better.  In theory, the driver can
  #  do this per-packet as part of its rate control and CSMA/CA.  If that
  #  works correctly, it can theoretically reduce contention more than using
  #  narrower channels.  But we'd need to experiment more to be sure.

  # The reasoning for this sort order is as follows:
  #  - Longer groups are wider channels.  Use the widest channel available.
  #  - Assume that every competing AP is using the widest channels possible.
  #    That means even if we were using 20 MHz channels, competing activity
  #    on an adjacent channel might impact us if the other AP is using 40 or
  #    80 MHz width.  Thus, sort by 80 MHz busy and count values first.
  #  - Given equal 80 MHz activity, we can pick the subchannel with the least
  #    activity and make it the primary.
  #  - Total airtime is more informative than the count of APs
  #    (since most of them might be mostly idle, and one super busy AP can
  #    ruin a whole channel).  But total airtime takes a long time to measure.
  #    So ChannelActivityList will blank out the busy_* times if they aren't
  #    available, making them all sort equal, and only then do we fall back
  #    to count_*.
  #  - In the worst case where *all* channels appear equal for some reason
  #    (probably a bug), choose one at random rather than accidentally setting
  #    every AP to the same channel somehow.
  # pylint:disable=g-long-lambda
  activities.sort(key=lambda i: (-len(i.group),
                                 i.busy_80, i.count_80,
                                 i.busy_40, i.count_40,
                                 i.busy_20, i.count_20,
                                 random.random()))
  if use_primary_spreading:
    # simply use the primary channel with the least activity.
    return activities[0]
  else:
    # pick the most popular primary channel inside the group.
    # Rumour has it that this makes CSMA/CA work better with a mix of old/new
    # clients.
    bestgroup = activities[0].group
    activities = [i for i in activities if i.group == bestgroup]
    activities.sort(key=lambda i: (-i.count_20, random.random()))
    return activities[0]
