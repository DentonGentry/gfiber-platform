INSTALL?=install
BINDIR=$(DESTDIR)/bin
LIBDIR=$(DESTDIR)/usr/wifi
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
	PYTHONPATH=..:$(PYTHONPATH) ./$<

runtests: \
	$(patsubst %_test.py,%.test,$(wildcard *_test.py))

lint: $(filter-out options.py,$(wildcard *.py))
	$(GPYLINT) $^

test_only: all
	./wvtest/wvtestrun $(MAKE) runtests

# Use a submake here, only because otherwise GNU make (3.81) will not print
# an error about 'test' itself failing if one of the two sub-targets fails.
# Without such output, 'lint' could fail long before test_only fails, and
# the test_only output could scroll off the top of the screen, leaving the
# misleading impression that everything tested successfully.
test: all
	$(MAKE) test_only lint

install:
	mkdir -p $(LIBDIR) $(BINDIR)
	$(INSTALL) -m 0644 *.py $(LIBDIR)/
	$(INSTALL) -m 0755 wifi.py $(LIBDIR)/
	rm -f $(BINDIR)/wifi
	ln -s /usr/wifi/wifi.py $(BINDIR)/wifi

install-libs:
	@echo "No libs to install."

clean:
	rm -rf *~ .*~ *.pyc *.tmp */*~
