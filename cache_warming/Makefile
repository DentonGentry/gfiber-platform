INSTALL?=install
PREFIX=$(DESTDIR)/bin
LIBDIR=$(PREFIX)/cache_warming
PYTHON?=python
GPYLINT=$(shell \
    if which gpylint >/dev/null; then \
      echo gpylint; \
    else \
      echo 'echo "(gpylint-missing)" >&2'; \
    fi \
)

all:

%.test: %_test.py
	$(PYTHON) $<

runtests: $(patsubst %_test.py,%.test,$(wildcard *_test.py))

lint: $(filter-out options.py,$(wildcard *.py))
	$(GPYLINT) $^

test_only: all
	PYTHONPATH=..:$(PYTHONPATH) ./wvtest/wvtestrun $(MAKE) runtests

test: all
	$(MAKE) test_only lint

install:
	mkdir -p $(LIBDIR)
	$(INSTALL) -m 0755 -D cache_warming.py $(LIBDIR)/

install-libs:
	@echo "No libs to install."

clean:
	rm -rf *~ .*~ *.pyc
