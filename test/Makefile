CPPFLAGS += -I.. -I../lib  -g -std=c++11

LDFLAGS  += $(shell pkg-config --libs cppunit)
LDFLAGS  += -lstdc++
LDFLAGS  += -llirc -llirc_client -L ../lib/.libs -Wl,-rpath=../lib/.libs

TESTS     = ClientTest.h \
            DrvAdminTest.h \
            IrRemoteTest.h \
	    LogTest.h \
            OptionsTest.h

LIRC_LIBS = ../lib/.libs/liblirc.so.0 ../lib/.libs/liblirc_client.so.0

all: run-tests

run-tests: run-tests.cpp $(TESTS) $(LIRC_LIBS) Makefile
	gcc -o run-tests  $(CPPFLAGS) $(LDFLAGS) run-tests.cpp

clean:
	rm -f *.o run-tests *.log
