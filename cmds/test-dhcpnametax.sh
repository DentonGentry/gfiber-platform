#!/bin/bash
. ./wvtest/wvtest.sh

pid=$$
TAX=./host-dhcpnametax

WVSTART "dhcpnametax test"

WVPASSEQ "$($TAX -l label -d 1,3,6,12,15,28,42 -h Hopper_ETH0)" "name label DISH Networks Hopper;Hopper"
WVPASSEQ "$($TAX -l label -d 3,1,252,42,15,6,12 -h steamlink)" "name label Steam Link;Steam Link"

# Test serial numbers with special format handling
WVPASSEQ "$($TAX -l label -d 1,3,6,15,12 -h NP-1G0123456789)" "name label Roku;Roku 3 4200X"
WVPASSEQ "$($TAX -l label -d 1,3,6,15,12 -h NP-120123456789)" "name label Roku;Roku 2 XD 3050"
WVPASSEQ "$($TAX -l label -d 1,28,2,3,15,6,12 -h TIVO-848XXXXXXXXXXXX)" "name label TiVo;TiVo Roamio Plus"
WVPASSEQ "$($TAX -l label -d 1,28,2,3,15,6,12 -h TIVO-849XXXXXXXXXXXX)" "name label TiVo;TiVo BOLT"
WVPASSEQ "$($TAX -l label -d 3,1,252,42,15,6,12 -h 01AA01AB23456789)" "name label Nest Thermostat;Nest Thermostat v1"
WVPASSEQ "$($TAX -l label -d 3,1,252,42,15,6,12 -h 09AA01AB23456789)" "name label Nest Thermostat;Nest Thermostat v3"
WVPASSEQ "$($TAX -l label -d 1,3,6,12,15,28,42 -h XL824-XXXXXXX)" "name label Trane Thermostat;XL824"
WVPASSEQ "$($TAX -l label -d 1,3,6,12,15,28,40,41,42 -h DIRECTV-H21-01234567)" "name label DirecTV;H21"
WVPASSEQ "$($TAX -l label -d 1,3,6,12,15,28,42 -h DIRECTV-HR22-01234567)" "name label DirecTV;HR22"
WVPASSEQ "$($TAX -l label -d 1,28,2,3,15,6,119,12,44,47,26,121,42 -h 500-cc04b40XXXXX)" "name label Select Comfort SleepIQ;SleepIQ"

# check invalid or missing arguments.
WVFAIL $TAX
WVFAIL $TAX -m mac
WVFAIL $TAX -h hostname
WVFAIL $TAX -d dhcpsig

rm -f *.$pid.tmp
