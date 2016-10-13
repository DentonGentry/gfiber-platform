#!/usr/bin/python

"""Fake wpa_cli implementation.  Used by fake WPACtrl too."""

import ifdown
import ifup


_INTERFACE_STATE = {}


def call(*args, **unused_kwargs):
  if 'status' not in args:
    raise ValueError('Fake wpa_cli can only do status requests.')

  if '-i' not in args:
    raise ValueError('Must specify interface with -i.')

  interface = args[args.index('-i') + 1]

  # Fails for not present or empty dict.
  if not _INTERFACE_STATE.get(interface, None):
    return 1, ('Failed to connect to non-global ctrl_ifname: %r  '
               'error: No such file or directory' % interface)

  state = _INTERFACE_STATE[interface]

  return 0, '\n'.join('%s=%s' % (k, v) for k, v in state.iteritems())


# Pass no kwargs to "kill" wpa_supplicant.
def mock(interface, **kwargs):
  _INTERFACE_STATE[interface] = {k: v for k, v in kwargs.iteritems() if v}
  if kwargs:
    ifup.call(interface)
  else:
    ifdown.call(interface)
