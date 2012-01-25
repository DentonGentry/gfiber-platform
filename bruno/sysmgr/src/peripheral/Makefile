# Copyright 2011 Google Inc. All Rights Reserved.
# Author: kedong@google.com (Ke Dong)

CXX:=$(CROSS_COMPILE)g++
AR:=$(CROSS_COMPILE)ar
RM:=rm -f
LIBNAME=brunoperipheral

all: ../../build/lib$(LIBNAME).so

CFLAGS:=-fPIC -Wall -W
CPPFLAGS=-I../../include $(subst -Wstrict-prototypes,,$(subst -std=c99,,$(shell $(PKG_CONFIG) --cflags nexus brunobase)))
SRCS=$(shell ls *.cc)

%.o : %.cc
	$(CXX) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

../../build/lib$(LIBNAME).so : $(SRCS:.cc=.o)
	$(CXX) -shared -Wl,-soname,lib$(LIBNAME).so -Wl,-export-dynamic -o $@ $^

clean:
	$(RM) *.o *.so *.a
