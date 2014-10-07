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


};

#endif

// vim: set expandtab ts=4 sw=4:
