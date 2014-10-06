#ifndef  DECODE_TEST
#define  DECODE_TEST

#include	<stdio.h>
#include	<signal.h>

#include    <iostream>
#include    <unordered_map>
#include    <cppunit/TestFixture.h>
#include    <cppunit/TestSuite.h>
#include    <cppunit/TestCaller.h>

#include	"../lib/lirc_private.h"
#include	"../lib/lirc_client.h"

#undef      ADD_TEST
#define     ADD_TEST(id, func) \
    testSuite->addTest(new CppUnit::TestCaller<DecodeTest>( \
                       id,  &DecodeTest::func))

using namespace std;

class DecodeTest : public CppUnit::TestFixture
{
    private:

    public:
        static CppUnit::Test* suite()
        {
            CppUnit::TestSuite* testSuite =
                 new CppUnit::TestSuite( "ClientTest" );
            ADD_TEST("testDBS", testDBS);
            ADD_TEST("testLongpress", testLongpress);
            ADD_TEST("testSpaceEnc1", testSpaceEnc1);
            ADD_TEST("testSpaceEnc2", testSpaceEnc1);
            ADD_TEST("testSpaceEnc3", testSpaceEnc1);
            ADD_TEST("testRc5", testSpaceEnc1);
            ADD_TEST("testRc6", testSpaceEnc1);
            ADD_TEST("testRaw", testSpaceEnc1);
            return testSuite;
        };

        void setUp() {};

        void tearDown() {};

        void testDBS()
        {
            unsetenv("LIRC_SOCKET_PATH");
            unsetenv("LIRC_LOGLEVEL");
            system("tests/DBS/run-test.sh");
        }

        void testLongpress()
        {
            unsetenv("LIRC_SOCKET_PATH");
            unsetenv("LIRC_LOGLEVEL");
            system("tests/longpress/run-test.sh");
        }

        void testSpaceEnc1()
        {
            unsetenv("LIRC_SOCKET_PATH");
            unsetenv("LIRC_LOGLEVEL");
            system("tests/space-enc-1/run-test.sh");
        }

        void testSpaceEnc2()
        {
            unsetenv("LIRC_SOCKET_PATH");
            unsetenv("LIRC_LOGLEVEL");
            system("tests/space-enc-2/run-test.sh");
        }

        void testSpaceEnc3()
        {
            unsetenv("LIRC_SOCKET_PATH");
            unsetenv("LIRC_LOGLEVEL");
            system("tests/space-enc-3/run-test.sh");
        }

        void testRc5()
        {
            unsetenv("LIRC_SOCKET_PATH");
            unsetenv("LIRC_LOGLEVEL");
            system("tests/rc5/run-test.sh");
        }

        void testRc6()
        {
            unsetenv("LIRC_SOCKET_PATH");
            unsetenv("LIRC_LOGLEVEL");
            system("tests/rc6/run-test.sh");
        }

        void testRaw()
        {
            unsetenv("LIRC_SOCKET_PATH");
            unsetenv("LIRC_LOGLEVEL");
            system("tests/raw/run-test.sh");
        }
};

#endif

// vim: set expandtab ts=4 sw=4:
