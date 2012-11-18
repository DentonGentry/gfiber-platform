# Copyright 2012 Google Inc. All Rights Reserved.
# Author: shenglien@google.com (Sheng-Lien Lin)

CC		:= $(CROSS_COMPILE)gcc
AR		:= $(CROSS_COMPILE)ar
RM		:=rm -f
LIBNAME		=brunodiag

all: ./lib$(LIBNAME).so

CFLAGS		:= -fPIC -Os -Wall -pipe -fno-strict-aliasing -I../include -I../../libstacktrace
SRCS=$(shell ls *.c)

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

lib$(LIBNAME).so : $(SRCS:.c=.o)
	$(CC) -shared -Wl,-soname,lib$(LIBNAME).so -Wl,-export-dynamic -o $@ $^

clean:
	$(RM) *.o *.so *.a
