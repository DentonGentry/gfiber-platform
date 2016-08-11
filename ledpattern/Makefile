default:

ETCDIR=$(DESTDIR)/etc
BINDIR=$(DESTDIR)/bin
PYTHON?=python

all:

install:
	mkdir -p $(BINDIR)
	cp ledpattern.py $(BINDIR)/ledpattern
	cp ledtapcode.sh $(BINDIR)/ledtapcode
	cp ledpatterns $(ETCDIR)/ledpatterns
	chmod +x $(BINDIR)/ledtapcode

install-libs:
	@echo "No libs to install."

test: lint
	set -e; \
	for pytest in $(wildcard *_test.py); do \
		echo; \
		echo "Testing $$pytest"; \
		$(PYTHON) $$pytest; \
	done

clean:
	rm -rf *.pyc

lint:
	gpylint ledpattern.py ledpattern_test.py
