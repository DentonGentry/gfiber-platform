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
CXXFLAGS=

all: $(TARGETS)

test:
	@echo "Nothing to test."

install: all utest/install
	mkdir -p $(LIBDIR) $(BINDIR)
	$(INSTALL) -m 0755 peripheral/libbrunoperipheral.so $(LIBDIR)/
	$(INSTALL) -m 0755 sysmgr $(BINDIR)/

utest/all: peripheral/all
sysmgr: |peripheral/all
utest/install: utest/all
sysmgr: \
	sysmgr.o \
	../libstacktrace/libstacktrace.so \
	../base/libbrunobase.so \
	peripheral/libbrunoperipheral.so

install-libs: all
	mkdir -p $(INCLUDEDIR)/sysmgr $(LIBDIR)/pkgconfig
	$(INSTALL) -m 0644 platform_peripheral_api.h $(INCLUDEDIR)/sysmgr/
	$(INSTALL) -m 0755 peripheral/libbrunoperipheral.so $(LIBDIR)/

peripheral/libbrunoperipheral.so: peripheral/all

clean: $(addsuffix /clean,$(DIRS))
	$(RM) *~ *.o $(TARGETS)

%: %.o
	@echo "LINK $@"
	$(Q)$(CXX) $(CFLAGS) $(CXXFLAGS) $(LDFLAGS) $(INCS) \
		-o $@ $^ -lpthread

%.o: %.cc
	@echo " CXX $@"
	$(Q)$(CXX) $(CFLAGS) $(CXXFLAGS) $(INCS) -o $@ -c $<

%/all:
	$(MAKE) -C $* all

%/clean:
	$(MAKE) -C $* clean

%/install:
	$(MAKE) -C $* install
