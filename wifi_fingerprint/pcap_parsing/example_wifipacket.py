#!/usr/bin/python2
"""Examples of how to use wifipacket.py."""

import wifipacket
import sys
import csv

__author__ = 'shantanuj@google.com (Shantanu Jain)'


def BasicPrinter(p):
  basetime = 0
  for opt, frame in wifipacket.Packetize(p):
    ts = opt.pcap_secs
    if basetime:
      ts -= basetime
    else:
      basetime = ts
      ts = 0
    print (ts, opt)


def BasicCSVWriter(p):
  want_fields = [
      'ta',
      'ra',
      'seq',
      'mcs',
      'rate',
      'retry',
      'dbm_antsignal',
      'dbm_antnoise',
      'typestr',
  ]
  co = csv.writer(sys.stdout)
  co.writerow(['pcap_secs'] + want_fields)
  tbase_pcap = 0
  tbase_mac = 0
  for opt, frame in wifipacket.Packetize(p):
    t_pcap = opt.get('pcap_secs', 0)
    if not tbase_pcap: tbase_pcap = t_pcap
    co.writerow(['%.6f' % (t_pcap - tbase_pcap)] +
                [opt.get(f, None) for f in want_fields])


def PCAPToStdOut(p):
  wifipacket.StdOut(wifipacket.ZOpen(p))


def InterfaceToStdOut(p):
  wifipacket.StdOut(p)


if __name__ == '__main__':
  PCAPToStdOut(sys.argv[1])
  # BasicPrinter(sys.argv[1])
