# Copyright 2012 Google Inc. All Rights Reserved.
# Author: irinams@google.com (Irina Stanescu)
CXX=$(CROSS_COMPILE)g++
RM=rm -f
INSTALL=install
PREFIX=/usr
LIBDIR=$(DESTDIR)$(PREFIX)/lib
INCLUDEDIR=$(DESTDIR)$(PREFIX)/include

all: libstacktrace.so

CFLAGS=-Wall -fPIC -Os -Wextra -Werror -Wswitch-enum
CPPFLAGS=-DPOSIX -DLINUX -D_DEBUG -DLOGGING=1
OBJS=$(patsubst %.c,%.o,$(wildcard *.c))

%.o: %.cc
	$(CXX) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

libstacktrace.so: $(OBJS)
	$(CXX) -shared -Wl,-soname,libstacktrace.so -o $@ $^

install: all
	echo 'target-install=$(INSTALL)'
	mkdir -p $(LIBDIR)
	$(INSTALL) -m 0755 libstacktrace.so $(LIBDIR)/

install-libs: all
	echo 'staging-install=$(INSTALL)'
	mkdir -p $(INCLUDEDIR) $(LIBDIR) $(LIBDIR)/pkgconfig
	$(INSTALL) -m 0644 *.h $(INCLUDEDIR)/
	$(INSTALL) -m 0755 libstacktrace.so $(LIBDIR)/

test:
	@echo "Nothing to test."

clean:
	$(RM) *.[oa] *.so *~
