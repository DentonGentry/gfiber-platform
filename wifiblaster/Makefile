# Copyright 2015 Google Inc. All Rights Reserved.
# Author: mikemu@google.com (Mike Mu)

default:

PREFIX=/usr
BINDIR=$(DESTDIR)$(PREFIX)/bin
PYTHON?=python

all:

install:
	mkdir -p $(BINDIR)
	cp -f wifiblaster.py $(BINDIR)/wifiblaster
	chmod 755 $(BINDIR)/wifiblaster

install-libs:
	@echo "No libs to install."

test:
	$(PYTHON) wifiblaster_test.py

lint:
	gpylint wifiblaster.py wifiblaster_test.py

clean:
	rm -rf *.pyc
