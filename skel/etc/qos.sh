#!/bin/sh

set -e
set -x

# File for QOS rules

GIGE=eth0
MOCA=eth1
WIFI=eth2
ALL_POSSIBLE_IFS="$GIGE $MOCA $WIFI"

set +e # ignore bad ifconfig, we're checking it
for if in $ALL_POSSIBLE_IFS; do
  ifconfig $if 2>&1 > /dev/null
  if [ $? == 0 ]; then
    ALL_IFS="$ALL_IFS $if"
  fi
done
set -e # back to error checking

iptables -F
ebtables -F

iptables -t mangle -F
ebtables -t nat -F

VLAN_GOOG=3
VLAN_GUEST=4
VLAN_MEDIA=100
VLAN_DATA=0
VLANS="$VLAN_GOOG $VLAN_GUEST $VLAN_MEDIA"

VLAN_IFS=""
for vlan in $VLANS; do
  brctl addbr br$vlan
  for rawif in $ALL_IFS; do
  set +e
  ip link delete dev $rawif.$vlan
  set -e
  ip link add link $rawif name $rawif.$vlan type vlan id $vlan;
  brctl addif br$vlan $rawif.$vlan
  VLAN_IFS="$VLAN_IFS $rawif.$vlan"
  done
done

ebtables -t nat -A PREROUTING -i $WIFI --mark-set 8
ebtables -t nat -A PREROUTING -i $GIGE --mark-set 8
ebtables -t nat -A PREROUTING -i $MOCA --mark-set 8

ebtables -t nat -A PREROUTING -i $WIFI.$VLAN_GUEST --mark-set 7

# ebtables -t nat -A PREROUTING --vlan-id $VLAN_GUEST --mark-set 7

iptables -t mangle -A OUTPUT -m owner --uid-owner video -j DSCP --set-dscp-class CS5
iptables -t mangle -A OUTPUT -m owner --uid-owner video -j MARK --set-mark 3

for if in $VLAN_IFS; do
  tc qdisc add dev $if root handle 1: prio bands 8
  for id in $(seq 1 8); do
    mark=$id
    handle=$id
    tc filter add dev $if parent 1:0 protocol ip prio $(expr $id - 1) handle $mark fw classid 1:$handle
  done
done

for mark in $(seq 1 7); do
  CS=$(expr 8 - $mark)
  iptables -t mangle -A POSTROUTING -m dscp --dscp-class CS$CS -j MARK --set-mark $mark
  iptables -t mangle -A POSTROUTING -m dscp --dscp-class CS$CS -j RETURN
done
iptables -t mangle -A POSTROUTING -j MARK --set-mark 8
iptables -t mangle -A POSTROUTING -j RETURN

for vlan in $VLANS; do
  for mark in $(seq 1 7); do
    VLAN_PRIO=$(expr 8 - $mark)
    # nat/PREREOUTING is earliest in ebtables path
    ebtables -t nat -A POSTROUTING -p 802_1Q --vlan-id $vlan --vlan-prio $VLAN_PRIO -j mark --set-mark $mark --mark-target CONTINUE
  done
done
