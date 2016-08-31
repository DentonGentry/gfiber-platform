default:	all

BINARY = diags

TARGETS=$(BINARY)
INSTALL=install
PREFIX=$(DESTDIR)/usr
BINDIR=$(PREFIX)/sbin
LIBDIR=$(PREFIX)/lib
INCLUDEDIR=$(PREFIX)/include

CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
RM=rm -f

CFLAGS = -Wall -Werror -Wimplicit -Wno-unknown-pragmas -D_GNU_SOURC -g
CFLAGS += $(EXTRA_CFLAGS)
LDFLAGS += $(EXTRA_LDFLAGS)

IFLAGS += $(patsubst %,-I%,$(INC_DIRS))
CFILES = $(wildcard ../common/*.c *.c)
OFILES = $(patsubst %.c,%.o,$(CFILES))

all:	$(TARGETS)

install:
	$(INSTALL) -m 0755 $(BINARY) $(BINDIR)/

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
