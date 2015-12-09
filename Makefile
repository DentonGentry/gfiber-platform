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
BUILD_CRYPTDEV?=    # default off: needs libdevmapper
BUILD_SIGNING?=     # default off: needs libgtest
export BUILD_HNVRAM BUILD_SSDP BUILD_DNSSD BUILD_LOGUPLOAD \
	BUILD_IBEACON BUILD_WAVEGUIDE BUILD_DVBUTILS BUILD_SYSMGR \
	BUILD_STATUTILS BUILD_CRYPTDEV BUILD_SIGNING

# note: libgpio is not built here.  It's conditionally built
# via buildroot/packages/google/google_platform/google_platform.mk
DIRS=libstacktrace ginstall cmds \
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


ifeq ($(BR2_TARGET_GENERIC_PLATFORM_NAME),gfsc100)
DIRS+=diags
endif

ifeq ($(BR2_TARGET_GENERIC_PLATFORM_NAME),gfmn100)
DIRS+=diags
endif

PREFIX=/usr
BINDIR=$(DESTDIR)$(PREFIX)/bin
LIBDIR=$(DESTDIR)$(PREFIX)/lib


all:     $(addsuffix /all,$(DIRS)) build-optionspy
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
	$(MAKE) install-optionspy

sysmgr/all: base/all libstacktrace/all
cmds/all: libstacktrace/all
gpio-mailbox/all: libstacktrace/all

%/all:
	$(MAKE) -C $* all

%/test:
	$(MAKE) -C $* test

%/clean:
	$(MAKE) -C $* clean

%/install:
	$(MAKE) -C $* install

build-optionspy:
	PYTHONPATH=$(HOSTPYTHONPATH) $(HOSTDIR)/usr/bin/python setup.py build
	PYTHONPATH=$(TARGETPYTHONPATH) $(HOSTDIR)/usr/bin/python setup.py build

install-optionspy:
	PYTHONPATH=$(HOSTPYTHONPATH) $(HOSTDIR)/usr/bin/python setup.py install --prefix=$(HOSTDIR)$(PREFIX)
	PYTHONPATH=$(TARGETPYTHONPATH) $(HOSTDIR)/usr/bin/python setup.py install --prefix=$(DESTDIR)$(PREFIX)

%/install-libs:
	$(MAKE) -C $* install-libs
