default: all

# note: libgpio is not built here.  It's conditionally built
# via buildroot/packages/google/google_platform/google_platform.mk
DIRS=libstacktrace ginstall sysmgr cmds base \
	antirollback tvstat gpio-mailbox spectralanalyzer wifiblaster

ifneq ($(BR2_TARGET_GOOGLE_PLATFORM),gfiberlt)
DIRS+=waveguide
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
logos/all: libstacktrace/all
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
