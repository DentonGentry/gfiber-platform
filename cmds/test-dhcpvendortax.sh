#!/bin/bash
. ./wvtest/wvtest.sh

pid=$$
TAX=./host-dhcpvendortax

WVSTART "dhcpvendortax test"

# Check regex matches
WVPASS $TAX -l label -v "AastraIPPhone55i" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Aastra IP Phone;55i"
WVPASS $TAX -l label -v "AXIS,Network Camera,M3006,5.40.13" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label AXIS Network Camera;M3006"
WVPASS $TAX -l label -v "AXIS,Thermal Network Camera,Q1931-E,5.55.4.1" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label AXIS Network Camera;Q1931-E"
WVPASS $TAX -l label -v "Canon MF620C Series" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Canon Printer;MF620C"
WVPASS $TAX -l label -v "Cisco AP c1240" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Cisco Wifi AP;c1240"
WVPASS $TAX -l label -v "Cisco Systems, Inc. IP Phone CP-7961G" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Cisco IP Phone;CP-7961G"
WVPASS $TAX -l label -v "Cisco SPA525G2" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Cisco IP Phone;SPA525G2"
WVPASS $TAX -l label -v "ATA186-H6.0|V3.2.0|B041111A" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Cisco IP Phone;ATA186"
WVPASS $TAX -l label -v "CPQRIB3" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Compaq Remote Insight;CPQRIB3"
WVPASS $TAX -l label -v "Dell Color MFP E525w" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Dell Printer;E525w"
WVPASS $TAX -l label -v "Dell C1760nw Color Printer" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Dell Printer;C1760nw"
WVPASS $TAX -l label -v "Dell C2660dn Color Laser" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Dell Printer;C2660dn"
WVPASS $TAX -l label -v "Dell 2155cn Color MFP" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Dell Printer;2155cn"
WVPASS $TAX -l label -v "FortiAP-FP321C" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Fortinet Wifi AP;FP321C"
WVPASS $TAX -l label -v "FortiWiFi-60D-POE" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Fortinet Wifi AP;60D-POE"
WVPASS $TAX -l label -v "Grandstream GXP1405 dslforum.org" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Grandstream IP Phone;GXP1405"
WVPASS $TAX -l label -v "Grandstream HT702 dslforum.org" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Grandstream VoIP Adapter;HT702"
WVPASS $TAX -l label -v "HT500 dslforum.org" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Grandstream VoIP Adapter;HT500"
WVPASS $TAX -l label -v "DP7XX dslforum.org" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Grandstream IP Phone;DP7XX"
WVPASS $TAX -l label -v "iPECS IP Edge 5000i-24G" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label iPECS IP Phone;5000i-24G"
WVPASS $TAX -l label -v "Juniper-ex2200-c-12p-2g" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Juniper Router;ex2200"
WVPASS $TAX -l label -v "LINKSYS SPA-942" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Linksys IP Phone;SPA-942"
WVPASS $TAX -l label -v "MotorolaAP.AP7131" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Motorola Wifi AP;AP7131"
WVPASS $TAX -l label -v "NECDT700" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label NEC IP Phone;NECDT700"
WVPASS $TAX -l label -v "6=qPolycomSoundPointIP-SPIP_1234567-12345-001" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Polycom IP Phone;SPIP_1234567"
WVPASS $TAX -l label -v "Polycom-VVX310" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Polycom IP Phone;VVX310"
WVPASS $TAX -l label -v "Rabbit2000-TCPIP:Z-World:Testfoo:1.1.3" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Rabbit Microcontroller;Rabbit2000"
WVPASS $TAX -l label -v "Rabbit-TCPIP:Z-World:DHCP-Test:1.2.0" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Rabbit Microcontroller;Rabbit"
WVPASS $TAX -l label -v "ReadyNet_WRT500" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label ReadyNet Wifi AP;WRT500"
WVPASS $TAX -l label -v "SAMSUNG SCX-6x45" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Samsung Network MFP;SCX-6x45"
WVPASS $TAX -l label -v "SF200-24P" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Cisco Managed Switch;SF200-24P"
WVPASS $TAX -l label -v "SG 200-08" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Cisco Managed Switch;SG 200-08"
WVPASS $TAX -l label -v "SG200-26" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Cisco Managed Switch;SG200-26"
WVPASS $TAX -l label -v "SG300-10" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Cisco Managed Switch;SG300-10"
WVPASS $TAX -l label -v "snom-m3-SIP/02.11//18-Aug-10 15:36" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Snom IP Phone;snom-m3-SIP"
WVPASS $TAX -l label -v "snom320" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Snom IP Phone;snom320"
WVPASS $TAX -l label -v "telsey-stb-f8" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Telsey Media Player;f8"

# Check exact matches
WVPASS $TAX -l label -v "Dell Network Printer" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Dell Printer;Dell Printer"
WVPASS $TAX -l label -v "Xbox 360" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Xbox;Xbox 360"

# Check model/type/manufacturer handling
WVPASS $TAX -l label -v "Mfg=DELL;Typ=Printer;Mod=Dell 2330dn Laser Printer;Ser=0123AB5;" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label DELL Printer;Dell 2330dn Laser Printer"

# Check case sensitivity
WVPASS $TAX -l label -v "mFG=DELL;tYP=Printer;mOD=Dell 2330dn Laser Printer;Ser=0123AB5;" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label DELL Printer;Dell 2330dn Laser Printer"

# Check some other printer vendor formats
WVPASS $TAX -l label -v "Mfg=FujiXerox;Typ=AIO;Mod=WorkCentre 6027;Ser=P1A234567" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label FujiXerox AIO;WorkCentre 6027"
WVPASS $TAX -l label -v "Mfg=Hewlett Packard;Typ=Printer;Mod=HP LaserJet 400 M401n;Ser=ABCDE01234;" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Hewlett Packard Printer;HP LaserJet 400 M401n"
WVPASS $TAX -l label -v "mfg=Xerox;typ=MFP;mod=WorkCentre 3220;ser=ABC012345;loc=" >test1.$pid.tmp
WVPASSEQ "$(cat test1.$pid.tmp)" "dhcpv label Xerox MFP;WorkCentre 3220"

# check invalid or missing arguments. -l and -v are required.
WVFAIL $TAX
WVFAIL $TAX -l label
WVFAIL $TAX -v vendor

rm -f *.$pid.tmp
