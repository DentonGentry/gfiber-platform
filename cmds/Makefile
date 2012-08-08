default: all

PREFIX=/
BINDIR=$(DESTDIR)$(PREFIX)/bin

TARGETS=grep wait-until-created randomdata watch-dirs
HOST_TARGETS=$(addprefix host-,$(TARGETS))

HOST_CC ?= cc
CFLAGS += -Wall -Wextra -Wswitch-enum -Werror -g -O
.PRECIOUS: %.o host-%.o

all: $(TARGETS) $(HOST_TARGETS)

install:
	mkdir -p $(BINDIR)
	cp $(TARGETS) $(BINDIR)

install-libs:
	@echo "No libs to install."

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

host-%.o: %.c
	$(HOST_CC) $(CFLAGS) -o $@ -c $<

%: %.o
	$(HOST_CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

host-%: host-%.o
	$(HOST_CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

TESTS=$(wildcard test*.sh)
runtests: all $(TESTS)
	set -e; \
	for d in $(TESTS); do \
		echo Running $$d; \
		./$$d; \
	done

test:
	./wvtest/wvtestrun $(MAKE) runtests

clean:
	rm -f *.o $(TARGETS) $(HOST_TARGETS) *~
