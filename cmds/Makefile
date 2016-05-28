default: all

PREFIX=/
BINDIR=$(DESTDIR)$(PREFIX)/bin
LIBDIR=$(DESTDIR)$(PREFIX)/lib

PORTABLE_TARGETS=\
	balloon \
	gsetsid \
	isoping \
	isostream \
	mcastreceive \
	mmap \
	randomdata \
	setuid \
	udpburst \
	usleep

TARGETS=\
	$(PORTABLE_TARGETS) \
	alivemonitor \
	bsa2bluez \
	burnin-flash \
	buttonmon \
	chg_mod_own \
	cpulog \
	dhcpnametax \
	dhcpvendortax \
	dhcp-rogue \
	dir-monitor \
	diskbench \
	dnsck \
	freemegs \
	gstatic \
	http_bouncer \
	ionice \
	memwatcher \
	netusage \
	logos \
	randint \
	readubootver \
	realtime \
	rtwatcher \
	soft_rc \
	stun \
	wait-until-created \
	watch-dir

HOST_TARGETS=$(addprefix host-,$(TARGETS))
LIB_TARGETS=\
	stdoutline.so
HOST_TEST_TARGETS=\
	host-netusage_test \
	host-utils_test
SCRIPT_TARGETS=\
	is-secure-boot
ARCH_TARGETS=\

ifeq ($(BUILD_LIBNL_UTILS),y)
ARCH_TARGETS += wifi_files dnssdmon
TARGETS += asustax
HOST_TEST_TARGETS += host-wifi_files_test host-asustax_test
endif

ifeq ($(BUILD_SSDP),y)
TARGETS += ssdptax
HOST_TEST_TARGETS += host-test-ssdptax.sh
endif

ifeq ($(BUILD_DNSSD),y)
# Don't bother building for host
ARCH_TARGETS += dnssd_hosts
endif

ifeq ($(BUILD_IBEACON),y)
ARCH_TARGETS += ibeacon eddystone
endif

ifeq ($(BUILD_STATUTILS),y)
TARGETS += statpitcher statcatcher
endif

AS=$(CROSS_COMPILE)as
CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
LD=$(CROSS_COMPILE)gcc
HOST_CC ?= cc
HOST_CXX ?= g++
HOST_LD ?= cc
HOST_PROTOC ?= $(HOSTDIR)/usr/bin/protoc
GPERF ?= gperf
CFLAGS += -Wall -Wextra -Wswitch-enum -Werror -Wno-unused-parameter \
	-g -O -std=c99 -D_GNU_SOURCE $(EXTRACFLAGS)
CXXFLAGS += -Wall -Wextra -Wswitch-enum -Werror -Wno-unused-parameter \
	-g -O -std=gnu++0x -D_GNU_SOURCE $(EXTRACXXFLAGS)
LDFLAGS += $(EXTRALDFLAGS)
HOST_INCS=-I$(HOSTDIR)/usr/include
HOST_LIBS=-L$(HOSTDIR)/usr/lib -Wl,-rpath=$(HOSTDIR)/usr/lib
INCS=-I../libstacktrace

.PRECIOUS: %.o host-%.o

# add minimal MacOS build to support checking out dir and building test commands
UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
all: $(PORTABLE_TARGETS)
else
all: $(TARGETS) $(HOST_TARGETS) $(LIB_TARGETS) $(ARCH_TARGETS) $(TEST_TARGETS)
RT = -lrt
endif

install:
	mkdir -p $(BINDIR) $(LIBDIR)
	cp $(TARGETS) $(ARCH_TARGETS) $(BINDIR)
	cp $(LIB_TARGETS) $(LIBDIR)
	for n in $(SCRIPT_TARGETS); do \
		test ! -f $$n.$(BR2_TARGET_GENERIC_PLATFORM_NAME) || \
			cp -f $$n.$(BR2_TARGET_GENERIC_PLATFORM_NAME) $(BINDIR)/$$n; \
	done

