# Copyright 2011 Google Inc. All Rights Reserved.
# Author: kedong@google.com (Ke Dong)

CC:=$(CROSS_COMPILE)gcc
RM:=rm -f

SRCS:=$(shell ls *.c)
EXES:=$(addprefix ./,$(SRCS:.c=))

all: $(EXES)

CFLAGS+= -Os -Wall -pipe -fno-strict-aliasing -I../included -I../lib
LDFLAGS=-L../lib -lbrunodiag

%: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	$(RM) $(EXES)
