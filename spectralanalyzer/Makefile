CC:=$(CROSS_COMPILE)gcc
CPP:=$(CROSS_COMPILE)g++
LD:=$(CROSS_COMPILE)ld
AR:=$(CROSS_COMPILE)ar
RANLIB:=$(CROSS_COMPILE)ranlib
STRIP:=$(CROSS_COMPILE)strip
BINDIR=$(DESTDIR)/bin

CFLAGS += -Wall -Werror $(EXTRACFLAGS)
LDFLAGS += -lm -lrt $(EXTRALDFLAGS)

all: spectral
SRCS = spectral.c
INCS =

spectral: $(SRCS) $(INCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(LDFLAGS)

install:
	mkdir -p $(BINDIR)
	cp spectral $(BINDIR)

install-libs:
	@echo "No libs to install."

test:
	@echo "No tests to run."

clean:
	rm -f spectral spectral_test *.o
