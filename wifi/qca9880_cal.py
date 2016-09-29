#!/usr/bin/python -S

"""Check and fix mis-calibrated QCA9880 modules on gfrg200/gfrg210.

   Some modules were delivered to customers mis-calibrated. This script will
   check if the module is affected, and if so, generate a patch that will be
   used after driver reload.
"""
import glob
import os
import os.path
import experiment
import utils

NO_CAL_EXPERIMENT = 'WifiNoCalibrationPatch'
PLATFORM_FILE = '/etc/platform'
CALIBRATION_DIR = '/tmp/ath10k_cal'
CAL_PATCH_FILE = 'cal_data_patch.bin'
ATH10K_CAL_DATA = '/sys/kernel/debug/ieee80211/phy[0-9]*/ath10k/cal_data'
OUI_OFFSET = 6
OUI_LEN = 3
VERSION_OFFSET = 45
VERSION_LEN = 3
SUSPECT_OUIS = ((0x28, 0x24, 0xff), (0x48, 0xa9, 0xd2), (0x60, 0x02, 0xb4),
                (0xbc, 0x30, 0x7d), (0xbc, 0x30, 0x7e))
MISCALIBRATED_VERSION_FIELD = (0x0, 0x0, 0x0)
MODULE_PATH = '/sys/class/net/{}/device/driver/module'


def _log(msg):
  utils.log('ath10k calibration: {}'.format(msg))


def _is_ath10k(interface):
  """Check if interface is driven by the ath10k driver.

  Args:
    interface: The interface to be checked. eg wlan1

  Returns:
    True if ath10k, otherwise False.
  """
  try:
    return os.readlink(MODULE_PATH.format(interface)).find('ath10k')
  except OSError:
    return False


def _oui_string(oui):
  """Convert OUI from bytes to a string.

  Args:
    oui: OUI in byte format.

  Returns:
    OUI is string format separated by ':'. Eg. 88:dc:96.
  """
  return ':'.join('{:02x}'.format(ord(b)) for b in oui)


def _version_string(version):
  """Convert version from bytes to a string.

  Args:
    version: version in byte format.

  Returns:
    Three byte version string in hex format: 0x00 0x00 0x00
  """

  return ' '.join('0x{:02x}'.format(ord(b)) for b in version)


def _is_module_miscalibrated():
  """Check the QCA8990 module to see if it is improperly calibrated.

  There are two manufacturers of the modules, Senao and Wistron of which only
  Wistron modules are suspect. Wistron provided a list of OUIs manufactured
  which are listed in SUSPECT_OUIS. Modules manufactured by Winstron containing
  V02 at offset VERSION_OFFSET have been corrected, while those containing 3
  zero's at this offset are still suspect and will be considered mis-calibrated.

  Returns:
    True if module is mis-calibrated, None if it can't be determined, and False
    otherwise.
  """

  try:
    cal_data_path = _ath10k_cal_data_path()
    if cal_data_path is None:
      return None

    with open(cal_data_path, mode='rb') as f:
      f.seek(OUI_OFFSET)
      oui = f.read(OUI_LEN)
      f.seek(VERSION_OFFSET)
      version = f.read(VERSION_LEN)

  except IOError as e:
    _log('unable to open cal_data {}: {}'.format(cal_data_path, e.strerror))
    return None

  if oui not in (bytearray(s) for s in SUSPECT_OUIS):
    _log('OUI {} is properly calibrated.'.format(_oui_string(oui)))
    return False

  if version != (bytearray(MISCALIBRATED_VERSION_FIELD)):
    _log('version field {} signals proper calibration.'.
         format(_version_string(version)))
    return False

  _log('May be mis-calibrated. OUI: {} version: {}'.
       format(_oui_string(oui), _version_string(version)))

  return True


def _is_previously_calibrated():
  """Check if this calibration script already ran since the last boot.

  Returns:
    True if calibration checks already ran, False otherwise.
  """
  return os.path.exists(CALIBRATION_DIR)


def _create_calibration_dir():
  """Create calibration directory.

  Calibration directory contains the calibration patch file.
  If the directory is empty it signals that calibration checks have already
  run.

  Returns:
    True if directory exists or is created, false if any error.
  """
  try:
    if not os.path.isdir(CALIBRATION_DIR):
      os.makedirs(CALIBRATION_DIR)
      return True
  except OSError as e:
    _log('unable to create calibration dir {}: {}.'.
         format(CALIBRATION_DIR, e.strerror))
    return False

  return True


def _ath10k_cal_data_path():
  """Find the current path to cal data.

  This path encodes the phy number, which is usually phy1, but if the
  driver load order changed or if this runs after a reload, the phy
  number will change.

  Returns:
    Path to cal_data in debugfs.
  """

  return glob.glob(ATH10K_CAL_DATA)[0]


def _generate_calibration_patch():
  """Create calibration patch and write to storage.

  Returns:
    True for success or False for failure.
  """
  try:
    with open(_ath10k_cal_data_path(), mode='rb') as f:
      cal_data = bytearray(f.read())
  except IOError as e:
    _log('cal patch: unable to open for read {}: {}.'.
         format(_ath10k_cal_data_path(), e.strerror))
    return False

  # Patch cal_data here once we get the actual calibration data.
  # For now just return False until we get the data.
  _log('patch not generated as data not supplied yet.')
  # pylint: disable=unreachable
  return False

  if not _create_calibration_dir():
    return False

  try:
    patched_file = os.path.join(CALIBRATION_DIR, CAL_PATCH_FILE)
    open(patched_file, 'wb').write(cal_data)
  except IOError as e:
    _log('unable to open for writing {}: {}.'.format(patched_file, e.strerror))
    return False

  return True


def _reload_driver():
  """Reload the ath10k driver so it picks up modified calibration file."""
  ret = utils.subprocess_quiet(('rmmod', 'ath10k_pci'))
  if ret != 0:
    _log('rmmod ath10k_pci failed: {}.'.format(ret))
    return

  ret = utils.subprocess_quiet(('modprobe', 'ath10k_pci'))
  if ret != 0:
    _log('modprobe ath10k_pci failed: {}.'.format(ret))
    return

  _log('reload ath10k driver complete')


def qca8990_calibration():
  """Main QCA8990 calibration check."""

  if experiment.enabled(NO_CAL_EXPERIMENT):
    _log('experiment {} on. Skip calibration check.'.format(NO_CAL_EXPERIMENT))
    return

  if _is_previously_calibrated():
    _log('calibration check completed earlier.')
    return

  if not _is_ath10k('wlan1'):
    _log('this platform does not use ath10k.')
    return

  cal_result = _is_module_miscalibrated()
  if cal_result is None:
    _log('unknown if miscalibrated.')
  elif not cal_result:
    _log('module is NOT miscalibrated.')
    # Creating an empty directory signals that this script has already run.
    _create_calibration_dir()
  else:
    if _generate_calibration_patch():
      _log('generated new patch.')
      _reload_driver()


if __name__ == '__main__':
  qca8990_calibration()
