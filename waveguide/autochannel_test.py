#!/usr/bin/python
import time
import autochannel
import wgdata
from wvtest import wvtest


@wvtest.wvtest
def OverlapsTest():
  wvtest.WVPASSGE(autochannel.Overlaps20(2412, 2411), 2)  # partial overlap
  wvtest.WVPASSEQ(autochannel.Overlaps20(2412, 2412), 1)  # clean overlap
  wvtest.WVPASS(autochannel.Overlaps20(2412, 2393))
  wvtest.WVFAIL(autochannel.Overlaps20(2412, 2392))
  wvtest.WVPASS(autochannel.Overlaps20(2412, 2412))
  wvtest.WVPASS(autochannel.Overlaps20(2412, 2417))
  wvtest.WVPASS(autochannel.Overlaps20(2412, 2422))
  wvtest.WVPASS(autochannel.Overlaps20(2412, 2427))
  wvtest.WVPASS(autochannel.Overlaps20(2412, 2431))
  wvtest.WVFAIL(autochannel.Overlaps20(2412, 2432))

  wvtest.WVPASS(autochannel.Overlaps40(2412, 2432))
  wvtest.WVPASS(autochannel.Overlaps40(2412, 2451))

  # This one is non-obvious!  Because on 2.4 GHz, 40 MHz wide channels are so
  # flexible, any 40 MHz channel overlaps with any other 40 MHz channel, even
  # if the primary channels are >= 40 MHz apart.  For example, imagine an
  # AP on (2412,2432) and another on (2432,2452).  They will definitely
  # interfere with each other.
  #
  # To narrow this down, we cheat.  Overlaps40 treats the first parameter
  # as a verbatim 20 MHz channel, and searches for the second parameter
  # as part of a channel group.
  wvtest.WVFAIL(autochannel.Overlaps40(2412, 2452))

  # On the other hand, on 5 GHz, 40 MHz channel pairs are better defined,
  # so there are far fewer risky combinations.
  wvtest.WVFAIL(autochannel.Overlaps40(5745, 5725))
  wvtest.WVPASS(autochannel.Overlaps40(5745, 5765))
  wvtest.WVPASS(autochannel.Overlaps40(5745, 5784))
  wvtest.WVFAIL(autochannel.Overlaps40(5745, 5785))

  # There are no 80 MHz channels on 2.4 GHz, so it's always the same answer
  # as for 40 MHz.
  wvtest.WVPASS(autochannel.Overlaps80(2412, 2432))
  wvtest.WVFAIL(autochannel.Overlaps80(2412, 2452))
  wvtest.WVFAIL(autochannel.Overlaps80(2412, 2462))

  # On 5 GHz, 80 MHz pairs exist and are well-defined.
  wvtest.WVFAIL(autochannel.Overlaps80(5745, 5725))
  wvtest.WVPASS(autochannel.Overlaps80(5745, 5765))
  wvtest.WVPASS(autochannel.Overlaps80(5745, 5784))
  wvtest.WVPASS(autochannel.Overlaps80(5745, 5785))


@wvtest.wvtest
def LegalCombosTest():
  r = list(autochannel.LegalCombos(allowed_freqs=[],
                                   candidates=autochannel.C_ALL))
  wvtest.WVPASSEQ(r, [])
  r = list(autochannel.LegalCombos(allowed_freqs=[2432],
                                   candidates=autochannel.C_ALL))
  wvtest.WVPASSEQ(r, [(2432,)])
  r = list(autochannel.LegalCombos(allowed_freqs=[2412, 2432, 2452],
                                   candidates=autochannel.C_ALL))
  wvtest.WVPASSEQ(r, [(2412, 2432), (2412,), (2432,), (2432, 2452,), (2452,)])
  r = list(autochannel.LegalCombos(allowed_freqs=[2412, 2432, 2452],
                                   candidates=autochannel.C_24MAIN))
  wvtest.WVPASSEQ(r, [(2412, 2432), (2412,), (2432,)])
  r = list(autochannel.LegalCombos(allowed_freqs=range(5745, 5826, 20),
                                   candidates=autochannel.C_ALL))
  wvtest.WVPASSEQ(r, [(5745, 5765, 5785, 5805),
                      (5745, 5765),
                      (5785, 5805),
                      (5745,),
                      (5765,),
                      (5785,),
                      (5805,),
                      (5825,)])


