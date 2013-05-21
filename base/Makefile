# Copyright 2011 Google Inc. All Rights Reserved.
# Author: kedong@google.com (Ke Dong)

CXX=$(CROSS_COMPILE)g++
AR=$(CROSS_COMPILE)ar
RM=rm -f
INSTALL=install
PREFIX=/usr
LIBDIR=$(DESTDIR)$(PREFIX)/lib
INCLUDEDIR=$(DESTDIR)$(PREFIX)/include/bruno

all: libbrunobase.so libbrunobase.a

CFLAGS=-Wall -fPIC -Os
CPPFLAGS=-DPOSIX -DLINUX -D_DEBUG -DLOGGING=1
OBJS=$(patsubst %.cc,%.o,$(wildcard *.cc))

%.o: %.cc
	$(CXX) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

libbrunobase.so: $(OBJS)
	$(CXX) -shared -Wl,-soname,libbrunobase.so -o $@ $^ -lrt

libbrunobase.a: $(OBJS)
	$(AR) rcs libbrunobase.a $^

install: all
	echo 'target-install=$(INSTALL)'
	mkdir -p $(LIBDIR)
	$(INSTALL) -m 0755 libbrunobase.so $(LIBDIR)/

install-libs: all
	echo 'staging-install=$(INSTALL)'
	mkdir -p $(INCLUDEDIR) $(LIBDIR) $(LIBDIR)/pkgconfig
	$(INSTALL) -m 0644 bruno/*.h $(INCLUDEDIR)/
	$(INSTALL) -m 0644 brunobase.pc $(LIBDIR)/pkgconfig/
	$(INSTALL) -m 0755 libbrunobase.so libbrunobase.a $(LIBDIR)/

test:
	@echo "Nothing to test."

clean:
	$(RM) *.[oa] *.so *~
