#!/bin/bash
. ./wvtest/wvtest.sh

pid=$$
TAX=./host-dhcpvendortax

WVSTART "dhcpvendortax test"

# Check regex matches
WVPASS $TAX -l label -v "AastraIPPhone55i" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Aastra IP Phone"
WVPASS $TAX -l label -v "6=qPolycomSoundPointIP-SPIP_1234567-12345-001" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Polycom IP Phone"
WVPASS $TAX -l label -v "Polycom-VVX310" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Polycom IP Phone"

# Check exact matches
WVPASS $TAX -l label -v "Dell Network Printer" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Dell Printer"
WVPASS $TAX -l label -v "Xbox 360" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Xbox 360"

# Check model/type/manufacturer handling
WVPASS $TAX -l label -v "Mfg=DELL;Typ=Printer;Mod=Dell 2330dn Laser Printer;Ser=0123AB5;" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label DELL Printer"
WVPASS $TAX -l label -v "Mfg=DELL;Mod=Dell 2330dn Laser Printer;Ser=0123AB5;" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Dell 2330dn Laser Printer"

# Check case sensitivity
WVPASS $TAX -l label -v "mFG=DELL;tYP=Printer;mOD=Dell 2330dn Laser Printer;Ser=0123AB5;" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label DELL Printer"

# Check some other printer vendor formats
WVPASS $TAX -l label -v "Mfg=FujiXerox;Typ=AIO;Mod=WorkCentre 6027;Ser=P1A234567" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label FujiXerox AIO"
WVPASS $TAX -l label -v "Mfg=Hewlett Packard;Typ=Printer;Mod=HP LaserJet 400 M401n;Ser=ABCDE01234;" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Hewlett Packard Printer"
WVPASS $TAX -l label -v "mfg=Xerox;typ=MFP;mod=WorkCentre 3220;ser=ABC012345;loc=" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Xerox MFP"

# Check specials
WVPASS $TAX -l label -v "Dell 2155cn Color MFP" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Dell Printer"
WVPASS $TAX -l label -v "Dell C1760nw Color Printer" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Dell Printer"
WVPASS $TAX -l label -v "Dell C2660dn Color Laser" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Dell Printer"

WVPASS $TAX -l label -v "HT500 dslforum.org" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Grandstream VoIP adapter"
WVPASS $TAX -l label -v "HT7XX dslforum.org" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Grandstream VoIP adapter"

# check invalid or missing arguments. -l and -v are required.
WVFAIL $TAX
WVFAIL $TAX -l label
WVFAIL $TAX -v vendor

rm -f *.$pid.tmp
