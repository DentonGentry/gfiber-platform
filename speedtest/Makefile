CXX=$(CROSS_COMPILE)g++
INSTALL?=install
PREFIX=$(DESTDIR)/usr
BINDIR=$(PREFIX)/bin
DEBUG?=-g
WARNINGS=-Wall -Werror -Wno-unused-result -Wno-unused-but-set-variable
CXXFLAGS=$(DEBUG) $(WARNINGS) -O3 -DNDEBUG -std=c++11 $(EXTRACFLAGS)
LDFLAGS=$(DEBUG) $(EXTRALDFLAGS)

GTEST_DIR=googletest
GMOCK_DIR=googlemock
TFLAGS=$(DEBUG) -isystem ${GTEST_DIR}/include -isystem $(GMOCK_DIR)/include -pthread -std=c++11

LIBS=-lcurl -lpthread -ljsoncpp
TOBJS=curl_env.o url.o errors.o request.o utils.o
OBJS=config.o \
     curl_env.o \
     download_test.o \
     errors.o \
     generic_test.o \
     options.o \
     ping_test.o \
     request.o \
     runner.o \
     speedtest.o \
     transfer_test.o \
     upload_test.o \
     url.o \
     utils.o

all: speedtest

config.o: config.cc config.h url.h
errors.o: errors.cc errors.h
curl_env.o: curl_env.cc curl_env.h errors.h request.h
download_test.o: download_test.cc download_test.h generic_test.h transfer_test.h utils.h
generic_test.o: generic_test.cc generic_test.h request.h utils.h
options.o: options.cc options.h url.h
ping_test.o: ping_test.cc ping_test.h generic_test.h request.h url.h utils.h
request.o: request.cc request.h url.h
runner.o: runner.cc runner.h generic_test.h transfer_test.h
speedtest.o: speedtest.cc \
             speedtest.h \
             config.h \
             curl_env.h \
             download_test.h \
             generic_test.h \
             options.h \
             ping_test.h \
             request.h \
             runner.h \
             upload_test.h \
             url.h
speedtest_main.o: speedtest_main.cc options.h speedtest.h
transfer_test.o: transfer_test.cc transfer_test.h generic_test.h
upload_test.o: upload_test.cc upload_test.h generic_test.h transfer_test.h utils.h
utils.o: utils.cc options.h
url.o: url.cc url.h utils.h

speedtest: speedtest_main.o $(OBJS)
	$(CXX) -o $@ $< $(OBJS) $(LDFLAGS) $(LIBS)

libgtest.a:
	g++ -isystem ${GTEST_DIR}/include -I${GTEST_DIR} \
		-pthread -c ${GTEST_DIR}/src/gtest-all.cc
	ar -rv libgtest.a gtest-all.o

libgmock.a:
	g++ -isystem ${GTEST_DIR}/include -I${GTEST_DIR} \
		-isystem ${GMOCK_DIR}/include -I${GMOCK_DIR} \
		-pthread -c ${GTEST_DIR}/src/gtest-all.cc
	g++ -isystem ${GTEST_DIR}/include -I${GTEST_DIR} \
		-isystem ${GMOCK_DIR}/include -I${GMOCK_DIR} \
		-pthread -c ${GMOCK_DIR}/src/gmock-all.cc
	ar -rv libgmock.a gtest-all.o gmock-all.o

libspeedtesttest.a: $(TOBJS)
	ar -rv libspeedtesttest.a $(TOBJS)

%_test.o: %_test.cc %.h %.cc
	$(CXX) -c $< $(TFLAGS) $(CXXFLAGS)

%_test: %_test.o %.o libgmock.a libspeedtesttest.a
	$(CXX) -o $@ $(TFLAGS) googlemock/src/gmock_main.cc $< $*.o $(LDFLAGS) $(LIBS) libgmock.a libspeedtesttest.a
	./$@

test: config_test options_test request_test url_test

install: speedtest
	$(INSTALL) -m 0755 speedtest $(BINDIR)/

install-libs:
	@echo "No libs to install"

clean:
	rm -f *.o *.a speedtest core *_test

