default:	all

BINARY = gpio-mailbox

TARGETS=$(BINARY)
INSTALL=install
PREFIX=$(DESTDIR)/usr
BINDIR=$(PREFIX)/bin
LIBDIR=$(PREFIX)/lib
INCLUDEDIR=$(PREFIX)/include

CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
RM=rm -f
CFLAGS = -Wall -Wextra -Werror -Wimplicit -Wno-unknown-pragmas -std=c99

#CFLAGS += $(shell $(PKG_CONFIG) --cflags brunobase)
#LDFLAGS += $(shell $(PKG_CONFIG) --libs brunobase)

CFLAGS += $(EXTRA_CFLAGS)
LDFLAGS += $(EXTRA_LDFLAGS)

# enable the platform we're supporting
ifeq ($(BR2_PACKAGE_BCM_NEXUS),y)
  CFLAGS += -DBROADCOM
else ifeq ($(BR2_PACKAGE_MINDSPEED_DRIVERS),y)
  CFLAGS += -DMINDSPEED
else ifeq ($(BR2_TARGET_GENERIC_PLATFORM_NAME),gflt110)
  CFLAGS += -DGFIBER_LT
else ifeq ($(BR2_TARGET_GENERIC_PLATFORM_NAME),gfmn100)
  CFLAGS += -DWINDCHARGER
else ifeq ($(BR2_TARGET_GENERIC_PLATFORM_NAME),gfch100)
  CFLAGS += -DGFCH100
else ifeq ($(BR2_TARGET_GENERIC_PLATFORM_NAME),kvm)
  CFLAGS += -DSTUB
  LDFLAGS += -lm
else
  $(error can't determine which pin implementation to use for gpio_mailbox)
endif

INC_DIRS += ../libstacktrace
LDFLAGS += -L../libstacktrace -lstacktrace -lrt

CFLAGS += -g

IFLAGS += $(patsubst %,-I%,$(INC_DIRS))

CFILES = $(wildcard *.c)
OFILES = $(patsubst %.c,%.o,$(CFILES))

ifndef BRUNO_ARCH
$(warning BRUNO_ARCH is undefined.  Set it to arm or mips or i386)
endif

all:	$(TARGETS)

install:
	$(INSTALL) -m 0755 gpio-mailbox $(BINDIR)/

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
