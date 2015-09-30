default: all

CC:=$(CROSS_COMPILE)gcc
CXX:=$(CROSS_COMPILE)g++
LD:=$(CROSS_COMPILE)ld
AR:=$(CROSS_COMPILE)ar
RANLIB:=$(CROSS_COMPILE)ranlib
STRIP:=$(CROSS_COMPILE)strip
USRBINDIR=$(DESTDIR)/usr/bin

CFLAGS+=-Wall -Werror $(EXTRACFLAGS)
LDFLAGS+=$(EXTRALDFLAGS)
LIBS=-lrt -lcurl -lz -lm

# Test Flags
TEST_LDFLAGS=$(LDFLAGS)
TEST_LIBS=$(LIBS) -lgtest -pthread

OBJS = log_uploader.o kvextract.o upload.o utils.o
INCS = kvextract.h upload.h utils.h
MAIN_OBJ = log_uploader_main.o
TESTS = kvextract_test utils_test log_uploader_test

all: upload-crash-log2 upload-logs

tests: all $(TESTS)

kvextract_test: kvextract_test.o
utils_test: utils_test.o
log_uploader_test: log_uploader_test.o

# an alias the Debian package can use, since upload-crash-log2 is kind of
# a weird name, present mainly for backward compatibility with the old
# loguploader on GFiber CPE devices (for now).
upload-logs: upload-crash-log2
	ln -f $< $@

%.o: %.c $(INCS)
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cc $(INCS)
	$(CXX) $(CFLAGS) -c -o $@ $<

upload-crash-log2: $(OBJS) $(MAIN_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%_test: %_test.o $(OBJS)
	$(CXX) $(TEST_LDFLAGS) -o $@ $^ $(TEST_LIBS)

install:
	mkdir -p $(USRBINDIR)
	cp upload-crash-log2 $(USRBINDIR)

install-libs:
	@echo "No libs to install."

test: tests
	set -e; for d in $(TESTS); do ./$$d; done
	# Ensure proper handling of binary files
	rm -f test.tmp
	./upload-crash-log2 --stdin=test --stdout <upload-crash-log2 >test.tmp
	cmp upload-crash-log2 test.tmp
	rm -f test.tmp
	wvtest/wvtestrun ./prefix-logs.test

clean:
	rm -f upload-crash-log2 upload-logs $(TESTS) *~ *.o
