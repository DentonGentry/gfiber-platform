default:

PYTHON?=python
PREFIX=/
BINDIR=$(DESTDIR)$(PREFIX)/usr/bin

all:

install:
	mkdir -p $(BINDIR)
	cp -f antirollback.py $(BINDIR)/antirollback
	chmod 555 $(BINDIR)/antirollback

install-libs:
	@echo "No libs to install."

test:
	set -e; \
	for d in $(wildcard *_test.py); do \
		$(PYTHON) $$d; \
	done

clean:
	rm -f *~
