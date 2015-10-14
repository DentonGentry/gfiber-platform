default:	all

TARGETS=$(BINARY)
INSTALL=install
PREFIX=$(DESTDIR)/usr
BINDIR=$(PREFIX)/bin
LIBDIR=$(PREFIX)/lib
INCLUDEDIR=$(PREFIX)/include

CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
RM=rm -f
CFLAGS = -Wall -Werror -Wimplicit -Wno-unknown-pragmas -D_GNU_SOURCE

CFLAGS += $(EXTRA_CFLAGS)
LDFLAGS += $(EXTRA_LDFLAGS)

CFLAGS += -g

IFLAGS += $(patsubst %,-I%,$(INC_DIRS))
CFILES = $(wildcard *.c)
OFILES = $(patsubst %.c,%.o,$(CFILES))

all:	$(OFILES)

install:

install-libs:
	@:

test:
	@:

%.o:	%.c
	$(CC) $(CFLAGS) $(IFLAGS) -c $^ -c

clean:
	$(RM) $(OFILES) $(BINARY)