install-libs:
	@echo "No libs to install."

%.o: %.c
	$(CC) $(CFLAGS) $(INCS) -o $@ -c $<

%.po: %.c
	$(CC) $(CFLAGS) -fPIC $(INCS) -o $@ -c $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(INCS) -o $@ -c $<

%.po: %.cc
	$(CXX) $(CXXFLAGS) -fPIC $(INCS) -o $@ -c $<

host-%.o: %.c
	$(HOST_CC) $(CFLAGS) $(INCS) $(HOST_INCS) -DCOMPILE_FOR_HOST=1 -o $@ -c $<

host-%.o: %.cc
	$(HOST_CXX) $(CXXFLAGS) $(INCS) $(HOST_INCS) -DCOMPILE_FOR_HOST=1 -o $@ -c $<

%: %.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS) -lc

host-%: host-%.o
	$(HOST_CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS) -lc

%: %.py
	ln -s $< $@

host-%: %.py
	ln -s $< $@

%.so: %.po
	$(CC) $(CFLAGS) $(INCS) -o $@ -fPIC -shared -Wl,-soname,$@ $<

%.pb.cc: %.proto
	echo "Building .pb.cc"
	$(HOST_PROTOC) --cpp_out=. $<

host-%.pb.cc: %.proto
	echo "Building .pb.cc"
	$(HOST_PROTOC) --cpp_out=. $<


host-isoping isoping: LIBS+=$(RT) -lm
host-isostream isostream: LIBS+=$(RT)
host-diskbench diskbench: LIBS+=-lpthread $(RT)
host-dnsck: LIBS+=$(HOST_LIBS) -lcares $(RT)
host-http_bouncer: LIBS+=$(HOST_LIBS) -lcurl $(RT)
dnssd_hosts: LIBS+=-lavahi-client -lavahi-common
host-dnssd_hosts: LIBS+=-lavahi-client
host-logos logos: LIBS+=-lrt
logos: LIBS+=-L../libstacktrace -lstacktrace
host-randint randint: LIBS+=-lstdc++
host-rtwatcher rtwatcher: LIBS+=-lpthread $(RT)
host-realtime realtime: LIBS+=-lpthread
host-alivemonitor alivemonitor: LIBS+=$(RT)
host-buttonmon buttonmon: LIBS+=$(RT)
alivemonitor: alivemonitor.o
isoping: isoping.o
isostream: isostream.o
diskbench: diskbench.o
dnsck: LIBS+=-lcares $(RT)
dnsck: dnsck.o
dnssd_hosts: dnssd_hosts.o
host-logos: host-logos.o host-utils.o
logos: logos.o utils.o
realtime: realtime.o
rtwatcher: rtwatcher.o
buttonmon: buttonmon.o
http_bouncer: LIBS+=-lcurl $(RT)
http_bouncer: http_bouncer.o
host-utils_test: host-utils_test.o host-utils.o
asustax: asustax.o l2utils.o
asustax: LIBS += -lnl-3 -lstdc++ -lm
host-asustax: host-asustax.o host-l2utils.o
host-asustax: LIBS += $(HOST_LIBS) -lnl-3 -lstdc++ -lm
host-asustax_test: host-asustax_test.o
host-asustax_test: LIBS += $(HOST_LIBS) -lstdc++ -lm
ssdptax: ssdptax.o l2utils.o
ssdptax: LIBS += -lcurl -lnl-3 -lstdc++ -lm
host-ssdptax: host-ssdptax.o host-l2utils.o
host-ssdptax: LIBS += $(HOST_LIBS) -lcurl -lnl-3 -lstdc++ -lm
statpitcher.o: device_stats.pb.o
statpitcher: LIBS+=-L$(DESTDIR)$(PREFIX)/usr/lib -lprotobuf-lite -lpthread -lstdc++
statpitcher: device_stats.pb.o statpitcher.o
host-statpitcher: LIBS+=-L$(HOSTDIR)/usr/lib -lprotobuf-lite -lpthread -lstdc++
host-statpitcher: host-device_stats.pb.o host-statpitcher.o
statcatcher.o: device_stats.pb.o
statcatcher: LIBS+=-L$(DESTDIR)$(PREFIX)/usr/lib -lprotobuf-lite -lpthread -lstdc++
statcatcher: device_stats.pb.o statcatcher.o
host-statcatcher: LIBS+=-L$(HOSTDIR)/usr/lib -lprotobuf-lite -lpthread -lstdc++
host-statcatcher: host-device_stats.pb.o host-statcatcher.o
ibeacon: LIBS += -lbluetooth -luuid
ibeacon: CFLAGS += -std=gnu99
ibeacon: ibeacon.o
eddystone: LIBS += -lbluetooth
eddystone: CFLAGS += -std=gnu99
eddystone: eddystone.o
host-dir-monitor dir-monitor: LIBS+=-lstdc++
netusage: CFLAGS += -Wno-sign-compare
host-netusage_test: host-netusage_test.o
wifi_files: wifi_files.o
wifi_files: LIBS += -lnl-3 -lnl-genl-3 -lglib-2.0
host-wifi_files_test: host-wifi_files_test.o
host-wifi_files_test: LIBS += -lnl-3 -lnl-genl-3 -lglib-2.0
dhcpvendortax: dhcpvendortax.o dhcpvendorlookup.o
dhcpvendorlookup.c: dhcpvendorlookup.gperf
	$(GPERF) -G -C -t -L ANSI-C -N exact_match -K vendor_class \
		--includes --output-file=dhcpvendorlookup.c dhcpvendorlookup.gperf
