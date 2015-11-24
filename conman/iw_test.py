#!/usr/bin/python

"""Tests for iw.py."""

import iw
from wvtest import wvtest


SCAN_OUTPUT = """BSS 00:23:97:57:f4:d8(on wcli0)
  TSF: 1269828266773 usec (14d, 16:43:48)
  freq: 2437
  beacon interval: 100 TUs
  capability: ESS Privacy ShortSlotTime (0x0411)
  signal: -60.00 dBm
  last seen: 2190 ms ago
  Information elements from Probe Response frame:
  Vendor specific: OUI 00:11:22, data: 01 23 45 67
  SSID: short scan result
  Supported rates: 1.0* 2.0* 5.5* 11.0* 18.0 24.0 36.0 54.0
  DS Parameter set: channel 6
  ERP: <no flags>
  ERP D4.0: <no flags>
  Privacy:  WEP
  Extended supported rates: 6.0 9.0 12.0 48.0
BSS 94:b4:0f:f1:02:a0(on wcli0)
  TSF: 16233722683 usec (0d, 04:30:33)
  freq: 2412
  beacon interval: 100 TUs
  capability: ESS Privacy ShortPreamble SpectrumMgmt ShortSlotTime RadioMeasure (0x1531)
  signal: -54.00 dBm
  last seen: 2490 ms ago
  Information elements from Probe Response frame:
  SSID: Google
  Supported rates: 36.0* 48.0 54.0
  DS Parameter set: channel 1
  Country: US Environment: Indoor/Outdoor
    Channels [1 - 11] @ 36 dBm
  Power constraint: 0 dB
  TPC report: TX power: 3 dBm
  ERP: <no flags>
  RSN:   * Version: 1
     * Group cipher: CCMP
     * Pairwise ciphers: CCMP
     * Authentication suites: IEEE 802.1X
     * Capabilities: 4-PTKSA-RC 4-GTKSA-RC (0x0028)
  BSS Load:
     * station count: 0
     * channel utilisation: 33/255
     * available admission capacity: 25625 [*32us]
  HT capabilities:
    Capabilities: 0x19ad
      RX LDPC
      HT20
      SM Power Save disabled
      RX HT20 SGI
      TX STBC
      RX STBC 1-stream
      Max AMSDU length: 7935 bytes
      DSSS/CCK HT40
    Maximum RX AMPDU length 65535 bytes (exponent: 0x003)
    Minimum RX AMPDU time spacing: 4 usec (0x05)
    HT RX MCS rate indexes supported: 0-23
    HT TX MCS rate indexes are undefined
  HT operation:
     * primary channel: 1
     * secondary channel offset: no secondary
     * STA channel width: 20 MHz
     * RIFS: 1
     * HT protection: nonmember
     * non-GF present: 0
     * OBSS non-GF present: 1
     * dual beacon: 0
     * dual CTS protection: 0
     * STBC beacon: 0
     * L-SIG TXOP Prot: 0
     * PCO active: 0
     * PCO phase: 0
  Overlapping BSS scan params:
     * passive dwell: 20 TUs
     * active dwell: 10 TUs
     * channel width trigger scan interval: 300 s
     * scan passive total per channel: 200 TUs
     * scan active total per channel: 20 TUs
     * BSS width channel transition delay factor: 5
     * OBSS Scan Activity Threshold: 0.25 %
  Extended capabilities: HT Information Exchange Supported, Extended Channel Switching, BSS Transition, 6
  WMM:   * Parameter version 1
     * u-APSD
     * BE: CW 15-1023, AIFSN 3
     * BK: CW 15-1023, AIFSN 7
     * VI: CW 7-15, AIFSN 2, TXOP 3008 usec
     * VO: CW 3-7, AIFSN 2, TXOP 1504 usec
BSS 94:b4:0f:f1:35:60(on wcli0)
  TSF: 1739987968 usec (0d, 00:28:59)
  freq: 2462
  beacon interval: 100 TUs
  capability: ESS Privacy ShortPreamble SpectrumMgmt ShortSlotTime RadioMeasure (0x1531)
  signal: -39.00 dBm
  last seen: 1910 ms ago
  Information elements from Probe Response frame:
  SSID: Google
  Supported rates: 36.0* 48.0 54.0
  DS Parameter set: channel 11
  Country: US Environment: Indoor/Outdoor
    Channels [1 - 11] @ 36 dBm
  Power constraint: 0 dB
  TPC report: TX power: 3 dBm
  ERP: <no flags>
  RSN:   * Version: 1
     * Group cipher: CCMP
     * Pairwise ciphers: CCMP
     * Authentication suites: IEEE 802.1X
     * Capabilities: 4-PTKSA-RC 4-GTKSA-RC (0x0028)
  BSS Load:
     * station count: 0
     * channel utilisation: 49/255
     * available admission capacity: 26875 [*32us]
  HT capabilities:
    Capabilities: 0x19ad
      RX LDPC
      HT20
      SM Power Save disabled
      RX HT20 SGI
      TX STBC
      RX STBC 1-stream
      Max AMSDU length: 7935 bytes
      DSSS/CCK HT40
    Maximum RX AMPDU length 65535 bytes (exponent: 0x003)
    Minimum RX AMPDU time spacing: 4 usec (0x05)
    HT RX MCS rate indexes supported: 0-23
    HT TX MCS rate indexes are undefined
  HT operation:
     * primary channel: 11
     * secondary channel offset: no secondary
     * STA channel width: 20 MHz
     * RIFS: 1
     * HT protection: nonmember
     * non-GF present: 1
     * OBSS non-GF present: 1
     * dual beacon: 0
     * dual CTS protection: 0
     * STBC beacon: 0
     * L-SIG TXOP Prot: 0
     * PCO active: 0
     * PCO phase: 0
  Overlapping BSS scan params:
     * passive dwell: 20 TUs
     * active dwell: 10 TUs
     * channel width trigger scan interval: 300 s
     * scan passive total per channel: 200 TUs
     * scan active total per channel: 20 TUs
     * BSS width channel transition delay factor: 5
     * OBSS Scan Activity Threshold: 0.25 %
  Extended capabilities: HT Information Exchange Supported, Extended Channel Switching, BSS Transition, 6
  WMM:   * Parameter version 1
     * u-APSD
     * BE: CW 15-1023, AIFSN 3
     * BK: CW 15-1023, AIFSN 7
     * VI: CW 7-15, AIFSN 2, TXOP 3008 usec
     * VO: CW 3-7, AIFSN 2, TXOP 1504 usec
BSS 94:b4:0f:f1:35:61(on wcli0)
  TSF: 1739988134 usec (0d, 00:28:59)
  freq: 2462
  beacon interval: 100 TUs
  capability: ESS ShortPreamble SpectrumMgmt ShortSlotTime RadioMeasure (0x1521)
  signal: -38.00 dBm
  last seen: 1910 ms ago
  Information elements from Probe Response frame:
  SSID: GoogleGuest
  Supported rates: 36.0* 48.0 54.0
  DS Parameter set: channel 11
  Country: US Environment: Indoor/Outdoor
    Channels [1 - 11] @ 36 dBm
  Power constraint: 0 dB
  TPC report: TX power: 3 dBm
  ERP: <no flags>
  BSS Load:
     * station count: 1
     * channel utilisation: 49/255
     * available admission capacity: 26875 [*32us]
  HT capabilities:
    Capabilities: 0x19ad
      RX LDPC
      HT20
      SM Power Save disabled
      RX HT20 SGI
      TX STBC
      RX STBC 1-stream
      Max AMSDU length: 7935 bytes
      DSSS/CCK HT40
    Maximum RX AMPDU length 65535 bytes (exponent: 0x003)
    Minimum RX AMPDU time spacing: 4 usec (0x05)
    HT RX MCS rate indexes supported: 0-23
    HT TX MCS rate indexes are undefined
  HT operation:
     * primary channel: 11
     * secondary channel offset: no secondary
     * STA channel width: 20 MHz
     * RIFS: 1
     * HT protection: nonmember
     * non-GF present: 1
     * OBSS non-GF present: 1
     * dual beacon: 0
     * dual CTS protection: 0
     * STBC beacon: 0
     * L-SIG TXOP Prot: 0
     * PCO active: 0
     * PCO phase: 0
  Overlapping BSS scan params:
     * passive dwell: 20 TUs
     * active dwell: 10 TUs
     * channel width trigger scan interval: 300 s
     * scan passive total per channel: 200 TUs
     * scan active total per channel: 20 TUs
     * BSS width channel transition delay factor: 5
     * OBSS Scan Activity Threshold: 0.25 %
  Extended capabilities: HT Information Exchange Supported, Extended Channel Switching, BSS Transition, 6
  WMM:   * Parameter version 1
     * u-APSD
     * BE: CW 15-1023, AIFSN 3
     * BK: CW 15-1023, AIFSN 7
     * VI: CW 7-15, AIFSN 2, TXOP 3008 usec
     * VO: CW 3-7, AIFSN 2, TXOP 1504 usec
BSS 94:b4:0f:f1:3a:e0(on wcli0)
  TSF: 24578560051 usec (0d, 06:49:38)
  freq: 2437
  beacon interval: 100 TUs
  capability: ESS Privacy ShortPreamble SpectrumMgmt ShortSlotTime RadioMeasure (0x1531)
  signal: -55.00 dBm
  last seen: 2310 ms ago
  Information elements from Probe Response frame:
  SSID: Google
  Supported rates: 36.0* 48.0 54.0
  DS Parameter set: channel 6
  TIM: DTIM Count 0 DTIM Period 1 Bitmap Control 0x0 Bitmap[0] 0x0
  Country: US Environment: Indoor/Outdoor
    Channels [1 - 11] @ 36 dBm
  Power constraint: 0 dB
  TPC report: TX power: 3 dBm
  ERP: <no flags>
  RSN:   * Version: 1
     * Group cipher: CCMP
     * Pairwise ciphers: CCMP
     * Authentication suites: IEEE 802.1X
     * Capabilities: 4-PTKSA-RC 4-GTKSA-RC (0x0028)
  BSS Load:
     * station count: 1
     * channel utilisation: 21/255
     * available admission capacity: 28125 [*32us]
  HT capabilities:
    Capabilities: 0x19ad
      RX LDPC
      HT20
      SM Power Save disabled
      RX HT20 SGI
      TX STBC
      RX STBC 1-stream
      Max AMSDU length: 7935 bytes
      DSSS/CCK HT40
    Maximum RX AMPDU length 65535 bytes (exponent: 0x003)
    Minimum RX AMPDU time spacing: 4 usec (0x05)
    HT RX MCS rate indexes supported: 0-23
    HT TX MCS rate indexes are undefined
  HT operation:
     * primary channel: 6
     * secondary channel offset: no secondary
     * STA channel width: 20 MHz
     * RIFS: 1
     * HT protection: nonmember
     * non-GF present: 1
     * OBSS non-GF present: 1
     * dual beacon: 0
     * dual CTS protection: 0
     * STBC beacon: 0
     * L-SIG TXOP Prot: 0
     * PCO active: 0
     * PCO phase: 0
  Overlapping BSS scan params:
     * passive dwell: 20 TUs
     * active dwell: 10 TUs
     * channel width trigger scan interval: 300 s
     * scan passive total per channel: 200 TUs
     * scan active total per channel: 20 TUs
     * BSS width channel transition delay factor: 5
     * OBSS Scan Activity Threshold: 0.25 %
  Extended capabilities: HT Information Exchange Supported, Extended Channel Switching, BSS Transition, 6
  WMM:   * Parameter version 1
     * u-APSD
     * BE: CW 15-1023, AIFSN 3
     * BK: CW 15-1023, AIFSN 7
     * VI: CW 7-15, AIFSN 2, TXOP 3008 usec
     * VO: CW 3-7, AIFSN 2, TXOP 1504 usec
BSS 94:b4:0f:f1:3a:e1(on wcli0)
  TSF: 24578576547 usec (0d, 06:49:38)
  freq: 2437
  beacon interval: 100 TUs
  capability: ESS ShortPreamble SpectrumMgmt ShortSlotTime RadioMeasure (0x1521)
  signal: -65.00 dBm
  last seen: 80 ms ago
  Information elements from Probe Response frame:
  SSID: GoogleGuest
  Supported rates: 36.0* 48.0 54.0
  DS Parameter set: channel 6
  Country: US Environment: Indoor/Outdoor
    Channels [1 - 11] @ 36 dBm
  Power constraint: 0 dB
  TPC report: TX power: 3 dBm
  ERP: <no flags>
  BSS Load:
     * station count: 2
     * channel utilisation: 21/255
     * available admission capacity: 28125 [*32us]
  HT capabilities:
    Capabilities: 0x19ad
      RX LDPC
      HT20
      SM Power Save disabled
      RX HT20 SGI
      TX STBC
      RX STBC 1-stream
      Max AMSDU length: 7935 bytes
      DSSS/CCK HT40
    Maximum RX AMPDU length 65535 bytes (exponent: 0x003)
    Minimum RX AMPDU time spacing: 4 usec (0x05)
    HT RX MCS rate indexes supported: 0-23
    HT TX MCS rate indexes are undefined
  HT operation:
     * primary channel: 6
     * secondary channel offset: no secondary
     * STA channel width: 20 MHz
     * RIFS: 1
     * HT protection: nonmember
     * non-GF present: 1
     * OBSS non-GF present: 1
     * dual beacon: 0
     * dual CTS protection: 0
     * STBC beacon: 0
     * L-SIG TXOP Prot: 0
     * PCO active: 0
     * PCO phase: 0
  Overlapping BSS scan params:
     * passive dwell: 20 TUs
     * active dwell: 10 TUs
     * channel width trigger scan interval: 300 s
     * scan passive total per channel: 200 TUs
     * scan active total per channel: 20 TUs
     * BSS width channel transition delay factor: 5
     * OBSS Scan Activity Threshold: 0.25 %
  Extended capabilities: HT Information Exchange Supported, Extended Channel Switching, BSS Transition, 6
  WMM:   * Parameter version 1
     * u-APSD
     * BE: CW 15-1023, AIFSN 3
     * BK: CW 15-1023, AIFSN 7
     * VI: CW 7-15, AIFSN 2, TXOP 3008 usec
     * VO: CW 3-7, AIFSN 2, TXOP 1504 usec

BSS 94:b4:0f:f1:36:41(on wcli0)
  TSF: 12499149351 usec (0d, 03:28:19)
  freq: 2437
  beacon interval: 100 TUs
  capability: ESS ShortPreamble SpectrumMgmt ShortSlotTime RadioMeasure (0x1521)
  signal: -67.00 dBm
  last seen: 80 ms ago
  Information elements from Probe Response frame:
  SSID: GoogleGuest
  Supported rates: 36.0* 48.0 54.0
  DS Parameter set: channel 6
  Country: US Environment: Indoor/Outdoor
    Channels [1 - 11] @ 36 dBm
  Power constraint: 0 dB
  TPC report: TX power: 3 dBm
  ERP: <no flags>
  BSS Load:
     * station count: 1
     * channel utilisation: 28/255
     * available admission capacity: 27500 [*32us]
  HT capabilities:
    Capabilities: 0x19ad
      RX LDPC
      HT20
      SM Power Save disabled
      RX HT20 SGI
      TX STBC
      RX STBC 1-stream
      Max AMSDU length: 7935 bytes
      DSSS/CCK HT40
    Maximum RX AMPDU length 65535 bytes (exponent: 0x003)
    Minimum RX AMPDU time spacing: 4 usec (0x05)
    HT RX MCS rate indexes supported: 0-23
    HT TX MCS rate indexes are undefined
  HT operation:
     * primary channel: 6
     * secondary channel offset: no secondary
     * STA channel width: 20 MHz
     * RIFS: 1
     * HT protection: nonmember
     * non-GF present: 1
     * OBSS non-GF present: 1
     * dual beacon: 0
     * dual CTS protection: 0
     * STBC beacon: 0
     * L-SIG TXOP Prot: 0
     * PCO active: 0
     * PCO phase: 0
  Overlapping BSS scan params:
     * passive dwell: 20 TUs
     * active dwell: 10 TUs
     * channel width trigger scan interval: 300 s
     * scan passive total per channel: 200 TUs
     * scan active total per channel: 20 TUs
     * BSS width channel transition delay factor: 5
     * OBSS Scan Activity Threshold: 0.25 %
  Extended capabilities: HT Information Exchange Supported, Extended Channel Switching, BSS Transition, 6
  WMM:   * Parameter version 1
     * u-APSD
     * BE: CW 15-1023, AIFSN 3
     * BK: CW 15-1023, AIFSN 7
     * VI: CW 7-15, AIFSN 2, TXOP 3008 usec
     * VO: CW 3-7, AIFSN 2, TXOP 1504 usec
BSS 94:b4:0f:f1:36:40(on wcli0)
  TSF: 12499150000 usec (0d, 03:28:19)
  freq: 2437
  beacon interval: 100 TUs
  capability: ESS Privacy ShortPreamble SpectrumMgmt ShortSlotTime RadioMeasure (0x1531)
  signal: -66.00 dBm
  last seen: 2350 ms ago
  Information elements from Probe Response frame:
  SSID: Google
  Supported rates: 36.0* 48.0 54.0
  DS Parameter set: channel 6
  TIM: DTIM Count 0 DTIM Period 1 Bitmap Control 0x0 Bitmap[0] 0x0
  Country: US Environment: Indoor/Outdoor
    Channels [1 - 11] @ 36 dBm
  Power constraint: 0 dB
  TPC report: TX power: 3 dBm
  ERP: <no flags>
  RSN:   * Version: 1
     * Group cipher: CCMP
     * Pairwise ciphers: CCMP
     * Authentication suites: IEEE 802.1X
     * Capabilities: 4-PTKSA-RC 4-GTKSA-RC (0x0028)
  BSS Load:
     * station count: 0
     * channel utilisation: 28/255
     * available admission capacity: 27500 [*32us]
  HT capabilities:
    Capabilities: 0x19ad
      RX LDPC
      HT20
      SM Power Save disabled
      RX HT20 SGI
      TX STBC
      RX STBC 1-stream
      Max AMSDU length: 7935 bytes
      DSSS/CCK HT40
    Maximum RX AMPDU length 65535 bytes (exponent: 0x003)
    Minimum RX AMPDU time spacing: 4 usec (0x05)
    HT RX MCS rate indexes supported: 0-23
    HT TX MCS rate indexes are undefined
  HT operation:
     * primary channel: 6
     * secondary channel offset: no secondary
     * STA channel width: 20 MHz
     * RIFS: 1
     * HT protection: nonmember
     * non-GF present: 1
     * OBSS non-GF present: 1
     * dual beacon: 0
     * dual CTS protection: 0
     * STBC beacon: 0
     * L-SIG TXOP Prot: 0
     * PCO active: 0
     * PCO phase: 0
  Overlapping BSS scan params:
     * passive dwell: 20 TUs
     * active dwell: 10 TUs
     * channel width trigger scan interval: 300 s
     * scan passive total per channel: 200 TUs
     * scan active total per channel: 20 TUs
     * BSS width channel transition delay factor: 5
     * OBSS Scan Activity Threshold: 0.25 %
  Extended capabilities: HT Information Exchange Supported, Extended Channel Switching, BSS Transition, 6
  WMM:   * Parameter version 1
     * u-APSD
     * BE: CW 15-1023, AIFSN 3
     * BK: CW 15-1023, AIFSN 7
     * VI: CW 7-15, AIFSN 2, TXOP 3008 usec
     * VO: CW 3-7, AIFSN 2, TXOP 1504 usec
"""


