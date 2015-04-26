PREFIX=/
BINDIR=$(DESTDIR)$(PREFIX)/bin
INSTALL=install
CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++

CFLAGS := -Wall -O2
CXXFLAGS := $(CFLAGS)

APPS := dvbnet dvbtune tssequencer

all: $(APPS)

$(APPS) : % : %.o common.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.o : %.c common.h
	$(CC) -c $(CFLAGS) $< -o $@

tssequencer: crc32.o

install: all
	echo 'target-install=$(INSTALL)'
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) $(APPS) $(BINDIR)

install-libs: all
	@echo "No libs to install."

test:
	@echo "Nothing to test."

.PHONY: clean
clean:
	$(RM) -f $(APPS) *.o