dhcpvendorlookup.o: CFLAGS += -Wno-missing-field-initializers
host-dhcpvendorlookup.o: CFLAGS += -Wno-missing-field-initializers
host-dhcpvendortax: host-dhcpvendortax.o host-dhcpvendorlookup.o
dnssdmon: dnssdmon.o l2utils.o modellookup.o
dnssdmon: LIBS += -lnl-3 -lstdc++ -lm -lresolv
modellookup.c: modellookup.gperf
	$(GPERF) -G -C -t -T -L ANSI-C -n -N model_lookup -K model --delimiters="|" \
		--includes --output-file=modellookup.c modellookup.gperf
modellookup.o: CFLAGS += -Wno-missing-field-initializers
host-modellookup.o: CFLAGS += -Wno-missing-field-initializers
dhcpnametax: dhcpnametax.o hostnamelookup.o
host-dhcpnametax: host-dhcpnametax.o host-hostnamelookup.o
hostnamelookup.c: hostnamelookup.gperf
	$(GPERF) -G -C -t -T -L ANSI-C -n -c -N hostname_lookup -K name --delimiters="|" \
		--includes --output-file=hostnamelookup.c hostnamelookup.gperf
hostnamelookup.o: CFLAGS += -Wno-missing-field-initializers
host-hostnamelookup.o: CFLAGS += -Wno-missing-field-initializers


TESTS = $(wildcard test-*.sh) $(wildcard test-*.py) $(wildcard *_test.py) $(TEST_TARGETS)
ifeq ($(RUN_HOST_TESTS),y)
TESTS += $(HOST_TEST_TARGETS)
endif
runtests: all $(TESTS)
	set -e; \
	for d in $(TESTS); do \
		echo Running $$d; \
		./$$d; \
	done

test: all $(TESTS)
	./wvtest/wvtestrun $(MAKE) runtests

clean:
	rm -f *.o $(TARGETS) \
		$(HOST_TARGETS) \
		$(LIB_TARGETS) \
		$(TEST_TARGETS) \
		$(HOST_TEST_TARGETS) \
		$(ARCH_TARGETS) \
		*~ .*~ */*.pyc test_file *.pb.*
	rm -rf test_dir
