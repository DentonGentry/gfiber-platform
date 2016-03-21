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
TOBJS=curl_env.o url.o errors.o request.o utils.o
OBJS=config.o \
     curl_env.o \
     download_task.o \
     errors.o \
     http_task.o \
     options.o \
     ping_task.o \
     request.o \
     speedtest.o \
     task.o \
     timed_runner.o \
     transfer_runner.o \
     transfer_task.o \
     upload_task.o \
     url.o \
     utils.o

all: speedtest

config.o: config.cc config.h url.h
errors.o: errors.cc errors.h
curl_env.o: curl_env.cc curl_env.h errors.h request.h
download_task.o: download_task.cc download_task.h transfer_task.h utils.h
http_task.o: http_task.cc http_task.h
options.o: options.cc options.h url.h
ping_task.o: ping_task.cc ping_task.h http_task.h request.h url.h utils.h
request.o: request.cc request.h url.h
speedtest.o: speedtest.cc \
             speedtest.h \
             config.h \
             curl_env.h \
             download_task.h \
             options.h \
             ping_task.h \
             request.h \
             task.h \
             timed_runner.h \
             transfer_runner.h \
             upload_task.h \
             url.h
speedtest_main.o: speedtest_main.cc options.h speedtest.h
task.o: task.cc task.h utils.h
timed_runner.o: timed_runner.cc timed_runner.h task.h
transfer_runner.o: transfer_runner.cc transfer_runner.h transfer_task.h utils.h
transfer_task.o: transfer_task.cc transfer_task.h http_task.h
upload_task.o: upload_task.cc upload_task.h transfer_task.h utils.h
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

