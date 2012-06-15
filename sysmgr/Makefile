# Copyright 2011 Google Inc. All Rights Reserved.
# Author: kedong@google.com (Ke Dong)

default: all

DIRS=utest peripheral
TARGETS=sysmgr $(addsuffix /all,$(DIRS))
INSTALL=install
DESTDIR=/
PREFIX=$(DESTDIR)/usr
BINDIR=$(PREFIX)/bin
LIBDIR=$(PREFIX)/lib
INCLUDEDIR=$(PREFIX)/include

CXX=$(CROSS_COMPILE)g++
RM=rm -f
INCS=-Iperipheral -I../base
CFLAGS=-Wall -Wimplicit -Wno-unknown-pragmas -W
CPPFLAGS=$(subst -Wstrict-prototypes,,$(subst -std=c99,,$(shell $(PKG_CONFIG) --cflags nexus brunobase)))
LDFLAGS=-Lperipheral -L../base -lbrunoperipheral \
	$(shell $(PKG_CONFIG) --libs nexus brunobase)
OBJS=$(patsubst %.cc,%.o,$(wildcard *.cc))

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

install-libs: all
	mkdir -p $(INCLUDEDIR)/sysmgr $(LIBDIR)/pkgconfig
	$(INSTALL) -m 0644 platform_peripheral_api.h $(INCLUDEDIR)/sysmgr/
	$(INSTALL) -m 0644 brunoperipheral.pc $(LIBDIR)/pkgconfig/
	$(INSTALL) -m 0755 peripheral/libbrunoperipheral.so $(LIBDIR)/

clean: $(addsuffix /clean,$(DIRS))
	$(RM) *~ *.o $(TARGETS)

%: %.o
	$(CXX) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(INCS) \
		-o $@ \
		$(filter %.o,$^)

%.o: %.cc
	$(CXX) $(CLFAGS) $(CPPFLAGS) $(INCS) -o $@ -c $<

%/all:
	$(MAKE) -C $* all

%/clean:
	$(MAKE) -C $* clean

%/install:
	$(MAKE) -C $* install