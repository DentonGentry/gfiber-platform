default: all

DIRS=ginstall sysmgr cmds base antirollback libstacktrace tvstat

# TODO(apenwarr): only install this on appropriate platforms.
#  For now, catawampus depends on sfmodule to run its tests, so we'll
#  just always install it.
DIRS+=prism/sfmodule

ifeq ($(HAS_MOCA),y)
DIRS+=diag
endif

PREFIX=/usr
BINDIR=$(DESTDIR)$(PREFIX)/bin
LIBDIR=$(DESTDIR)$(PREFIX)/lib


all:     $(addsuffix /all,$(DIRS)) build-optionspy
test:    $(addsuffix /test,$(DIRS))
clean:   $(addsuffix /clean,$(DIRS))
install-libs: $(addsuffix /install-libs,$(DIRS))

# The install targets in the recursive call use setuptools to build the python
# packages. These cannot be run in parallel, as they appear to race with each
# other to write site-packages/easy-install.pth.
install:
	set -e; for d in $(DIRS); do $(MAKE) -C $$d install; done
	$(MAKE) install-optionspy

diag/all: libstacktrace/all

sysmgr/all: base/all libstacktrace/all

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
