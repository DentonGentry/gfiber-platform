default:

PREFIX=/
BINDIR=$(DESTDIR)$(PREFIX)/bin
WWWDIR=$(DESTDIR)$(PREFIX)/usr/craftui
PYTHON?=python

all:

install:
	mkdir -p $(BINDIR) $(WWWDIR)
	cp craftui craftui.py $(BINDIR)
	cp -rp www $(WWWDIR)

install-libs:
	@echo "No libs to install."

test: lint
	set -e; \
	for n in $(wildcard ./*_test.*); do \
		echo; \
		echo "Testing $$n"; \
		$$n; \
	done

clean:
	rm -rf *.pyc

lint:
	for n in *.py; do gpylint $$n || exit 1; done
