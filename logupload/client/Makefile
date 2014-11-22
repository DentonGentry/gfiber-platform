CC:=$(CROSS_COMPILE)gcc
CPP:=$(CROSS_COMPILE)g++
LD:=$(CROSS_COMPILE)ld
AR:=$(CROSS_COMPILE)ar
RANLIB:=$(CROSS_COMPILE)ranlib
STRIP:=$(CROSS_COMPILE)strip
USRBINDIR=$(DESTDIR)/usr/bin

CFLAGS += -Wall -Werror $(EXTRACFLAGS)
LDFLAGS += -lrt -lcurl -lz -lm $(EXTRALDFLAGS)

# Test Flags
TEST_LDFLAGS=$(LDFLAGS) -lgtest -pthread

all: upload-crash-log2
tests: kvextract_test
SRCS = log_uploader.c kvextract.c upload.c utils.c
INCS = kvextract.h upload.h utils.h
MAIN_SRC = log_uploader_main.c

upload-crash-log2: $(SRCS) $(INCS) $(MAIN_SRC)
	$(CC) $(CFLAGS) $(SRCS) $(MAIN_SRC) -o $@ $(LDFLAGS)

kvextract_test: $(SRCS) $(INCS) kvextract_test.c
	$(CPP) $(CFLAGS) kvextract_test.c $(SRCS) -o $@ $(TEST_LDFLAGS)

utils_test: $(SRCS) $(INCS) utils_test.c
	$(CPP) $(CFLAGS) utils_test.c $(SRCS) -o $@ $(TEST_LDFLAGS)

log_uploader_test: $(SRCS) $(INCS) log_uploader_test.c
	$(CPP) $(CFLAGS) log_uploader_test.c $(SRCS) -o $@ $(TEST_LDFLAGS)

install:
	mkdir -p $(USRBINDIR)
	cp upload-crash-log2 $(USRBINDIR)

install-libs:
	@echo "No libs to install."

test: kvextract_test utils_test log_uploader_test
	./kvextract_test
	./utils_test
	./log_uploader_test

clean:
	rm -f upload-crash-log2 *.o
