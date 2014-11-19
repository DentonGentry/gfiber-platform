INSTALL?=install
PREFIX=$(DESTDIR)/usr
BINDIR=$(PREFIX)/bin
LIBDIR=$(PREFIX)/waveguide

all:

%.test: %_test.py
	./$<

%.test: %_test.sh
	./$<

runtests: \
	$(patsubst %_test.py,%.test,$(wildcard *_test.py)) \
	$(patsubst %_test.sh,%.test,$(wildcard *_test.sh))

test: all
	./wvtest/wvtestrun $(MAKE) runtests

install:
	mkdir -p $(LIBDIR) $(BINDIR)
	$(INSTALL) -m 0644 *.py $(LIBDIR)/
	$(INSTALL) -m 0755 waveguide.py $(LIBDIR)/
	$(INSTALL) -m 0755 waveguide $(BINDIR)/

install-libs:
	@echo "No libs to install."

clean:
	rm -rf *~ .*~ *.pyc *.tmp */*~ fake/scanresults.*
