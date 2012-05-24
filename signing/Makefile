# Copyright 2012 Google Inc. All Rights Reserved.
# Author: kedong@google.com (Ke Dong)

CC:=$(CROSS_COMPILE)gcc
CXX:=$(CROSS_COMPILE)g++
RM:=rm -f
EXES=readverity

all: $(EXES)

CFLAGS:=-Wall -Wimplicit -Wno-unknown-pragmas -W

readverity : readverity.c
	$(CC) -o $@ $< $(CFLAGS)

clean:
	$(RM) $(EXES)
