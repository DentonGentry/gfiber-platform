default:

PREFIX=/
BINDIR=$(DESTDIR)$(PREFIX)/bin
PYTHON?=python

all:

install:
	mkdir -p $(BINDIR)
	cp presterastats.py $(BINDIR)/presterastats
	cp prestera_periodic.py $(BINDIR)/prestera_periodic

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
	for n in $(filter-out options.py, $(wildcard *.py)); do \
		gpylint $$n || exit 1; \
	done
