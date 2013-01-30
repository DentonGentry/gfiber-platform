default: all

PREFIX=/
BINDIR=$(DESTDIR)$(PREFIX)/bin

TARGETS=\
	dir-monitor \
	diskbench \
	grep \
	mcastreceive \
	logos \
	randomdata \
	realtime \
	rtwatcher \
	usleep \
	wait-until-created \
	watch-dir
HOST_TARGETS=$(addprefix host-,$(TARGETS))

CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
HOST_CC ?= cc
HOST_CXX ?= g++
CFLAGS += -Wall -Wextra -Wswitch-enum -Werror -Wno-unused-parameter \
	-g -O -std=c99 -D_GNU_SOURCE
CXXFLAGS += -Wall -Wextra -Wswitch-enum -Werror -Wno-unused-parameter \
	-g -O -std=gnu++0x -D_GNU_SOURCE
.PRECIOUS: %.o host-%.o

all: $(TARGETS) $(HOST_TARGETS)

install:
	mkdir -p $(BINDIR)
	cp $(TARGETS) $(BINDIR)

install-libs:
	@echo "No libs to install."

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) -o $@ -c $<

host-%.o: %.c
	$(HOST_CC) $(CFLAGS) -o $@ -c $<

host-%.o: %.cc
	$(HOST_CXX) $(CXXFLAGS) -o $@ -c $<

%: %.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

host-%: host-%.o
	$(HOST_CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)


host-diskbench diskbench: LIBS+=-lpthread -lrt
host-logos logos: LIBS+=-lrt
host-rtwatcher rtwatcher: LIBS+=-lpthread -lrt
host-realtime realtime: LIBS+=-lpthread
diskbench: diskbench.o
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
	rm -f *.o $(TARGETS) $(HOST_TARGETS) *~ .*~ */*.pyc test_file
	rm -rf test_dir