def GenState(bssfreqs, surveys):
  me = wgdata.Me(now=0,
                 uptime_ms=0,
                 consensus_key='',
                 mac='',
                 flags=0)
  bss_list = [wgdata.BSS(is_ours=False, mac='', freq=freq, rssi=-40,
                         flags=0, last_seen=time.time())
              for freq in bssfreqs]
  survey_list = [wgdata.Channel(freq=freq, noise_dbm=0,
                                observed_ms=observed_ms, busy_ms=busy_ms)
                 for freq, observed_ms, busy_ms in surveys]
  return wgdata.State(me=me, seen_bss=bss_list, channel_survey=survey_list,
                      assoc=[], arp=[])


@wvtest.wvtest
def AutoChannelTest():
  combos = list(autochannel.LegalCombos(range(2412, 2500, 5),
                                        autochannel.C_24ANY))
  combos_20 = [i for i in combos if len(i) == 1]

  # With no other APs and all idle channels, every channel is equally likely.
  results = set()
  for i in range(10000):
    results.add(autochannel.SoloChooseChannel(
        GenState([], []), combos, use_primary_spreading=True,
        use_active_time=True, hysteresis_freq=None).primary_freq)
    if len(results) == 11:
      break
  wvtest.WVPASSGE(len(results), 11)

  # Every channel is equally likely, but with hysteresis, we pick the
  # same channel every time.
  result1 = autochannel.SoloChooseChannel(
      GenState([], []), combos, use_primary_spreading=True,
      use_active_time=True, hysteresis_freq=None).primary_freq
  results = set()
  for i in range(100):
    results.add(autochannel.SoloChooseChannel(
        GenState([], []), combos, use_primary_spreading=True,
        use_active_time=True, hysteresis_freq=result1).primary_freq)
  print results
  wvtest.WVPASSEQ(len(results), 1)

  result1 = autochannel.SoloChooseChannel(
      GenState([], []), combos, use_primary_spreading=False,
      use_active_time=True, hysteresis_freq=None).primary_freq
  results = set()
  for i in range(100):
    results.add(autochannel.SoloChooseChannel(
        GenState([], []), combos, use_primary_spreading=False,
        use_active_time=True, hysteresis_freq=result1).primary_freq)
  print results
  wvtest.WVPASSEQ(len(results), 1)

  # With a single AP at 2432, the system should try for a 40 MHz channel
  # that either doesn't include 2432 (impossible) or aligns so it
  # at least doesn't partially overlap with 2432.  That means one of the
  # two channels should be 2432.  And we want it to *not* be the primary
  # channel, so that 20 MHz traffic is non-interfering.
  r = autochannel.SoloChooseChannel(
      GenState([2432], []), combos, use_primary_spreading=True,
      use_active_time=True, hysteresis_freq=None)
  print r
  wvtest.WVPASSNE(r.primary_freq, 2432)
  wvtest.WVPASSEQ(len(r.group), 2)
  wvtest.WVPASS(r.primary_freq in [2412, 2452])
  wvtest.WVPASSEQ(r.count_80, 1)

  # With use_primary_spreading=False, the logic above should all work the
  # same, but now it should choose 2432 as the primary every time.
  r = autochannel.SoloChooseChannel(
      GenState([2432], []), combos, use_primary_spreading=False,
      use_active_time=True, hysteresis_freq=None)
  print r
  wvtest.WVPASSEQ(r.primary_freq, 2432)
  wvtest.WVPASSEQ(len(r.group), 2)
  wvtest.WVPASSEQ(r.count_80, 1)

  # A bad scenario: two existing APs too close together in the middle of the
  # spectrum.  There's no way to get a 40 MHz channel without some kind of
  # partial overlap.  With use_primary_spreading=True, at least the primary
  # channel will be clear.
  r = autochannel.SoloChooseChannel(
      GenState([2432, 2437], []), combos, use_primary_spreading=True,
      use_active_time=True, hysteresis_freq=None)
  print r
  wvtest.WVPASS(r.primary_freq in [2412, 2457, 2462])
  # TODO(apenwarr): really we should switch to 20 MHz wide in this case.
  #  Anything to avoid a partial overlap.  But we don't yet.
  # wvtest.WVPASSEQ(len(r.group), 1)
  wvtest.WVPASSGT(r.count_80, 1)
  wvtest.WVPASSGT(r.count_40, 1)
  wvtest.WVPASSEQ(r.count_20, 0)

  r = autochannel.SoloChooseChannel(
      GenState([2432, 2437], []), combos, use_primary_spreading=False,
      use_active_time=True, hysteresis_freq=None)
  print r
  wvtest.WVPASS(r.primary_freq in [2432, 2437])
  wvtest.WVPASSGT(r.count_80, 1)
  wvtest.WVPASSGT(r.count_40, 1)
  wvtest.WVPASSGT(r.count_20, 1)

  # When we force to a 20 MHz channel, it should not have any overlap,
  # regardless of use_primary_spreading.
  r = autochannel.SoloChooseChannel(
      GenState([2432, 2437], []), combos_20, use_primary_spreading=False,
      use_active_time=True, hysteresis_freq=None)
  print r
  wvtest.WVPASS(r.primary_freq in [2412, 2457, 2462])
  wvtest.WVPASSEQ(r.count_20, 0)

  # If there are two APs on 2432, it's better to partially overlap with
  # those than with the one AP on 2437.
  r = autochannel.SoloChooseChannel(
      GenState([2432, 2432, 2437], []), combos, use_primary_spreading=False,
      use_active_time=True, hysteresis_freq=None)
  print r
  wvtest.WVPASSEQ(r.primary_freq, 2432)
  r = autochannel.SoloChooseChannel(
      GenState([2432, 2432, 2437], []), combos, use_primary_spreading=True,
      use_active_time=True, hysteresis_freq=None)
  print r
  wvtest.WVPASSEQ(r.primary_freq, 2412)
  # Even when we force 20 MHz channels, only 2412 is the right match.
  # The system considers 2412+2432, 2417+2437, 2432+2452, and 2437+2457 are
  # all in use, some of them twice.  2412 has the fewest *partial* overlaps.
  # (This can be improved if we teach waveguide about HT40+ vs HT40-.)
  r = autochannel.SoloChooseChannel(
      GenState([2432, 2432, 2437], []), combos_20, use_primary_spreading=True,
      use_active_time=True, hysteresis_freq=None)
  print r
  wvtest.WVPASS(r.primary_freq in [2412, 2452, 2457, 2462])

  # Check that a busy channel overrides other logic.
  # With 2412 ruled out, 2452 is the next best match.
  r = autochannel.SoloChooseChannel(
      GenState([2432, 2432, 2437], [(2412, 1000, 100)]),
      combos_20, use_primary_spreading=True,
      use_active_time=True, hysteresis_freq=None)
  print r
  wvtest.WVPASSEQ(r.primary_freq, 2452)

  # Make sure use_active_time=False causes us to ignore busy channels.
  r = autochannel.SoloChooseChannel(
      GenState([2432, 2432, 2437], [(2412, 1000, 100)]),
      combos_20, use_primary_spreading=True,
      use_active_time=False, hysteresis_freq=None)
  print r
  wvtest.WVPASSEQ(r.primary_freq, 2412)

if __name__ == '__main__':
  wvtest.wvtest_main()
