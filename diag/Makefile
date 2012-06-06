CC		:= $(CROSS_COMPILE)gcc
AR		:= $(CROSS_COMPILE)ar
RANLIB		:= $(CROSS_COMPILE)ranlib

CFLAGS		+= -Os -Wall -pipe -fno-strict-aliasing -I./include
LIBS		:= -lmocactl -lmoca -lpthread -lm
SRCS		:= diagd.c diagTest.c
TARGETS		:= diagd diagTest

PREFIX=/
BINDIR=$(DESTDIR)$(PREFIX)/usr/bin
LIBDIR=$(DESTDIR)$(PREFIX)/usr/lib/diag
SODIR=$(DESTDIR)$(PREFIX)/usr/lib
TESTDIR=$(DESTDIR)$(PREFIX)/home/test
INSTALL=install

# It is the diag core files
CORE_OBJS	:= diagParseKernMsgs.o diagMonApis.o \
	diagNetworkTests.o diagApis.o diagLogging.o diagSubs.o \
	diagMoca.o diagParseRefData.o diagError.o

.PHONY: all
all: subdirs $(TARGETS)

subdirs:
	$(MAKE) -C lib all
	$(MAKE) -C utest all

$(TARGETS): |subdirs

.PHONY: clean
clean:
	rm -f $(TARGETS) *.[od]

diagd: diagd.o $(CORE_OBJS)
	$(CC) $(LDFLAGS) diagd.o $(CORE_OBJS) -o $@ $(LIBS)

diagTest: diagTest.o $(CORE_OBJS)
	$(CC) $(LDFLAGS) diagTest.o $(CORE_OBJS) -o $@ $(LIBS)

ifneq ($(MAKECMDGOALS),clean)
-include $(SRCS:.c=.d)
endif

test:
	@echo "Nothing to test."

install: all
	mkdir -p $(BINDIR) $(LIBDIR) $(SODIR) $(TESTDIR)
	$(INSTALL) -m 0755 diagd $(BINDIR)/
	$(INSTALL) -m 0644 diag_kern_err_msgs.txt $(LIBDIR)/
	$(INSTALL) -m 0644 diag_kern_warn_msgs.txt $(LIBDIR)/
	$(INSTALL) -m 0644 diag_ref_data.txt $(LIBDIR)/
	$(INSTALL) -m 0755 lib/libbrunodiag.so $(SODIR)/
	$(INSTALL) -m 0755 utest/test_diaglib $(TESTDIR)/

install-libs:
	@echo "No libs to install."
