# Copyright (c) 2014, Infineon Technologies AG
# All rights reserved.
from sys import stderr,exit
import sys, getopt
tpm0= stderr
def init():
        global tpm0
        tpm0 = open("/dev/tpm0","r+b")

def deinit():
        global tpm0
        tpm0.close()


def transmit(cmd):
        global tpm0
        print "Sending",
        for c in cmd:
                print hex(ord(c)),
        print
        tpm0.write(cmd);
        print "Receiving" ,
        tpm0.flush()
        try:
                response=tpm0.read()
                for c in response:
                        print hex(ord(c)),

                print
                return [
                ord(response[6]),
                ord(response[7]),
                ord(response[8]),
                ord(response[9]),
                ]

        except (IOError) as (errno, strerror):
                print "Received IO Error {0}: {1}".format(errno, strerror)
                response = "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
                return -errno;

def main(argv):

        print "TPM Startup Clear"
        cmd = "\x00\xc1\x00\x00\x00\x0c\x00\x00\x00\x99\x00\x01"
        rc = transmit(cmd)
        if (rc[0] == 0 and rc[1] == 0 and rc[2] == 0 and rc[3] == 0):
                print "works as expected, returned Status 0"
        elif (rc[0] == 0 and rc[1] == 0 and rc[2] == 0 and rc[3] == 0x26):
                print "startup already sent"
        else:
                print "ERROR: returned %x %x %x %x"%(rc[0],rc[1],rc[2],rc[3])
                exit(1);

        print "TPM Selftest"
        cmd = "\x00\xc1\x00\x00\x00\x0a\x00\x00\x00\x50"
        rc = transmit(cmd)
        if (rc[0] == 0 and rc[1] == 0 and rc[2] == 0 and rc[3] == 0):
                print "works as expected, returned Status 0"
        else:
                print "ERROR: returned %x %x %x %x"%(rc[0],rc[1],rc[2],rc[3])
                exit(1);

        if (argv == []):
                print "Lock Physical Presence"
                cmd = "\x00\xc1\x00\x00\x00\x0C\x40\x00\x00\x0A\x00\x04"
                rc = transmit(cmd)
                if (rc[0] == 0 and rc[1] == 0 and rc[2] == 0 and rc[3] == 0):
                        print "works as expected, returned Status 0"
                else:
                        print "ERROR: returned %x %x %x %x"%(rc[0],rc[1],rc[2],rc[3])
                        exit(1);

        try:
                opts, args = getopt.getopt(argv,"a")
        except getopt.GetoptError:
                print 'input error'
                sys.exit(2)
        for opt, arg in opts:
                if opt == '-a':

                        print "Enable Physical Presence Command"
                        cmd = "\x00\xc1\x00\x00\x00\x0C\x40\x00\x00\x0A\x00\x20"
                        rc = transmit(cmd)
                        if (rc[0] == 0 and rc[1] == 0 and rc[2] == 0 and rc[3] == 0):
                                print "works as expected, returned Status 0"
                        else:
                                print "ERROR: returned %x %x %x %x"%(rc[0],rc[1],rc[2],rc[3])
                                exit(1);

                        print "Assert Physical Presence"
                        cmd = "\x00\xc1\x00\x00\x00\x0C\x40\x00\x00\x0A\x00\x08"
                        rc = transmit(cmd)
                        if (rc[0] == 0 and rc[1] == 0 and rc[2] == 0 and rc[3] == 0):
                                print "works as expected, returned Status 0"
                        else:
                                print "ERROR: returned %x %x %x %x"%(rc[0],rc[1],rc[2],rc[3])
                                exit(1);

                        print "Enable TPM"
                        cmd = "\x00\xc1\x00\x00\x00\x0A\x00\x00\x00\x6F"
                        rc = transmit(cmd)
                        if (rc[0] == 0 and rc[1] == 0 and rc[2] == 0 and rc[3] == 0):
                                print "works as expected, returned Status 0"
                        else:
                                print "ERROR: returned %x %x %x %x"%(rc[0],rc[1],rc[2],rc[3])
                                exit(1);

                        print "Activate TPM"
                        cmd = "\x00\xc1\x00\x00\x00\x0B\x00\x00\x00\x72\x00"
                        rc = transmit(cmd)
                        if (rc[0] == 0 and rc[1] == 0 and rc[2] == 0 and rc[3] == 0):
                                print "works as expected, returned Status 0"
                        else:
                                print "ERROR: returned %x %x %x %x"%(rc[0],rc[1],rc[2],rc[3])
                                exit(1);

if __name__ == "__main__":
        init()
        main(sys.argv[1:])
        deinit()
