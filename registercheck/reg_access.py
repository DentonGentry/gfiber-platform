#!/usr/bin/python
#
# Copyright 2011 Google Inc. All Rights Reserved.
#
# filename: reg_access.py - reference lsusb.py
# Usage: See ParseArgs()

import os
import sys
import re
import subprocess
from optparse import OptionParser

# from __future__ import print_function

#
# Global constants
#
MDIO_CMD_REG = 0x10b80e14      # 0x10ba0e14 - MDIO CMD reg addr for A1

BRUNO_REG_LIST_FILE    = "bruno_7425_reg_list.txt"
BRUNO_REG_RESULT_LIST_FILE = "bruno_reg_result.dat"

# Definition of elements for register list members
BRCM_GENET_PHY_REGS = "GenetPhy"       # Read GENET phy register via RGMII
BRCM_7425_REGS = "7425Regs"            # Read 7425 registers


# Definition of index for register list members
REG_ADDR_IDX     = 0
REG_EXP_DATA_IDX = 1
REG_MASK_IDX     = 2
# Definition of elements for register list members
REG_GOT_EXP_DATA_ELEM = 2     # Include the element of expected data
REG_GOT_MASK_ELEM     = 3     # Include the element of register mask off value
REG_MAX_ELEMS         = 3     # The max support elements per entry

# devmem bus error message
DEVMEM_ERR_MSG = "Bus error"

#
# Global options
#
show_dbg_msg     = False    # True  - turn on debugging msg
list_all_reg_msg = False    # False - print only mismatched reg data
                            # True  - print all reg data

reg_list_file   = BRUNO_REG_LIST_FILE
reg_result_file = BRUNO_REG_RESULT_LIST_FILE

class DevmemException(Exception):
  "Raised for devmem errors."
  pass

def dbgprint(msg=''):
  "Print debugging message routine"
  if (show_dbg_msg) and (msg):
      print >> sys.stderr, msg


