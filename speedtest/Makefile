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

LIBS=-lcurl -lpthread
TOBJS=curl_env.o url.o errors.o request.o utils.o
OBJS=errors.o curl_env.o options.o request.o utils.o speedtest.o url.o

all: speedtest

errors.o: errors.cc errors.h
curl_env.o: curl_env.cc curl_env.h errors.h request.h
options.o: options.cc options.h url.h
utils.o: utils.cc options.h
request.o: request.cc request.h curl_env.h url.h
url.o: url.cc url.h
speedtest.o: speedtest.cc speedtest.h curl_env.h options.h request.h url.h
speedtest_main.o: speedtest_main.cc options.h speedtest.h
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

%_test.o: %_test.cc %.h
	$(CXX) -c $< $(TFLAGS) $(CXXFLAGS)

%_test: %_test.o %.o libgmock.a libspeedtesttest.a
	$(CXX) -o $@ $(TFLAGS) googlemock/src/gmock_main.cc $< $*.o $(LDFLAGS) $(LIBS) libgmock.a libspeedtesttest.a
	./$@

test: options_test request_test url_test

install: speedtest
	$(INSTALL) -m 0755 speedtest $(BINDIR)/

install-libs:
	@echo "No libs to install"

clean:
	rm -f *.o *.a speedtest core *_test

