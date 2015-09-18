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

#ifdef      NAME
#undef      NAME
#endif

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
            ADD_TEST("testString2Level", testString2Level);
            ADD_TEST("testDefaultlevel", testDefaultlevel);
            return testSuite;
        };


        void setUp()
        {
            string path = string("logtest.log");
            lirc_log_set_file(path.c_str());
            lirc_log_open("IrRemoteTest", 0, LIRC_TRACE2);

            lirc_log_setlevel(LIRC_TRACE1);
            logprintf(LIRC_TRACE1, "Testing LIRC_TRACE1: %s", "TRACE1 arg");
            logprintf(LIRC_TRACE2, "Testing LIRC_TRACE2 (disabled): %s", "TRACE2 arg");

            lirc_log_setlevel(LIRC_TRACE);
            logprintf(LIRC_TRACE1, "Testing LIRC_TRACE: %s", "TRACE arg");
            logprintf(LIRC_INFO, "Testing enabled TRACE");

            lirc_log_setlevel(LIRC_WARNING);
            logprintf(LIRC_INFO, "Testing disabled WARNING");
            logprintf(LIRC_WARNING, "Testing enabled WARNING");
            lirc_log_close();

            ifstream logfile("logtest.log");
            stringstream buffer;
            buffer << logfile.rdbuf();
            log = buffer.str();

        };

        void testLevels()
        {
            CPPUNIT_ASSERT(log.find("LIRC_TRACE1") != string::npos);
            CPPUNIT_ASSERT(log.find("enabled TRACE") != string::npos);
            CPPUNIT_ASSERT(log.find("TRACE2") == string::npos);
            CPPUNIT_ASSERT(log.find("disabled WARNING") == string::npos);
            CPPUNIT_ASSERT(log.find("enabled WARNING") != string::npos);
        };

        void testString2Level()
        {
           CPPUNIT_ASSERT(string2loglevel("trace2") == LIRC_TRACE2);
           CPPUNIT_ASSERT(string2loglevel("error") == LIRC_ERROR);
           CPPUNIT_ASSERT(string2loglevel("InfO") == LIRC_INFO);
           CPPUNIT_ASSERT(string2loglevel("Notice") == LIRC_NOTICE);
           CPPUNIT_ASSERT(string2loglevel("Notter") == LIRC_BADLEVEL);
           CPPUNIT_ASSERT(string2loglevel("5") == 5);
           CPPUNIT_ASSERT(string2loglevel("0") == LIRC_BADLEVEL);
           CPPUNIT_ASSERT(string2loglevel("11") == LIRC_BADLEVEL);
        };

        void testDefaultlevel()
        {
            setenv("LIRC_LOGLEVEL", "info", 1);
            CPPUNIT_ASSERT(lirc_log_defaultlevel() == LIRC_INFO);
            setenv("LIRC_LOGLEVEL", "7", 1);
            CPPUNIT_ASSERT(lirc_log_defaultlevel() == LIRC_DEBUG);
            unsetenv("LIRC_LOGLEVEL");
            CPPUNIT_ASSERT(lirc_log_defaultlevel() == DEFAULT_LOGLEVEL);
            setenv("LIRC_LOGLEVEL", "foo", 1);
            CPPUNIT_ASSERT(lirc_log_defaultlevel() == DEFAULT_LOGLEVEL);
        }
};

#endif

// vim: set expandtab ts=4 sw=4:
