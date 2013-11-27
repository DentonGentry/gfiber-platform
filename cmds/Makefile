default: all

PREFIX=/
BINDIR=$(DESTDIR)$(PREFIX)/bin
LIBDIR=$(DESTDIR)$(PREFIX)/lib

TARGETS=\
	alivemonitor \
	balloon \
	buttonmon \
	isoping \
	chg_mod_own \
	dir-monitor \
	diskbench \
	ionice \
	mcastreceive \
	memwatcher \
	logos \
	randomdata \
	realtime \
	rtwatcher \
	setuid \
	soft_rc \
	stun \
	usleep \
	wait-until-created \
	watch-dir
HOST_TARGETS=$(addprefix host-,$(TARGETS))
LIB_TARGETS=\
	stdoutline.so

ifeq ($(BRUNO_ARCH),mips)
ARCH_TARGETS = bigbin
endif

AS=$(CROSS_COMPILE)as
CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
HOST_CC ?= cc
HOST_CXX ?= g++
CFLAGS += -Wall -Wextra -Wswitch-enum -Werror -Wno-unused-parameter \
	-g -O -std=c99 -D_GNU_SOURCE
CXXFLAGS += -Wall -Wextra -Wswitch-enum -Werror -Wno-unused-parameter \
	-g -O -std=gnu++0x -D_GNU_SOURCE
INCS=-I../libstacktrace

.PRECIOUS: %.o host-%.o

all: $(TARGETS) $(HOST_TARGETS) $(LIB_TARGETS) $(ARCH_TARGETS)

install:
	mkdir -p $(BINDIR) $(LIBDIR)
	cp $(TARGETS) $(ARCH_TARGETS) $(BINDIR)
	cp $(LIB_TARGETS) $(LIBDIR)

install-libs:
	@echo "No libs to install."


%.o: %.c
	$(CC) $(CFLAGS) $(INCS) -o $@ -c $<

%.po: %.c
	$(CC) $(CFLAGS) -fPIC $(INCS) -o $@ -c $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(INCS) -o $@ -c $<

%.po: %.cc
	$(CXX) $(CXXFLAGS) -fPIC $(INCS) -o $@ -c $<

host-%.o: %.c
	$(HOST_CC) $(CFLAGS) $(INCS) -DCOMPILE_FOR_HOST=1 -o $@ -c $<

host-%.o: %.cc
	$(HOST_CXX) $(CXXFLAGS) $(INCS) -DCOMPILE_FOR_HOST=1 -o $@ -c $<

%: %.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

host-%: host-%.o
	$(HOST_CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

%: %.py
	ln -s $< $@

host-%: %.py
	ln -s $< $@

%.so: %.po
	$(CC) $(CFLAGS) $(INCS) -o $@ -fPIC -shared -Wl,-soname,$@ $<


host-isoping isoping: LIBS+=-lrt
host-diskbench diskbench: LIBS+=-lpthread -lrt
host-logos logos: LIBS+=-lrt
logos: LIBS+=-L../libstacktrace -lstacktrace
host-rtwatcher rtwatcher: LIBS+=-lpthread -lrt
host-realtime realtime: LIBS+=-lpthread
host-alivemonitor alivemonitor: LIBS+=-lrt
host-buttonmon buttonmon: LIBS+=-lrt
alivemonitor: alivemonitor.o
isoping: isoping.o
diskbench: diskbench.o
logos: logos.o
realtime: realtime.o
rtwatcher: rtwatcher.o
bigbin: bigbin.o bigtext.o
buttonmon: buttonmon.o

bigtext.o: bigtext.S
	 $(AS) -mabi=32 -mips32 -EL -o $@ $^

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
	rm -f *.o $(TARGETS) $(HOST_TARGETS) $(ARCH_TARGETS) *~ .*~ */*.pyc test_file
	rm -rf test_dir