# pylint: disable=unused-argument,protected-access
def fake_scan(*args, **kwargs):
  return SCAN_OUTPUT
iw._scan = fake_scan


@wvtest.wvtest
def find_bssids_test():
  """Test iw.find_bssids."""
  short_scan_result = iw.BssInfo(ssid='short scan result',
                                 bssid='00:23:97:57:f4:d8',
                                 security=['WEP'],
                                 vendor_ies=[('00:11:22', '01 23 45 67')])

  with_ie, without_ie = iw.find_bssids('wcli0', lambda o, d: o == '00:11:22',
                                       True)

  wvtest.WVPASSEQ(with_ie, set([short_scan_result]))

  wvtest.WVPASSEQ(
      without_ie,
      set([iw.BssInfo(ssid='GoogleGuest', bssid='94:b4:0f:f1:36:41'),
           iw.BssInfo(ssid='GoogleGuest', bssid='94:b4:0f:f1:3a:e1'),
           iw.BssInfo(ssid='GoogleGuest', bssid='94:b4:0f:f1:35:61'),
           iw.BssInfo(ssid='Google', bssid='94:b4:0f:f1:36:40',
                      security=['WPA2']),
           iw.BssInfo(ssid='Google', bssid='94:b4:0f:f1:3a:e0',
                      security=['WPA2']),
           iw.BssInfo(ssid='Google', bssid='94:b4:0f:f1:35:60',
                      security=['WPA2']),
           iw.BssInfo(ssid='Google', bssid='94:b4:0f:f1:02:a0',
                      security=['WPA2'])]))

  with_ie, without_ie = iw.find_bssids('wcli0', lambda o, d: o == '00:11:22',
                                       False)
  wvtest.WVPASSEQ(with_ie, set())
  wvtest.WVPASSEQ(
      without_ie,
      set([iw.BssInfo(ssid='GoogleGuest', bssid='94:b4:0f:f1:36:41'),
           iw.BssInfo(ssid='GoogleGuest', bssid='94:b4:0f:f1:3a:e1'),
           iw.BssInfo(ssid='GoogleGuest', bssid='94:b4:0f:f1:35:61')]))

if __name__ == '__main__':
  wvtest.wvtest_main()
