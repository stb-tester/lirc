#ifndef  DRV_ADMIN_TEST
#define  DRV_ADMIN_TEST

#include    <iostream>
#include    <unordered_map>
#include	<stdio.h>
#include	"../lib/lirc_private.h"

#include    <cppunit/TestFixture.h>
#include    <cppunit/TestSuite.h>
#include    <cppunit/TestCaller.h>

#include    "lirc_private.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

#ifdef  ADD_TEST
#undef  ADD_TEST
#endif

#define     ADD_TEST(id, func) \
    testSuite->addTest(new CppUnit::TestCaller<DrvAdminTest>( \
                       id,  &DrvAdminTest::func))

static const int DRIVER_COUNT = 51;  // Total numbers of drivers.
static const int PLUGIN_COUNT = 40;  // Total numbers of drivers.

using namespace std;

static driver* drv_guest_counter(struct driver*, void* arg)
{
    (*(int*)arg)++;
    return 0;
}

struct driver* listPlugins(const char* path, drv_guest_func guest, void* arg)
{
    (*(int*)arg)++;
    return 0;
}

class DrvAdminTest : public CppUnit::TestFixture
{
    private:

    public:
        static CppUnit::Test* suite()
        {
            CppUnit::TestSuite* testSuite =
                 new CppUnit::TestSuite( "DrvAdminTest" );
            ADD_TEST("testLoad", testLoad);
            ADD_TEST("testCount", testCount);
            ADD_TEST("testListPlugins", testListPlugins);
            return testSuite;
        };

        void setUp()
        {
            lirc_log_set_file(string("drv_admin.log").c_str());
            lirc_log_open("DrvAdminTest", 0, LIRC_TRACE2);
            setenv("LIRC_PLUGINDIR", "../plugins/.libs", 1);
        };

        void tearDown()
        {
        };

        void testLoad()
        {
            options_unload();
            char* nil = 0;
            options_load(
                0, &nil, abspath("etc/lirc_options.conf"), dummy_load);
            hw_choose_driver("dvico");
            CPPUNIT_ASSERT(string(curr_driver->name) == "dvico");
            CPPUNIT_ASSERT(string(curr_driver->device) == "/dev/usb/hiddev0");
        }

        void testCount()
        {
            int count = 0;
            setenv("LD_LIBRARY_PATH", "../lib/.libs", 1);
            for_each_driver(drv_guest_counter, (void*)&count, NULL);
            if (getenv("LIRC_TEST_DEBUG"))
                cout << "Driver count: " << count << "\n";
            CPPUNIT_ASSERT( count == DRIVER_COUNT );
        }

        void testListPlugins()
        {
            int count = 0;
            for_each_plugin(listPlugins, (void*)&count, NULL);
            if (getenv("LIRC_TEST_DEBUG"))
                cout << "Plugin count: " << count << "\n";
            CPPUNIT_ASSERT(count == PLUGIN_COUNT );
        }
};

#endif

// vim: set expandtab ts=4 sw=4:
