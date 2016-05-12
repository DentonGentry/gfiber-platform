CXX=$(CROSS_COMPILE)g++
INSTALL?=install
PREFIX=$(DESTDIR)/usr
BINDIR=$(PREFIX)/bin
DEBUG?=-g
WARNINGS=-Wall -Werror -Wno-unused-result -Wno-unused-but-set-variable
CXXFLAGS=$(DEBUG) $(WARNINGS) -DNDEBUG -std=c++11 $(EXTRACFLAGS)
#CXXFLAGS=$(DEBUG) $(WARNINGS) -O3 -DNDEBUG -std=c++11 $(EXTRACFLAGS)
LDFLAGS=$(DEBUG) $(EXTRALDFLAGS)

GTEST_DIR=googletest
GMOCK_DIR=googlemock
TFLAGS=$(DEBUG) -isystem ${GTEST_DIR}/include -isystem $(GMOCK_DIR)/include -pthread -std=c++11

LIBS=-lcurl -lpthread -ljsoncpp
TOBJS=curl_env.o url.o errors.o request.o status.o utils.o
OBJS=config.o \
     curl_env.o \
     download.o \
     errors.o \
     find_nearest.o \
     init.o \
     options.o \
     ping.o \
     region.o \
     request.o \
     speedtest.o \
     status.o \
     transfer_runner.o \
     upload.o \
     url.o \
     utils.o

all: speedtest

config.o: config.cc \
          config.h \
          errors.h \
          region.h \
          request.h \
          status.h \
          url.h \
          utils.h
curl_env.o: curl_env.cc curl_env.h errors.h request.h utils.h
download.o: download.cc \
            download.h \
            request.h \
            status.h \
            utils.h
errors.o: errors.cc errors.h
find_nearest.o: find_nearest.cc \
                find_nearest.h \
                ping.h \
                region.h \
                request.h \
                status.h \
                utils.h
init.o: init.cc \
        init.h \
        config.h \
        find_nearest.h \
        region.h \
        request.h \
        status.h \
        timed_runner.h \
        url.h \
        utils.h
options.o: options.cc options.h request.h url.h
ping.o: ping.cc \
        ping.h \
        errors.h \
        region.h \
        request.h \
        status.h \
        url.h \
        utils.h
region.o: region.cc \
          region.h \
          errors.h \
          request.h \
          status.h \
          region.h \
          utils.h
request.o: request.cc request.h url.h utils.h
speedtest.o: speedtest.cc \
             speedtest.h \
             config.h \
             download.h \
             init.h \
             options.h \
             region.h \
             request.h \
             status.h \
             timed_runner.h \
             transfer_runner.h \
             upload.h \
             url.h \
             utils.h
speedtest_main.o: speedtest_main.cc \
                  curl_env.h \
                  options.h \
                  request.h \
                  speedtest.h
status.o: status.cc status.h utils.h
transfer_runner.o: transfer_runner.cc \
                   transfer_runner.h \
                   status.h \
                   utils.h
upload.o: upload.cc \
          upload.h \
          request.h \
          status.h \
          utils.h
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
	$(CXX) -o $@ $(TFLAGS) googlemock/src/gmock_main.cc $< $*.o $(LDFLAGS) libgmock.a libspeedtesttest.a $(LIBS)
	./$@

test: config_test options_test region_test request_test url_test

install: speedtest
	$(INSTALL) -m 0755 speedtest $(BINDIR)/

install-libs:
	@echo "No libs to install"

clean:
	rm -f *.o *.a speedtest core *_test

