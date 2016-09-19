default: all

BINDIR=$(DESTDIR)/bin
GPYLINT=$(shell \
    if which gpylint >/dev/null; then \
      echo gpylint; \
    else \
      echo 'echo "(gpylint-missing)" >&2'; \
    fi \
)

TARGETS=authorizer hash_mac_addr http_bouncer

HOST_TARGETS=$(addprefix host-,$(TARGETS))

all: $(TARGETS) $(HOST_TARGETS)

install:
	mkdir -p $(BINDIR)
	cp $(TARGETS) $(BINDIR)

install-libs:
	@echo "No libs to install."

%: %.py
	ln -s $< $@

host-%: %.py
	ln -s $< $@

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
