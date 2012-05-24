# Copyright 2011 Google Inc. All Rights Reserved.
# Author: kedong@google.com (Ke Dong)

CXX:=$(CROSS_COMPILE)g++
AR:=$(CROSS_COMPILE)ar
RM:=rm -f
LIBNAME=brunobase

all: ../build/lib$(LIBNAME).so ../build/lib$(LIBNAME).a

CFLAGS=-Wall -fPIC -Os
CPPFLAGS=-DPOSIX -DLINUX -D_DEBUG -DLOGGING=1
SRCS=$(shell ls *.cc)

%.o : %.cc
	$(CXX) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

../build/lib$(LIBNAME).so : $(SRCS:.cc=.o)
	$(CXX) -shared -Wl,-soname,lib$(LIBNAME).so -o $@ $^

../build/lib$(LIBNAME).a : $(SRCS:.cc=.o)
	$(AR) rcs lib$(LIBNAME).a $^

clean:
	$(RM) *.o *.so *.a
