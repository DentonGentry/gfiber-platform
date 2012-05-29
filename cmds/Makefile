default: all

PREFIX=/
BINDIR=$(DESTDIR)$(PREFIX)/bin

TARGETS=grep
HOST_TARGETS=$(addprefix host-,$(TARGETS))

HOST_CC ?= cc
CFLAGS += -Wall
.PRECIOUS: %.o host-%.o

all: $(TARGETS) $(HOST_TARGETS)

install:
	mkdir -p $(BINDIR)
	cp $(TARGETS) $(BINDIR)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

host-%.o: %.c
	$(HOST_CC) $(CFLAGS) -o $@ -c $<

%: %.o
	$(HOST_CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

host-%: host-%.o
	$(HOST_CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

runtests: $(wildcard test*.sh)
	set -e; \
	for d in $^; do \
		echo Running $$d; \
		./$$d; \
	done

test:
	./wvtest/wvtestrun $(MAKE) runtests

clean:
	rm -f *.o $(TARGETS) $(HOST_TARGETS) *~
