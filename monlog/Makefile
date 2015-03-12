# Copyright 2015 Google Inc. All Rights Reserved.
# Author: hunguyen@google.com (Huy Nguyen)

default:

PREFIX=/usr
BINDIR=$(DESTDIR)$(PREFIX)/bin
PYTHON?=python
INSTALL=install

all:

install:
	mkdir -p $(BINDIR)
	$(INSTALL) -m 0755 monlog_pusher.py $(BINDIR)/monlog_pusher

install-libs:
	@echo "No libs to install."

test:
	$(PYTHON) monlog_pusher_test.py
lint:
	gpylint monlog_pusher.py monlog_pusher_test.py
clean:
	rm -rf *.pyc