def rd_7425_reg(reg_addr):
  """"
  Read 7425 register
  Output:
  If devmem OK, return int
  If defmem get "Data bus error...", return it error string to caller
  """
  p = subprocess.Popen('devmem 0x%08x' % reg_addr, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  for line in p.stdout.readlines():
    line = line.strip('\n')
    if DEVMEM_ERR_MSG in line:
      raise DevmemException(line)
      break
    else:
      dbgprint('<reg>: 0x%08x <reg val>: %s' % (reg_addr, line))
      line = int(line, 0)
  retval = p.wait()
  return line


def wr_7425_reg(reg_addr, reg_val):
  "Write 7425 register"
  cmd_str = "devmem 0x%08x 32 0x%08x" % (reg_addr, reg_val)
  dbgprint('wr_7425_reg(): <cmd_str>: %s' % cmd_str)

  p = subprocess.Popen(cmd_str, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  for line in p.stdout.readlines():
    if DEVMEM_ERR_MSG in line:
      dbgprint(line)
      break
    dbgprint('<devmem rtn>: %s', line),

  retval = p.wait()
  try:
    value = rd_7425_reg(reg_addr)

  except DevmemException, inst:
    dbgprint('wr_7425(): %s' % inst)
  else:
    dbgprint('wr_7425(): val: 0x%08x' % value)


def rd_genet_ext_phy_74612_reg(phy_reg_addr):
  "Read GENET external Phy (74612) register via MDIO command register"
  reg_val = 0x28000000 | ((phy_reg_addr & 0x000000FF) << 16)
  dbgprint('rd_genet_ext_phy_74612_reg: 0x%08x' % reg_val)

  try:
    wr_7425_reg(MDIO_CMD_REG, reg_val)
    reg_val = rd_7425_reg(MDIO_CMD_REG)

  except DevmemException, inst:
    return inst                   # return string
  else:
    return (reg_val & 0x0000ffff) # return int


def wr_genet_ext_phy_74612_reg(phy_reg_addr, phy_reg_val):
  "Write to GENET external Phy (74612) register via MDIO command register"
  reg_val = 0x24000000 | ((phy_reg_addr & 0x000000FF) << 16) | (phy_reg_val & 0x0ffff)
  dbgprint('wr_phy_74612() <reg val> 0x%08x' % reg_val)

  wr_7425_reg(MDIO_CMD_REG, reg_val)


def rd_cmp_register(reg_info):
  """
  read the specified 7425 reg and compare to the expected data
  Assume - caller validates the number of elements.
    # reg_info - list. Its length could be 1 to multiple (support up to 3 now)
    # reg_info[REG_ADDR_IDX] = 7425 register's physical address
    # reg_info[REG_EXP_DATA_IDX] = the expected register data
    # reg_info[REG_MASK_IDX] = mask (on) value
  """
  # Get the number of elements
  no_elems = len(reg_info)
  dbgprint('rd_cmd_7425(): len of reg_info= %d' % no_elems)

  # Read the register data
  # 1. get the register address
  reg_addr = int(reg_info[REG_ADDR_IDX], 16)

  # Based on reg_type setting, call the reg read handler according.
  try:
    if reg_type == BRCM_GENET_PHY_REGS:
      reg_val = reg_val_org  = rd_genet_ext_phy_74612_reg(reg_addr)
    elif reg_type == BRCM_7425_REGS:
      reg_val = reg_val_org  = rd_7425_reg(reg_addr)

  # Check if type of return value
  except DevmemException, inst:
    # Get the "Data bus error due to the register can't be accessed
    line = "**<addr> 0x%08x  %s\n" % (reg_addr, inst)
    dbgprint('rd_cmp_register(): Bus error line = %s' % line)
    # write register data to output file either
    result_file = open(reg_result_file, "a")
    result_file.write(line)
    result_file.close()
    return        # exit from the routine.
  else:
    dbgprint('rd_cmp_register(): 0x%08x' % reg_val)

  # default the data is identical
  reg_data_matched = True
  mask_val = 0x0ffffffff  # default - no mask
  exp_reg_data = reg_val      # default - matched to reg data

  # Check if include expected data and compare
  if no_elems >= REG_GOT_EXP_DATA_ELEM:
    # get the expected register data
    exp_reg_data = int(reg_info[REG_EXP_DATA_IDX], 16)

  # Check if include mask off data
  mask_value_exist = False
  if no_elems == REG_GOT_MASK_ELEM:
    # get the mask-off value
    mask_val = int(reg_info[REG_MASK_IDX], 16)
    mask_value_exist = True

  # Now compare the data value based on the expected data and mask-off value
  reg_val &= mask_val
  if reg_val != exp_reg_data:
    reg_data_matched = False      # Mismatched. Set the flag
    dbgprint('rd_cmp_register(): mismatched=0x%x (org reg data 0x%-10x)  expected data= %s' \
             % (reg_val, reg_val_org, reg_info[REG_EXP_DATA_IDX]))

  # Compose line to write to the result file
  if not reg_data_matched:
    line = "**"
  else:
    line = "  "
  line += "<addr> 0x%08x  " %reg_addr
  if no_elems >= REG_GOT_EXP_DATA_ELEM:
    line += "<exp> 0x%08x  " %exp_reg_data
  if no_elems == REG_GOT_MASK_ELEM:
    line += "<mask> 0x%08x  " %mask_val
  line += "<reg_data> 0x%08x" %reg_val_org
  if mask_value_exist:
    line += " (0x%08x)" %reg_val
  line += "\n"

  # write register data to output file either
  # 1. list_all_reg_msg is True, or
  # 2. the readback reg data is not matched to expected data
  if list_all_reg_msg or not reg_data_matched:
    result_file = open(reg_result_file, "a")
    result_file.write(line)
    result_file.close()


def get_cmp_reg_list_from_file():
  "parse the file to get register addresseses to read and their expected data"
  os.path.exists(reg_list_file)    # Check if register list exist
  if os.path.exists(reg_result_file):
    os.remove(reg_result_file)     # Remove the result file if exist

  reg_file = open(reg_list_file, "r")
  while True:
    line = reg_file.readline()
    if not line:
      dbgprint('Reach to end of file...')
      break

    reg_info = line.split()
    no_elems = len(reg_info)
    # Inline comment is not allowed. If detect '#' at the beginning of line
    # or inline, the line will not be checked
    # Please refer to regListFiles/bruno_7425_reg_list_B0.txt as examples
    # Syntax -
    #   Line won't be checked - reg_addr expected_data    # comment
    #   Line will be checked  - # comment
    #                           reg_addr expected_data
    if '#' in line or not no_elems:
      result_file = open(reg_result_file, "a")
      result_file.write(line)
      result_file.close()
      continue

    if no_elems > REG_MAX_ELEMS:
      dbgprint('Too many elements: %d  reg_info= %s' % (no_elems, str(reg_info).strip('[]')))
      continue        # Go to next line

    dbgprint('no_elems: %d  reg_info= %s' \
            % (no_elems, str(reg_info).strip('[]')))

    for i in range(no_elems):
      dbgprint('reg_info[%d]= %s' % (i, reg_info[i]))

    rd_cmp_register(reg_info)

    dbgprint('next line.....')

  reg_file.close()


class ModifyOptionParser(OptionParser):
  """
  Chande error() - print help if error detected
  """
  def error(self, msg):
    self.print_help()
    sys.exit(2)

def ParseArgs():
  """
  Parse command-line arguments
  """
  global show_dbg_msg, reg_type, reg_list_file, reg_result_file, list_all_reg_msg
  parser = ModifyOptionParser()

  # Add command-line options
  #
  # Specify an input file for register list to read
  parser.add_option("-f", "--regfile", dest="regfile",
                    default=BRUNO_REG_LIST_FILE,
                    help="read register list from REG_LIST_FILE",
                    metavar="REG_LIST_FILE")

  # Specify an output file to save the readback register data
  parser.add_option("-o", "--outputfile", dest="outputfile",
                    default=BRUNO_REG_RESULT_LIST_FILE,
                    help="output register data to DATA_FILE", metavar="DATA_FILE")

  # Specify the register type to read
  # 1 - read GENET external PHY register
  parser.add_option("-r", "--regtype",
                    action="store", dest="regtype", default=BRCM_7425_REGS,
                    help="select register type to read (GenetPhy, 7425Regs)",
                    metavar="REGTYPE")

  # Specify output all register data
  # 1 - list all register data
  parser.add_option("-l", "--listall",
                    action="store_true", dest="listall", default=False,
                    help="list all register data in DATA_FILE",
                    metavar="REGTYPE")

  # To enable print debugging message
  parser.add_option("-d", "--debug",
                    action="store_true", dest="verbose", default=False,
                    help="print debugging messages to stdout")

  (options, args) = parser.parse_args()
  dbgprint('options = %s args = %s' % (options, args))

  reg_type         = options.regtype
  show_dbg_msg     = options.verbose
  reg_list_file    = options.regfile
  reg_result_file  = options.outputfile
  list_all_reg_msg = options.listall
  dbgprint('reg_type= %s' % reg_type)
  dbgprint('show_dbg_msg= %d' % show_dbg_msg)
  dbgprint('reg list name = %s' % reg_list_file)
  dbgprint('output file = %s' % reg_result_file)
  dbgprint('list_all_reg_msg = %d' % list_all_reg_msg)


def main(argv):
  "main entry point"
  ParseArgs()

  # Call the handler according
  get_cmp_reg_list_from_file()

# Entry point
if __name__ == "__main__":
  main(sys.argv)

