CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
INSTALL=install
PREFIX=/usr
LIBDIR=$(DESTDIR)$(PREFIX)/lib
INCLUDEDIR=$(DESTDIR)$(PREFIX)/include

all: libexperiments.so

CPPFLAGS=$(EXTRACFLAGS)
CFLAGS=-Wall -Werror -g -fPIC -Wswitch-enum -Wextra -fno-omit-frame-pointer \
    -Wno-sign-compare -Wno-unused-parameter $(EXTRACFLAGS)
CXXFLAGS=-Wall -Werror -g -fPIC -Wswitch-enum -Wextra -fno-omit-frame-pointer \
    -Wno-sign-compare -Wno-unused-parameter -std=c++0x $(EXTRACXXFLAGS)
LDFLAGS+=$(EXTRALDFLAGS)

libexperiments.so: experiments.o utils.o
	$(CC) -shared -Wl,-soname,libexperiments.so -Wl,-export-dynamic -o $@ $^

experiments_test: experiments.o experiments_test.o experiments_c_api_test.o utils.o
	$(CXX) -o $@ $^ $(LDFLAGS) $(CPPFLAGS) -lgtest -lpthread

%.o: %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@
%.o: %.cc
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) $< -o $@

# all *.o depend on all the header files
$(patsubst %.cc,%.o,$(wildcard *.cc)) $(patsubst %.c,%.o,$(wildcard *.c)): \
  $(wildcard *.h)

install: all
	echo 'target-install=$(INSTALL)'
	mkdir -p $(LIBDIR)
	$(INSTALL) -m 0755 libexperiments.so $(LIBDIR)/

install-libs: all
	echo 'staging-install=$(INSTALL)'
	mkdir -p $(INCLUDEDIR) $(LIBDIR) $(LIBDIR)/pkgconfig
	$(INSTALL) -m 0644 experiments.h $(INCLUDEDIR)/
	$(INSTALL) -m 0755 libexperiments.so $(LIBDIR)/

test: experiments_test
	./experiments_test

clean:
	rm -rf *.[oa] *.so *~
