# Copyright 2012 Google Inc. All Rights Reserved.
# Author: weixiaofeng@google.com (Xiaofeng Wei)

CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
CFLAGS := -fPIC -Os -Wall -I../include

all: libsysvar.so

libsysvar.so: sysvar.o sysvar_lib.o
	$(CC) -shared -Wl,-soname,libsysvar.so -Wl,-export-dynamic -o $@ $^

test:
	@echo "Nothing to test."

clean:
	rm -f *.o *.so *~

