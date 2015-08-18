# Copyright 2012 Google Inc. All Rights Reserved.
# Author: kedong@google.com (Ke Dong)

PYTHON?=python
AS=$(CROSS_COMPILE)as
CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
LD=$(CROSS_COMPILE)gcc
RM=rm -f
INSTALL=install

all: readverity

CFLAGS+=-Wall -Wno-unknown-pragmas -Wimplicit $(EXTRACFLAGS)
CXXFLAGS+=-Wall -Wno-unknown-pragmas $(EXTRACXXFLAGS)

readverity: readverity.o main.o
	$(CC) -o $@ $^ $(LDFLAGS)

readverity_test: readverity.o readverity_test.o
	$(CXX) -o $@ $^ $(LDFLAGS) $(CXXFLAGS) -lgtest -lpthread

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
	$(RM) readverity readverity_test
