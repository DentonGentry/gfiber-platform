default:	all

BINARY = diags

TARGETS=$(BINARY)
INSTALL=install
PREFIX=$(DESTDIR)/usr
BINDIR=$(PREFIX)/bin
LIBDIR=$(PREFIX)/lib
INCLUDEDIR=$(PREFIX)/include

CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
RM=rm -f

CFLAGS = -Wall -Werror -Wimplicit -Wno-unknown-pragmas -D_GNU_SOURCE -g
CFLAGS += $(EXTRA_CFLAGS)
LDFLAGS += $(EXTRA_LDFLAGS)

IFLAGS += $(patsubst %,-I%,$(INC_DIRS))
CFILES = $(wildcard *.c ../common/*.c)
OFILES = $(patsubst %.c,%.o,$(CFILES))

all:	$(TARGETS)

install:
	$(INSTALL) -m 0755 diags $(BINDIR)/
	$(INSTALL) -m 0755 signal_monitor $(BINDIR)/
	$(INSTALL) -m 0755 tpmstartup $(BINDIR)/
	$(INSTALL) -m 0755 tuner_test $(BINDIR)/
	$(INSTALL) -m 0755 udp_send.py $(BINDIR)/udp_send
	$(INSTALL) -m 0755 usb-diags $(BINDIR)/

install-libs:
	@:

test:
	@:

$(BINARY):	$(OFILES)
	$(CC) $^ $(LDFLAGS) -o $@

%.o:	%.c
	$(CC) $(CFLAGS) $(IFLAGS) -c $^ -c

clean:
	$(RM) $(OFILES) $(BINARY)
