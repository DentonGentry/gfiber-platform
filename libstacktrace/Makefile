# Copyright 2012 Google Inc. All Rights Reserved.
# Author: irinams@google.com (Irina Stanescu)
CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
RM=rm -f
INSTALL=install
PREFIX=/usr
LIBDIR=$(DESTDIR)$(PREFIX)/lib
INCLUDEDIR=$(DESTDIR)$(PREFIX)/include

all: libstacktrace.so

CFLAGS=-Wall -fPIC -Os -Wextra -Werror -Wswitch-enum $(EXTRACFLAGS)
CXXFLAGS=-Wall -fPIC -Os -Wextra -Werror -Wswitch-enum -std=c++0x $(EXTRACFLAGS)
CPPFLAGS=-DPOSIX -DLINUX -D_DEBUG -DLOGGING=1 $(EXTRACFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@
%.o: %.cc
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) $< -o $@

libstacktrace.so: stacktrace.o
	$(CC) -shared -Wl,-soname,libstacktrace.so -Wl,-export-dynamic -o $@ $^

stacktrace_test: stacktrace.o stacktrace_test.o
	$(CXX) -o $@ $^ $(LDFLAGS) $(CPPFLAGS) -lgtest -lpthread

install: all
	echo 'target-install=$(INSTALL)'
	mkdir -p $(LIBDIR)
	$(INSTALL) -m 0755 libstacktrace.so $(LIBDIR)/

install-libs: all
	echo 'staging-install=$(INSTALL)'
	mkdir -p $(INCLUDEDIR) $(LIBDIR) $(LIBDIR)/pkgconfig
	$(INSTALL) -m 0644 *.h $(INCLUDEDIR)/
	$(INSTALL) -m 0755 libstacktrace.so $(LIBDIR)/

test: stacktrace_test
	./stacktrace_test

clean:
	$(RM) *.[oa] *.so *~
