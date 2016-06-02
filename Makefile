default: all

# Build everything by default
BUILD_HNVRAM?=      # default off: needs separate library
BUILD_SSDP?=y
BUILD_DNSSD?=y
BUILD_LOGUPLOAD?=   # default off: needs libgtest
BUILD_IBEACON?=     # default off: needs bluetooth.h
BUILD_WAVEGUIDE?=y
BUILD_DVBUTILS?=y
BUILD_SYSMGR?=y
BUILD_STATUTILS?=y
BUILD_SPEEDTEST?=y
BUILD_CRYPTDEV?=    # default off: needs libdevmapper
BUILD_SIGNING?=     # default off: needs libgtest
BUILD_JSONPOLL?=n
BUILD_PRESTERASTATS?=n
export BUILD_HNVRAM BUILD_SSDP BUILD_DNSSD BUILD_LOGUPLOAD \
	BUILD_IBEACON BUILD_WAVEGUIDE BUILD_DVBUTILS BUILD_SYSMGR \
	BUILD_STATUTILS BUILD_CRYPTDEV BUILD_SIGNING BUILD_JSONPOLL \
	BUILD_PRESTERASTATS

# note: libgpio is not built here.  It's conditionally built
# via buildroot/packages/google/google_platform/google_platform.mk
DIRS=libstacktrace libexperiments ginstall cmds \
	antirollback tvstat gpio-mailbox spectralanalyzer wifi wifiblaster \
	sysvar py_mtd devcert

ifeq ($(BUILD_SYSMGR),y)
DIRS+=sysmgr
DIRS+=base
endif

ifeq ($(BUILD_WAVEGUIDE),y)
DIRS+=waveguide
DIRS+=taxonomy
endif

ifeq ($(BUILD_HNVRAM),y)
DIRS+=hnvram
endif

ifeq ($(BUILD_LOGUPLOAD),y)
DIRS+=logupload/client
endif

ifeq ($(BUILD_DVBUTILS),y)
DIRS+=dvbutils
endif

ifeq ($(BUILD_CRYPTDEV),y)
DIRS+=cryptdev
endif

ifeq ($(BUILD_SIGNING),y)
DIRS+=signing
endif

ifeq ($(BUILD_SPEEDTEST),y)
DIRS+=speedtest
endif

ifeq ($(BUILD_JSONPOLL),y)
DIRS+=jsonpoll
endif

ifeq ($(BUILD_CRAFTUI),y)
DIRS+=craftui
endif

ifeq ($(BR2_TARGET_GENERIC_PLATFORM_NAME),gfsc100)
DIRS+=diags
endif

ifeq ($(BR2_TARGET_GENERIC_PLATFORM_NAME),gfmn100)
DIRS+=diags
endif

ifeq ($(BUILD_CONMAN),y)
DIRS+=conman
endif

ifeq ($(BUILD_PRESTERASTATS),y)
DIRS+=presterastats
endif

PREFIX=/usr
BINDIR=$(DESTDIR)$(PREFIX)/bin
LIBDIR=$(DESTDIR)$(PREFIX)/lib


all:     $(addsuffix /all,$(DIRS)) build-commonpy
test:    $(addsuffix /test,$(DIRS))
clean:   $(addsuffix /clean,$(DIRS))
	find \( -name '*.pyc' -o -name '*~' \) -exec rm -fv {} \;
	rm -rf build

install-libs: $(addsuffix /install-libs,$(DIRS))

# The install targets in the recursive call use setuptools to build the python
# packages. These cannot be run in parallel, as they appear to race with each
# other to write site-packages/easy-install.pth.
install:
	set -e; for d in $(DIRS); do $(MAKE) -C $$d install; done
	$(MAKE) install-commonpy
	mkdir -p $(BINDIR)
	rm -fv $(BINDIR)/hnvram
ifeq ($(BR2_TARGET_GENERIC_PLATFORM_NAME), gfmn110)
	ln -s /usr/bin/hnvram_wrapper $(BINDIR)/hnvram
else ifeq ($(BR2_TARGET_GENERIC_PLATFORM_NAME), gflt110)
	ln -s /usr/bin/hnvram_wrapper $(BINDIR)/hnvram
else
	ln -s /usr/bin/hnvram_binary $(BINDIR)/hnvram
endif

sysmgr/all: base/all libstacktrace/all libexperiments/all
cmds/all: libstacktrace/all libexperiments/all
gpio-mailbox/all: libstacktrace/all libexperiments/all

%/all:
	$(MAKE) -C $* all

%/test:
	$(MAKE) -C $* test

%/clean:
	$(MAKE) -C $* clean

%/install:
	$(MAKE) -C $* install

build-commonpy:
	PYTHONPATH=$(HOSTPYTHONPATH) $(HOSTDIR)/usr/bin/python setup.py build
	PYTHONPATH=$(TARGETPYTHONPATH) $(HOSTDIR)/usr/bin/python setup.py build

install-commonpy:
	PYTHONPATH=$(HOSTPYTHONPATH) $(HOSTDIR)/usr/bin/python setup.py install --prefix=$(HOSTDIR)$(PREFIX)
	PYTHONPATH=$(TARGETPYTHONPATH) $(HOSTDIR)/usr/bin/python setup.py install --prefix=$(DESTDIR)$(PREFIX)

%/install-libs:
	$(MAKE) -C $* install-libs
