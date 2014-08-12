# Shantanu Jain shantanuj@google.com
# July 14 2014
# Helper script to create a regularly formatted text file to help 
# 	identify important fields when fingerprinting clients

echo usage: fingerprintHelper interface MAC-Addr-Filter-usually-fingerprintee
echo Best to use a monitor mode interface created with airmon-ng

tcpdump -i wlan1 -s 0 -nneI "(ether host $1) and ((wlan type mgt subtype assoc-req) or (wlan type mgt subtype auth) or wlan type mgt subtype probe-req)" -w $2