default:

PREFIX=/
BINDIR=$(DESTDIR)$(PREFIX)/bin
PYTHON?=python

all:

install:
	mkdir -p $(BINDIR)
	cp ginstall.py $(BINDIR)
	ln -sf ginstall.py $(BINDIR)/ginstall

install-libs:
	@echo "No libs to install."

test:
	set -e; \
	for d in $(wildcard *_test.py); do \
		$(PYTHON) $$d; \
	done
	./wvtest/wvtestrun ./install_test.sh

clean:
	rm -f *~ testdata/dev/* testdata/bin.tmp/*
