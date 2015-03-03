# Copyright 2012 Google Inc. All Rights Reserved.
# Author: weixiaofeng@google.com (Xiaofeng Wei)

default: all

DIRS := lib
TARGETS := sysvar_cmd $(addsuffix /all, $(DIRS))

CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
CFLAGS += -Wall -Wextra -Wswitch-enum -Werror -Wno-unused-parameter -g -O -I./include
LDFLAGS += -L./lib -lsysvar

PREFIX=$(DESTDIR)/usr
BINDIR=$(PREFIX)/bin
LIBDIR=$(PREFIX)/lib
INCDIR=$(PREFIX)/include
INSTALL=install

all: $(TARGETS)

sysvar_cmd: |lib/all

%.o: %.c
	@echo "# sysvar compile"
	$(CC) $(CFLAGS) -o $@ -c $<

%: %.o
	@echo "# sysvar link"
	$(CC) $(CFLAGS) -lsysvar -o $@ $^ $(LDFLAGS)

install:
	@echo "# sysvar install"
	mkdir -p $(BINDIR)
	$(INSTALL) -m 0755 sysvar_cmd $(BINDIR)
	mkdir -p $(LIBDIR)
	$(INSTALL) -m 0755 lib/libsysvar.so $(LIBDIR)/

install-libs:
	@echo "# sysvar install-libs"
	mkdir -p $(INCDIR)/sysvar
	$(INSTALL) -m 0644 include/*.h $(INCDIR)/
	mkdir -p $(LIBDIR)
	$(INSTALL) -m 0755 lib/libsysvar.so $(LIBDIR)/

test: all lib/test
	@echo "Nothing to test."

clean:
	rm -f *.o $(TARGETS) *~

%/all:
	$(MAKE) -C $* all

%/clean:
	$(MAKE) -C $* clean

%/install:
	$(MAKE) -C $* install

%/test:
	$(MAKE) -C $* test
