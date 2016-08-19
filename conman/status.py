#!/usr/bin/python

"""Tracks and exports conman status information for e.g. ledmonitor."""

# This may seem over-engineered, but conman has enough loosely-coupled moving
# parts that it is worth being able to reason formally and separately about the
# state of the system.  Otherwise it would be very easy for new conman code to
# create subtle bugs in e.g. LED behavior.

import inspect
import logging
import os


class P(object):
  """Enumerate propositions about conman status.

  Using class attributes rather than just strings will help prevent typos.
  """

  TRYING_OPEN = 'TRYING_OPEN'
  TRYING_WLAN = 'TRYING_WLAN'
  WLAN_FAILED = 'WLAN_FAILED'
  CONNECTED_TO_OPEN = 'CONNECTED_TO_OPEN'
  CONNECTED_TO_WLAN = 'CONNECTED_TO_WLAN'
  HAVE_CONFIG = 'HAVE_CONFIG'
  HAVE_WORKING_CONFIG = 'HAVE_WORKING_CONFIG'
  CAN_REACH_ACS = 'CAN_REACH_ACS'
  # Were we able to connect to the ACS last time we expected to be able to?
  COULD_REACH_ACS = 'COULD_REACH_ACS'
  CAN_REACH_INTERNET = 'CAN_REACH_INTERNET'
  PROVISIONING_FAILED = 'PROVISIONING_FAILED'
  ATTACHED_TO_WPA_SUPPLICANT = 'ATTACHED_TO_WPA_SUPPLICANT'


# Format:  { proposition: (implications, counter-implications), ... }
# If you want to add a new proposition to the Status class, just edit this dict.
IMPLICATIONS = {
    P.TRYING_OPEN: (
        (),
        (P.CONNECTED_TO_OPEN, P.TRYING_WLAN, P.CONNECTED_TO_WLAN)
    ),
    P.TRYING_WLAN: (
        (),
        (P.TRYING_OPEN, P.CONNECTED_TO_OPEN, P.CONNECTED_TO_WLAN)
    ),
    P.WLAN_FAILED: (
        (),
        (P.TRYING_WLAN, P.CONNECTED_TO_WLAN)
    ),
    P.CONNECTED_TO_OPEN: (
        (),
        (P.CONNECTED_TO_WLAN, P.TRYING_OPEN, P.TRYING_WLAN)
    ),
    P.CONNECTED_TO_WLAN: (
        (P.HAVE_WORKING_CONFIG,),
        (P.CONNECTED_TO_OPEN, P.TRYING_OPEN, P.TRYING_WLAN)
    ),
    P.CAN_REACH_ACS: (
        (P.COULD_REACH_ACS,),
        (P.TRYING_OPEN, P.TRYING_WLAN)
    ),
    P.COULD_REACH_ACS: (
        (),
        (P.PROVISIONING_FAILED,),
    ),
    P.PROVISIONING_FAILED: (
        (),
        (P.COULD_REACH_ACS,),
    ),
    P.HAVE_WORKING_CONFIG: (
        (P.HAVE_CONFIG,),
        (),
    ),
    P.ATTACHED_TO_WPA_SUPPLICANT: (
        (),
        (),
    )
}


class Proposition(object):
  """Represents a proposition.

  May imply truth or falsity of other propositions.
  """

  def __init__(self, name, export_path):
    self._name = name
    self._export_path = export_path
    self._value = None
    self._implications = set()
    self._counter_implications = set()
    self._impliers = set()
    self._counter_impliers = set()

  def implies(self, implication):
    self._counter_implications.discard(implication)
    self._implications.add(implication)
    # pylint: disable=protected-access
    implication._implied_by(self)

  def implies_not(self, counter_implication):
    self._implications.discard(counter_implication)
    self._counter_implications.add(counter_implication)
    # pylint: disable=protected-access
    counter_implication._counter_implied_by(self)

  def _implied_by(self, implier):
    self._counter_impliers.discard(implier)
    self._impliers.add(implier)

  def _counter_implied_by(self, counter_implier):
    self._impliers.discard(counter_implier)
    self._counter_impliers.add(counter_implier)

  def set(self, value):
    if value == self._value:
      return

    self._value = value
    self.export()
    logging.debug('%s is now %s', self._name, self._value)

    if value:
      for implication in self._implications:
        implication.set(True)
      for counter_implication in self._counter_implications:
        counter_implication.set(False)
      # Contrapositive:  (A -> ~B) -> (B -> ~A)
      for counter_implier in self._counter_impliers:
        counter_implier.set(False)
    # Contrapositive:  (A -> B) -> (~B -> ~A)
    else:
      for implier in self._impliers:
        implier.set(False)

  def export(self):
    filepath = os.path.join(self._export_path, self._name)
    if self._value:
      if not os.path.exists(filepath):
        open(filepath, 'w')
    else:
      if os.path.exists(filepath):
        os.unlink(filepath)


class Status(object):
  """Provides a convenient API for conman to describe system status."""

  def __init__(self, export_path):
    if not os.path.isdir(export_path):
      os.makedirs(export_path)

    self._export_path = export_path

    self._propositions = {
        p: Proposition(p, self._export_path)
        for p in dict(inspect.getmembers(P)) if not p.startswith('_')
    }

    for p, (implications, counter_implications) in IMPLICATIONS.iteritems():
      for implication in implications:
        self._propositions[p].implies(self._propositions[implication])
      for counter_implication in counter_implications:
        self._propositions[p].implies_not(
            self._propositions[counter_implication])

  def _proposition(self, p):
    return self._propositions[p]

  def __setattr__(self, attr, value):
    """Allow setting of propositions with attributes.

    If _propositions contains an attribute 'FOO', then `Status().foo = True`
    will set that Proposition to True.  This means that this class doesn't have
    to be changed when IMPLICATIONS is updated.

    Args:
      attr:  The attribute name.
      value:  The attribute value.
    """
    if hasattr(self, '_propositions') and not hasattr(self, attr):
      if attr.islower():
        if attr.upper() in self._propositions:
          self._propositions[attr.upper()].set(value)
          return

    super(Status, self).__setattr__(attr, value)
