#/bin/sh
# Wifipacket.py helper

if [ ! -d /tmp/fingerprints ]; then
	echo mkdir /tmp/fingerprints
	mkdir /tmp/fingerprints
fi

FILES=/usr/local/google/home/shantanuj/gfiber/vendor/google/platform/wifi_fingerprint/filtered_fingerprint_pcaps/*
for f in $FILES
do
	if [ ! -d /tmp/fingerprints/$(basename $f) ]; then
		echo mkdir /tmp/fingerprints/$(basename $f)
		mkdir /tmp/fingerprints/$(basename $f)
	fi
	k="$f/*"
	for g in $k
	do
		python /usr/local/google/home/shantanuj/gfiber/vendor/google/platform/wifi_fingerprint/pcap_parsing/wifipacket.py $g > /tmp/fingerprints/$(basename $f)/$(basename $g)
	done
done
