default: all

DIRS=ginstall diag cmds sysmgr base antirollback startupvideo

PREFIX=/usr
BINDIR=$(DESTDIR)$(PREFIX)/bin
LIBDIR=$(DESTDIR)$(PREFIX)/lib


all:     $(addsuffix /all,$(DIRS)) build-optionspy
test:    $(addsuffix /test,$(DIRS))
clean:   $(addsuffix /clean,$(DIRS))
install: $(addsuffix /install,$(DIRS)) install-optionspy
install-libs: $(addsuffix /install-libs,$(DIRS))

sysmgr/all: base/all

%/all:
	$(MAKE) -C $* all

%/test:
	$(MAKE) -C $* test

%/clean:
	$(MAKE) -C $* clean

%/install:
	$(MAKE) -C $* install

build-optionspy:
	PYTHONPATH=$(PYTHONPATH) $(HOST_DIR)/usr/bin/python setup.py build

install-optionspy:
	PYTHONPATH=$(PYTHONPATH) $(HOST_DIR)/usr/bin/python setup.py install --prefix=$(DESTDIR)$(PREFIX)

%/install-libs:
	$(MAKE) -C $* install-libs
