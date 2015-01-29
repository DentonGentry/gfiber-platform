PREFIX=/
BINDIR=$(DESTDIR)$(PREFIX)/bin
INSTALL=install
CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++

CFLAGS := -Wall -O2
CXXFLAGS := $(CFLAGS)

APPS := dvbnet dvbtune

all: $(APPS)

$(APPS) : % : %.o
	$(CC) -o $@ $< $(LDFLAGS)

$(addsuffix .o,$(APPS)): %.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@

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
