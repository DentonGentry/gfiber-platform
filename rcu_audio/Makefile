CC:=$(CROSS_COMPILE)gcc
CPP:=$(CROSS_COMPILE)g++
HOST_PROTOC ?= $(HOSTDIR)/usr/bin/protoc
PREFIX=/usr
BINDIR=$(DESTDIR)$(PREFIX)/bin
CFLAGS += $(EXTRACFLAGS)
LDFLAGS += $(EXTRALDFLAGS)

BINARIES = ti-rcu-audio gfrm100-rcu-audio gfrm-voice-demo
CHECKING = -Wall -Werror
all: $(BINARIES)

%.pb.cc: %.proto
	echo "Building .pb.cc"
	$(HOST_PROTOC) --cpp_out=. $<

ti-rcu-audio: ti-rcu-audio.cc RAS_lib.c RAS_lib.h rcu-audio.h remote_control_audio.pb.cc rcu-utils.cc
	$(CPP) -I. $(CHECKING) $(CFLAGS) ti-rcu-audio.cc RAS_lib.c remote_control_audio.pb.cc rcu-utils.cc -o $@ $(LDFLAGS) -lprotobuf-lite

gfrm100-rcu-audio: gfrm100-rcu-audio.cc rcu-audio.h remote_control_audio.pb.cc rcu-utils.cc
	$(CPP) -I. $(CHECKING) $(CFLAGS) gfrm100-rcu-audio.cc remote_control_audio.pb.cc rcu-utils.cc -o $@ $(LDFLAGS) -lprotobuf-lite

gfrm-voice-demo: gfrm-voice-demo.cc rcu-audio.h remote_control_audio.pb.cc rcu-utils.cc
	$(CPP) -I. $(CHECKING) $(CFLAGS) gfrm-voice-demo.cc remote_control_audio.pb.cc rcu-utils.cc -o $@ $(LDFLAGS) -lprotobuf-lite

install:
	mkdir -p $(BINDIR)
	cp $(BINARIES) $(BINDIR)

install-libs:
	@echo "No libs to install."

clean:
	rm -f $(BINARIES) *.o

test:
	true
