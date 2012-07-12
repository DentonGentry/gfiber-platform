# Copyright 2012 Google Inc. All Rights Reserved.
# Author: kedong@google.com (Ke Dong)

CC:=$(CROSS_COMPILE)gcc
RM:=rm -f
EXES=readverity readverity_test

all: $(EXES)

CFLAGS:=-Wall -Wimplicit -Wno-unknown-pragmas
HOST_CFLAGS=-I/include -I/usr/include -I$(HOST_DIR)/include -I$(HOST_DIR)/usr/include
HOST_LDFLAGS=-L$(HOST_DIR)/lib -L$(HOST_DIR)/usr/lib -lgtest -lpthread

readverity: readverity.o main.o
	$(CC) -o $@ $^ $(CFLAGS)

host_%.o: %.c
	gcc -c -o $@ $< $(CFLAGS) $(HOST_CFLAGS)

host_%.o: %.cc
	g++ -c -o $@ $< $(CFLAGS) $(HOST_CFLAGS)

readverity_test: host_readverity.o host_readverity_test.o
	g++ -o $@ $^ $(HOST_LDFLAGS)

install: readverity
	$(INSTALL) -D -m 0755 readverity \
		$(TARGET_DIR)/usr/sbin/readverity &&\
	$(INSTALL) -D -m 0755 readallfiles.py \
		$(TARGET_DIR)/usr/bin/readallfiles
	$(INSTALL) -D -m 0750 S99readallfiles \
		$(TARGET_DIR)/etc/init.d/S99readallfiles

test: readverity_test
	./readverity_test && \
		$(HOST_DIR)/usr/bin/python readallfilestest.py

clean:
	$(RM) $(EXES)
