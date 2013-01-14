# Copyright 2011 Google Inc. All Rights Reserved.
# Author: kedong@google.com (Ke Dong)

default: all

DIRS=utest peripheral
TARGETS=sysmgr $(addsuffix /all,$(DIRS))
INSTALL=install
PREFIX=$(DESTDIR)/usr
BINDIR=$(PREFIX)/bin
LIBDIR=$(PREFIX)/lib
INCLUDEDIR=$(PREFIX)/include

CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
RM=rm -f
INCS=-Iperipheral -I../base -I../libstacktrace
CFLAGS=-Wall -Wimplicit -Wno-unknown-pragmas -W
CPPFLAGS=$(subst -Wstrict-prototypes,,$(subst -std=c99,,$(shell $(PKG_CONFIG) --cflags brunobase)))
SYSMGR_LDFLAGS=-Lperipheral -L../base -L../libstacktrace -lbrunoperipheral -lstacktrace\
	$(shell $(PKG_CONFIG) --libs brunobase)

all: $(TARGETS)

test: all
	@echo "Nothing to test."

install: all utest/install
	mkdir -p $(LIBDIR)
	$(INSTALL) -m 0755 peripheral/libbrunoperipheral.so $(LIBDIR)/
	$(INSTALL) -m 0755 sysmgr $(BINDIR)/

utest/all: peripheral/all
sysmgr: |peripheral/all
utest/install: utest/all
sysmgr: sysmgr.o

sysmgr: LDFLAGS+=$(SYSMGR_LDFLAGS)

install-libs: all
	mkdir -p $(INCLUDEDIR)/sysmgr $(LIBDIR)/pkgconfig
	$(INSTALL) -m 0644 platform_peripheral_api.h $(INCLUDEDIR)/sysmgr/
	$(INSTALL) -m 0644 brunoperipheral.pc $(LIBDIR)/pkgconfig/
	$(INSTALL) -m 0755 peripheral/libbrunoperipheral.so $(LIBDIR)/

clean: $(addsuffix /clean,$(DIRS))
	$(RM) *~ *.o $(TARGETS)

%: %.o
	@echo "LINK $@"
	$(Q)$(CXX) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(INCS) \
		-o $@ \
		$(filter %.o,$^)

%.o: %.cc
	@echo " CXX $@"
	$(Q)$(CXX) $(CFLAGS) $(CPPFLAGS) $(INCS) -o $@ -c $<

%.o: %.c
	@echo " CC  $@"
	$(Q)$(CC) -std=c99 $(CFLAGS) $(CPPFLAGS) $(INCS) -o $@ -c $<

%/all:
	$(MAKE) -C $* all

%/clean:
	$(MAKE) -C $* clean

%/install:
	$(MAKE) -C $* install
