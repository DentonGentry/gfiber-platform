# Copyright 2011 Google Inc. All Rights Reserved.
# Author: dgentry@google.com (Denton Gentry)

CROSS_PREFIX:=$(TARGET)
CC:=$(CROSS_PREFIX)gcc
CPP:=$(CROSS_PREFIX)g++
LD:=$(CROSS_PREFIX)ld
AR:=$(CROSS_PREFIX)ar
RANLIB:=$(CROSS_PREFIX)ranlib
STRIP:=$(CROSS_PREFIX)strip

CFLAGS = -g -Os

ALL: hnvram
SRCS = hnvram_main.c
INCS =

hnvram: $(SRCS) $(INCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@ -lhmxupgrade

unit_test: test
test: hnvram_test
	./hnvram_test

hnvram_test: hnvram_test.cc hnvram_main.c $(INCS)
	$(CPP) $(CFLAGS) hnvram_test.cc -o $@ -lgtest -lhmxupgrade

clean:
	rm -f hnvram hnvram_test *.o
