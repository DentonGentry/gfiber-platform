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
CFLAGS = -Wall -Wimplicit -Wno-unknown-pragmas -W -std=c99 -D_GNU_SOURCE

CFLAGS += $(EXTRA_CFLAGS)
LDFLAGS += $(EXTRA_LDFLAGS)

# enable the platform we're supporting
ifeq ($(BR2_PACKAGE_BCM_NEXUS),y)
  CFLAGS += -DBROADCOM
  NOSTUB=1
endif
ifeq ($(BR2_PACKAGE_MINDSPEED_DRIVERS),y)
  CFLAGS += -DMINDSPEED
  NOSTUB=1
endif

ifeq ($(BR2_TARGET_GOOGLE_PLATFORM),gfiberlt)
  CFLAGS += -DGFIBER_LT
  NOSTUB=1
endif

ifndef NOSTUB
  CFLAGS += -DSTUB
  LDFLAGS += -lm
endif

CFLAGS += -g

IFLAGS += $(patsubst %,-I%,$(INC_DIRS))

CFILES = $(wildcard ../common/*.c *.c)
OFILES = $(patsubst %.c,%.o,$(CFILES))

all:	$(TARGETS)

install:
	$(INSTALL) -m 0755 diags $(BINDIR)/

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
