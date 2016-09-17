default: all

INSTALL?=install
BINDIR=$(DESTDIR)/bin
LIBDIR=$(DESTDIR)/usr/bouncer
GPYLINT=$(shell \
    if which gpylint >/dev/null; then \
      echo gpylint; \
    else \
      echo 'echo "(gpylint-missing)" >&2'; \
    fi \
)
NOINSTALL=options.py

all:

install:
	mkdir -p $(LIBDIR) $(BINDIR)
	$(INSTALL) -m 0644 $(filter-out $(NOINSTALL) $(TARGETS), $(wildcard *.py)) $(LIBDIR)/
	for t in authorizer hash_mac_addr http_bouncer; do \
		$(INSTALL) -m 0755 $$t.py $(LIBDIR)/; \
		ln -fs /usr/bouncer/$$t.py $(BINDIR)/$$t; \
	done

install-libs:
	@echo "No libs to install."

TESTS = $(wildcard test-*.sh) $(wildcard test-*.py) $(wildcard *_test.py)
runtests: all $(TESTS)
	set -e; \
	for d in $(TESTS); do \
		echo Running $$d; \
		./$$d; \
	done

lint: $(filter-out options.py,$(wildcard *.py))
	$(GPYLINT) $^

test: all $(TESTS)
	./wvtest/wvtestrun $(MAKE) runtests

clean:
	rm -f *.o $(TARGETS) \
		$(HOST_TARGETS) \
		*~ .*~ */*.pyc test_file *.pb.* *.tmp.*
	rm -rf test_dir
