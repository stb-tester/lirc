#ifndef  IR_REMOTE_TEST
#define  IR_REMOTE_TEST

#include    <iostream>
#include    <unordered_map>
#include	<stdio.h>
#include	"../lib/lirc_private.h"

#include    <cppunit/TestFixture.h>
#include    <cppunit/TestSuite.h>
#include    <cppunit/TestCaller.h>

#ifdef      NAME
#undef      NAME
#endif

#define     NAME "etc/lircd.conf.Aspire_6530G"

#define     ADD_TEST(id, func) \
    testSuite->addTest(new CppUnit::TestCaller<IrRemoteTest>( \
                       id,  &IrRemoteTest::func))

using namespace std;

class IrRemoteTest : public CppUnit::TestFixture
{
    private:
	    ir_remote* config;
        ir_remote* acer_config;
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
            ADD_TEST("testImplicitInclude", testImplicitInclude);
            ADD_TEST("testRawSorting", testRawSorting);
            ADD_TEST("testManualSorting", testManualSorting);
            return testSuite;
        };

        void std_setup()
        {
            struct ir_remote* irc;

            string path = string("ir_remote.log");
            lirc_log_set_file(path.c_str());
            lirc_log_open("IrRemoteTest", 0, LIRC_TRACE2);
            f = fopen(NAME, "r");
            CPPUNIT_ASSERT(f != NULL);
            config = read_config(f, NAME);
            for (irc = config; irc != NULL; irc = irc->next) {
                if (string(irc->name) == "Acer_Aspire_6530G_MCE") {
                    acer_config = irc;
                    break;
                }
            }
            CPPUNIT_ASSERT(acer_config != NULL);

        };

        void tearDown()
        {
            if (f != NULL)
                fclose(f);
        };

        void testName()
        {
            std_setup();
            CPPUNIT_ASSERT(string(config->name) == "ECHOSTAR-119420");
        };

        void testStart()
        {
            std_setup();
            CPPUNIT_ASSERT(acer_config->bits == 13);
            CPPUNIT_ASSERT(acer_config->flags == RC6|CONST_LENGTH);
            CPPUNIT_ASSERT(acer_config->eps == 30);
            CPPUNIT_ASSERT(acer_config->aeps == 122);
        };

        void testPulses()
        {
            std_setup();
            CPPUNIT_ASSERT(acer_config->pone == 482);
            CPPUNIT_ASSERT(acer_config->sone == 420);
            CPPUNIT_ASSERT(acer_config->phead == 2740);
            CPPUNIT_ASSERT(acer_config->shead == 860);
            CPPUNIT_ASSERT(acer_config->pzero == 482);
            CPPUNIT_ASSERT(acer_config->szero == 420);
            CPPUNIT_ASSERT(acer_config->pthree == 0);
            CPPUNIT_ASSERT(acer_config->stwo == 0);
            CPPUNIT_ASSERT(acer_config->ptwo == 0);
            CPPUNIT_ASSERT(acer_config->sthree == 0);
        };

        void testGaps()
        {
            std_setup();
            CPPUNIT_ASSERT(acer_config->pre_data_bits == 24);
            CPPUNIT_ASSERT(acer_config->pre_data == 0x1bff83);
            CPPUNIT_ASSERT(acer_config->gap == 110890);
            CPPUNIT_ASSERT(acer_config->toggle_bit_mask == 0x8000);
            CPPUNIT_ASSERT(acer_config->gap == 110890);
            CPPUNIT_ASSERT(acer_config->rc6_mask == 0x100000000);
        }

        void testCodes()
        {
            std_setup();
            unordered_map<string, int> codemap;
            struct ir_ncode* c = (struct ir_ncode*) (acer_config->codes);
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
            std_setup();
            CPPUNIT_ASSERT(acer_config->pthree == 0);
            CPPUNIT_ASSERT(acer_config->sthree == 0);
            CPPUNIT_ASSERT(acer_config->ptwo == 0);
            CPPUNIT_ASSERT(acer_config->stwo == 0);
            CPPUNIT_ASSERT(acer_config->pfoot == 0);
            CPPUNIT_ASSERT(acer_config->sfoot == 0);
            CPPUNIT_ASSERT(acer_config->prepeat == 0);
            CPPUNIT_ASSERT(acer_config->srepeat == 0);
            CPPUNIT_ASSERT(acer_config->pre_p == 0);
            CPPUNIT_ASSERT(acer_config->pre_s == 0);
            CPPUNIT_ASSERT(acer_config->dyncodes_name == 0);
            CPPUNIT_ASSERT(acer_config->post_data_bits == 0);
        }

        void testImplicitInclude()
        {
            int c;
            struct ir_remote* r;

            std_setup();
            for (c = 0, r = config; r != NULL; r = r->next)
                c += 1;
            CPPUNIT_ASSERT(c == 4);
        }

        void testRawSorting()
        {
            std_setup();
            struct ir_remote* last = config->next->next->next;
            CPPUNIT_ASSERT(string(last->name) == "Melectronic_PP3600");
        }

        void testManualSorting()
        {
            system("cp etc/00-manual_sort.conf etc/lircd.conf.d");
            f = fopen(NAME, "r");
            CPPUNIT_ASSERT(f != NULL);
            config = read_config(f, NAME);
            CPPUNIT_ASSERT(config != NULL);
            unlink("etc/lircd.conf.d/00-manual_sort.conf");
            const char* last = config->next->next->next->next->name;
            CPPUNIT_ASSERT(string(last) == "Melectronic_PP3600");
        }


};

#endif

// vim: set expandtab ts=4 sw=4:
