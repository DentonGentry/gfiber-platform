INSTALL?=install
PREFIX=$(DESTDIR)/usr
BINDIR=$(PREFIX)/bin
LIBDIR=$(PREFIX)/lib

all:

%.test: %_test.py
	./$<

runtests: $(patsubst %_test.py,%.test,$(wildcard *_test.py))

test: all
	./wvtest/wvtestrun $(MAKE) runtests

install:
	mkdir -p $(LIBDIR) $(BINDIR)
	$(INSTALL) -m 0755 waveguide.py $(BINDIR)/waveguide

install-libs:
	@echo "No libs to install."

clean:
	rm -rf *~ .*~ *.pyc
