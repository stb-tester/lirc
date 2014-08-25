#ifndef  LOG_TEST
#define  LOG_TEST

#include    <iostream>
#include    <istream>
#include    <unordered_map>
#include	<stdio.h>
#include	"../lib/lirc_private.h"

#include    <cppunit/TestFixture.h>
#include    <cppunit/TestSuite.h>
#include    <cppunit/TestCaller.h>

#define     NAME "Acer_Aspire_6530G_MCE"

#undef      ADD_TEST
#define     ADD_TEST(id, func) \
    testSuite->addTest(new CppUnit::TestCaller<LogTest>( \
                       id,  &LogTest::func))

using namespace std;

class LogTest : public CppUnit::TestFixture
{
    private:
	    ir_remote* config;
        string log;

    public:
        static CppUnit::Test* suite()
        {
            CppUnit::TestSuite* testSuite =
                 new CppUnit::TestSuite( "LogTest" );
            ADD_TEST("testLevels", testLevels);
            return testSuite;
        };


        void setUp()
        {
            string path = string("logtest.log");
            lirc_log_set_file(path.c_str());
            lirc_log_open("IrRemoteTest", 0, 10);

            lirc_log_setlevel("9");
            LOGPRINTF(2, "Testing LOG_PEEP: %s", "PEEP arg");
            LOGPRINTF(3, "Testing LOG_STALK (disabled): %s", "STALK arg");

            lirc_log_setlevel("8");
            LOGPRINTF(2, "Testing LOG_TRACE: %s", "TRACE arg");
            logprintf(LOG_INFO, "Testing enabled TRACE");

            lirc_log_setlevel("4");
            logprintf(LOG_INFO, "Testing disabled WARNING");
            logprintf(LOG_WARNING, "Testing enabled WARNING");
            lirc_log_close();

            ifstream logfile("logtest.log");
            stringstream buffer;
            buffer << logfile.rdbuf();
            log = buffer.str();

        };

        void testLevels()
        {
            CPPUNIT_ASSERT(log.find("LOG_PEEP") != string::npos);
            CPPUNIT_ASSERT(log.find("enabled TRACE") != string::npos);
            CPPUNIT_ASSERT(log.find("STALK") == string::npos);
            CPPUNIT_ASSERT(log.find("disabled WARNING") == string::npos);
            CPPUNIT_ASSERT(log.find("enabled WARNING") != string::npos);
        };


};

#endif

// vim: set expandtab ts=4 sw=4:
