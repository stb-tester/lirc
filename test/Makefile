CXXFLAGS += -I. -I../lib  -g -std=c++11 -DHAVE_KERNEL_LIRC_H=1

CFLAGS   += -I. -I../lib -g

LDLIBS   += $(shell pkg-config --libs cppunit)
LDLIBS   += -lstdc++
LDLIBS   += -llirc -llirc_client -L ../lib/.libs -Wl,-rpath=../lib/.libs

TESTS     = ClientTest.h \
	    DecodeTest.h \
            DrvAdminTest.h \
            IrRemoteTest.h \
	    LogTest.h \
            OptionsTest.h \
	    Util.h

LIRC_LIBS = ../lib/.libs/liblirc.so.0 ../lib/.libs/liblirc_client.so.0

all: run-tests echoserver

run-tests: run-tests.cpp $(TESTS) $(LIRC_LIBS) Makefile
	gcc -o run-tests  $(CXXFLAGS) $(LDLIBS) run-tests.cpp

clean:
	rm -f *.o run-tests *.log
