# Copyright 2011 Google Inc. All Rights Reserved.
# Author: dgentry@google.com (Denton Gentry)

CC:=$(CROSS_COMPILE)gcc
CPP:=$(CROSS_COMPILE)g++
LD:=$(CROSS_COMPILE)ld
AR:=$(CROSS_COMPILE)ar
RANLIB:=$(CROSS_COMPILE)ranlib
STRIP:=$(CROSS_COMPILE)strip
PREFIX=/usr
BINDIR=$(DESTDIR)$(PREFIX)/bin

HUMAX_UPGRADE_DIR ?= ../../../humax/misc/libupgrade
CFLAGS += -g -Os -I$(HUMAX_UPGRADE_DIR) -I$(HUMAX_UPGRADE_DIR)/test $(EXTRACFLAGS)
LDFLAGS += -L$(HUMAX_UPGRADE_DIR) $(EXTRALDFLAGS)

all: hnvram
SRCS = hnvram_main.c
INCS =

hnvram: $(SRCS) $(INCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(LDFLAGS) -lhmxupgrade

unit_test: test
test: clean hnvram_unit_test hnvram_integration_test
	./hnvram_unit_test
	./hnvram_integration_test

hnvram_unit_test: hnvram_test.cc hnvram_main.c $(INCS)
	$(CPP) $(CFLAGS) hnvram_test.cc -o $@ $(LDFLAGS) -lgtest -lpthread

hnvram_integration_test: hnvram_integration_test.cc
	$(CPP) $(CFLAGS) hnvram_integration_test.cc -o $@ $(LDFLAGS) -lgtest -lpthread

install:
	mkdir -p $(BINDIR)
	cp hnvram $(BINDIR)/hnvram_binary

install-libs:
	@echo "No libs to install."

clean:
	rm -f hnvram hnvram_unit_test hnvram_integration_test *.o
