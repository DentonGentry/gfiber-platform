#!/usr/bin/python -S

"""Special handling for mwifiex driver.

   The mwifiex driver has a mechanism to detect when the firmware is
   stuck and attempt a reset. It is disabled by default and can be
   configured with this helper.
"""
import glob
import utils

_MWIFIEX_SYS_RECOVERY = '/sys/kernel/debug/mwifiex/*/firmware_recover'


def set_recovery(recovery_flag):
  # Marvell's firmware can get into a non-recovery state
  # that hangs forever. The driver can detect this state and perform a
  # a reset when enabled with a debug flag, which is done here when the
  # corresponding experiment is active.
  recovery_flag = int(recovery_flag)
  for sys_path in glob.glob(_MWIFIEX_SYS_RECOVERY):
    utils.log('mwifiex.py: set %r to %r.' % (sys_path, recovery_flag))
    open(sys_path, 'w').write(str(recovery_flag))
