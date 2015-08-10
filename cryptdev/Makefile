PREFIX=/
BINDIR=$(DESTDIR)$(PREFIX)/bin
INSTALL=install
PKG_CONFIG?=pkg-config
CC=$(CROSS_COMPILE)gcc

CFLAGS = -Wall -O2 $(shell $(PKG_CONFIG) --cflags dbus-1) -D_LARGEFILE64_SOURCE
LDFLAGS = $(shell $(PKG_CONFIG) --libs dbus-1) -ldevmapper

APPS := cryptdev
SRCS := cryptdev.c tpm.c keyfile.c loopdev.c devmap.c
OBJS := $(patsubst %.c,%.o,$(SRCS))

all: $(APPS)

cryptdev : $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o : %.c %.h
	$(CC) -c $(CFLAGS) $< -o $@

install: all
	echo 'target-install=$(INSTALL)'
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) $(APPS) $(BINDIR)

install-libs: all
	@echo "No libs to install."

test:
	@echo "Nothing to test."

.PHONY: clean
clean:
	$(RM) $(APPS) *.o
