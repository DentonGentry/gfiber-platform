# Copyright 2011 Google Inc. All Rights Reserved.
# Author: kedong@google.com (Ke Dong)

CXX:=$(CROSS_COMPILE)g++
RM:=rm -f

SRCS:=$(shell ls *.cc)
EXES:=$(addprefix ../build/,$(SRCS:.cc=))

all: $(EXES)

CFLAGS:=-Wall -Wimplicit -Wno-unknown-pragmas -W
CPPFLAGS=-I../include -I../src/peripheral $(subst -Wstrict-prototypes,,$(subst -std=c99,,$(shell $(PKG_CONFIG) --cflags brunobase nexus)))
LDFLAGS=-L../build -lbrunoperipheral $(shell $(PKG_CONFIG) --libs brunobase nexus)

../build/% : %.cc
	$(CXX) $(CFLAGS) $(LDFLAGS) $(CPPFLAGS) -o $@ $<

clean:
	$(RM) $(EXES)
