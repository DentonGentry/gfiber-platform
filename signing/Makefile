# Copyright 2012 Google Inc. All Rights Reserved.
# Author: kedong@google.com (Ke Dong)

PYTHON?=python
CC:=$(CROSS_COMPILE)gcc
RM:=rm -f
EXES=readverity readverity_test
INSTALL=install

all: $(EXES)

CFLAGS:=-Wall -Wimplicit -Wno-unknown-pragmas
HOST_CFLAGS=-I/include -I/usr/include -I$(HOSTDIR)/include -I$(HOSTDIR)/usr/include
HOST_LDFLAGS=-L$(HOSTDIR)/lib -L$(HOSTDIR)/usr/lib -lgtest -lpthread

readverity: readverity.o main.o
	$(CC) -o $@ $^ $(CFLAGS)

host_%.o: %.c
	gcc -c -o $@ $< $(CFLAGS) $(HOST_CFLAGS)

host_%.o: %.cc
	g++ -c -o $@ $< $(CFLAGS) $(HOST_CFLAGS)

readverity_test: host_readverity.o host_readverity_test.o
	g++ -o $@ $^ $(HOST_LDFLAGS)

install-libs: readverity
	mkdir -p $(DESTDIR)/usr/sbin/
	$(INSTALL) -D -m 0755 repack.py signserial.py \
		$(DESTDIR)/usr/sbin/

install:
	mkdir -p $(DESTDIR)/usr/sbin $(DESTDIR)/usr/bin
	$(INSTALL) -D -m 0755 readverity \
		$(DESTDIR)/usr/sbin/
	$(INSTALL) -D -m 0755 readallfiles.py \
		$(DESTDIR)/usr/bin/readallfiles
	$(INSTALL) -D -m 0750 S99readallfiles \
		$(DESTDIR)/etc/init.d/S99readallfiles

test: readverity_test
	$(PYTHON) repacktest.py
	./readverity_test && \
		$(PYTHON) readallfilestest.py

clean:
	$(RM) $(EXES)
