#ifndef  OPTIONS_TEST
#define  OPTIONS_TEST

#include    <iostream>
#include    <unordered_map>
#include	<stdio.h>
#include	"../lib/lirc_private.h"

#include    <cppunit/TestFixture.h>
#include    <cppunit/TestSuite.h>
#include    <cppunit/TestCaller.h>

#include    "lirc_options.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

#ifdef  ADD_TEST
#undef  ADD_TEST
#endif

#define     ADD_TEST(id, func) \
    testSuite->addTest(new CppUnit::TestCaller<OptionsTest>( \
                       id,  &OptionsTest::func))

using namespace std;

class OptionsTest : public CppUnit::TestFixture
{
    private:

    public:
        static CppUnit::Test* suite()
        {
            CppUnit::TestSuite* testSuite =
                 new CppUnit::TestSuite( "OptionsTest" );
            ADD_TEST("testLoad", testLoad);
            ADD_TEST("testAddDefaults", testAddDefaults);
            ADD_TEST("testSetOpt", testSetOpt);
            return testSuite;
        };


        void setUp()
        {
            lirc_log_set_file(string("options.log").c_str());
            lirc_log_open("OptionsTest", 0, LIRC_TRACE2);
        };

        void tearDown()
        {
        };

        void testLoad()
        {
            char* nil = 0;
            options_load(
                0, &nil, abspath("etc/lirc_options.conf"), dummy_load);
            CPPUNIT_ASSERT(
                string(options_getstring("lircd:device")) == "/dev/lirc0");
            CPPUNIT_ASSERT(
                string(options_getstring("lircd:driver")) == "default");
            CPPUNIT_ASSERT(options_getboolean("lircd:allow-simulate") == 1);
            CPPUNIT_ASSERT(options_getint("lircd:repeat-max") == 600);
        }

        void testAddDefaults()
        {

            const char* defaults[] = {
                "lircd:allow-simulate", "Yes",
                "lircd:debug", "8",
                0, 0
            };

            char* nil = 0;
            options_load(
                0, &nil, abspath("etc/empty_options.conf"), dummy_load);
            options_add_defaults(defaults);
            CPPUNIT_ASSERT(options_getboolean("lircd:allow-simulate") == 1);
            CPPUNIT_ASSERT(options_getint("lircd:debug") == 8);
         }

        void testSetOpt()
        {

            char* nil = 0;
            options_load(
                0, &nil, abspath("etc/lirc_options.conf"), dummy_load);
            options_set_opt("lircd:foo", "bar");
            CPPUNIT_ASSERT(string(options_getstring("lircd:foo")) == "bar");
         };

};

#endif

// vim: set expandtab ts=4 sw=4:
