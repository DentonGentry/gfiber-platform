# Copyright 2011 Google Inc. All Rights Reserved.
# Author: kedong@google.com (Ke Dong)

CXX:=$(CROSS_COMPILE)g++
RM:=rm -f
INCS:=-I../peripheral -I../../include
EXES:=../../build/sysmgr

all: $(EXES)

CFLAGS:=-Wall -Wimplicit -Wno-unknown-pragmas -W
CPPFLAGS=$(subst -Wstrict-prototypes,,$(subst -std=c99,,$(shell $(PKG_CONFIG) --cflags nexus brunobase)))
LDFLAGS=-L../../build -lbrunoperipheral $(shell $(PKG_CONFIG) --libs nexus brunobase)
SRCS=$(shell ls *.cc)

$(EXES) : $(SRCS)
	$(CXX) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(INCS) -o $@ $^

clean:
	$(RM) *.o $(EXES)
