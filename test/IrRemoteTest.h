#ifndef  IR_REMOTE_TEST
#define  IR_REMOTE_TEST

#include    <iostream>
#include    <unordered_map>
#include	<stdio.h>
#include	"../lib/lirc_private.h"

#include    <cppunit/TestFixture.h>
#include    <cppunit/TestSuite.h>
#include    <cppunit/TestCaller.h>

#define     NAME "Acer_Aspire_6530G_MCE"

#define     ADD_TEST(id, func) \
    testSuite->addTest(new CppUnit::TestCaller<IrRemoteTest>( \
                       id,  &IrRemoteTest::func))

using namespace std;

class IrRemoteTest : public CppUnit::TestFixture
{
    private:
	    ir_remote* config;
	    FILE* f;

    public:
        static CppUnit::Test* suite()
        {
            CppUnit::TestSuite* testSuite =
                 new CppUnit::TestSuite( "IrRemoteTest" );
            ADD_TEST("testName", testName);
            ADD_TEST("testStart", testStart);
            ADD_TEST("testPulses", testPulses);
            ADD_TEST("testGaps", testGaps);
            ADD_TEST("testCodes", testCodes);
            ADD_TEST("testDefaults", testDefaults);
            return testSuite;
        };

        void setUp()
        {
            string path = string("ir_remote.log");
            lirc_log_set_file(path.c_str());
            lirc_log_open("IrRemoteTest", 0, LIRC_STALK);
            f = fopen("etc/lircd.conf.Aspire_6530G", "r");
            config = read_config(f, NAME);
        };

        void tearDown()
        {
            fclose(f);
        };

        void testName()
        {
            CPPUNIT_ASSERT(string(config->name) == NAME);
        };

        void testStart()
        {
            CPPUNIT_ASSERT(config->bits == 13);
            CPPUNIT_ASSERT(config->flags == RC6|CONST_LENGTH);
            CPPUNIT_ASSERT(config->eps == 30);
            CPPUNIT_ASSERT(config->aeps == 122);
        };

        void testPulses()
        {
            CPPUNIT_ASSERT(config->pone == 482);
            CPPUNIT_ASSERT(config->sone == 420);
            CPPUNIT_ASSERT(config->phead == 2740);
            CPPUNIT_ASSERT(config->shead == 860);
            CPPUNIT_ASSERT(config->pzero == 482);
            CPPUNIT_ASSERT(config->szero == 420);
            CPPUNIT_ASSERT(config->pthree == 0);
            CPPUNIT_ASSERT(config->stwo == 0);
            CPPUNIT_ASSERT(config->ptwo == 0);
            CPPUNIT_ASSERT(config->sthree == 0);
        };

        void testGaps()
        {
            CPPUNIT_ASSERT(config->pre_data_bits == 24);
            CPPUNIT_ASSERT(config->pre_data == 0x1bff83);
            CPPUNIT_ASSERT(config->gap == 110890);
            CPPUNIT_ASSERT(config->toggle_bit_mask == 0x8000);
            CPPUNIT_ASSERT(config->gap == 110890);
            CPPUNIT_ASSERT(config->rc6_mask == 0x100000000);
        }

        void testCodes()
        {
             unordered_map<string, int> codemap;
             struct ir_ncode* c = (struct ir_ncode*) (config->codes);
             while(c->name){
                codemap[c->name] = c->code;
                c++;
             };
             CPPUNIT_ASSERT(codemap["KEY_POWER"] = 0x1bf3);
             CPPUNIT_ASSERT(codemap["KEY_KPSLASH"] = 0x1be3);
             CPPUNIT_ASSERT(codemap.size() == 45);
        };

        void testDefaults()
        {
             CPPUNIT_ASSERT(config->pthree == 0);
             CPPUNIT_ASSERT(config->sthree == 0);
             CPPUNIT_ASSERT(config->ptwo == 0);
             CPPUNIT_ASSERT(config->stwo == 0);
             CPPUNIT_ASSERT(config->pfoot == 0);
             CPPUNIT_ASSERT(config->sfoot == 0);
             CPPUNIT_ASSERT(config->prepeat == 0);
             CPPUNIT_ASSERT(config->srepeat == 0);
             CPPUNIT_ASSERT(config->pre_p == 0);
             CPPUNIT_ASSERT(config->pre_s == 0);
             CPPUNIT_ASSERT(config->dyncodes_name == 0);
             CPPUNIT_ASSERT(config->post_data_bits == 0);
        }

};

#endif

// vim: set expandtab ts=4 sw=4:
