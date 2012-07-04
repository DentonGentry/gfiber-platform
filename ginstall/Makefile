default:

PREFIX=/
BINDIR=$(DESTDIR)$(PREFIX)/bin

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
		$(HOSTDIR)/usr/bin/python $$d; \
	done

clean:
	rm -f *~
