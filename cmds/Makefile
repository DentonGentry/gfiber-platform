default: all

PREFIX=/
BINDIR=$(DESTDIR)$(PREFIX)/bin

TARGETS=grep wait-until-created randomdata watch-dirs logos realtime rtwatcher
HOST_TARGETS=$(addprefix host-,$(TARGETS))

HOST_CC ?= cc
CFLAGS += -Wall -Wextra -Wswitch-enum -Werror -Wno-unused-parameter \
	-g -O -std=c99 -D_GNU_SOURCE
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
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

host-%: host-%.o
	$(HOST_CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

host-logos logos: LIBS+=-lrt
host-rtwatcher rtwatcher: LIBS+=-lpthread -lrt
host-realtime realtime: LIBS+=-lpthread
logos: logos.o
realtime: realtime.o
rtwatcher: rtwatcher.o

TESTS=$(wildcard test-*.sh) $(wildcard test-*.py)
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
