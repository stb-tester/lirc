#ifndef  CLIENT_TEST
#define  CLIENT_TEST

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
    testSuite->addTest(new CppUnit::TestCaller<ClientTest>( \
                       id,  &ClientTest::func))

#define     RUN_LIRCD   "../daemons/lircd \
                        --plugindir=../plugins/.libs \
                        --pidfile=var/lircd.pid \
                        --logfile=client.log \
                        --output=var/lircd.socket  \
                        --allow-simulate \
                        --driver devinput \
                        etc/lircd.conf.Aspire_6530G"

#define IRSEND         " ../tools/irsend -d var/lircd.socket  SIMULATE  \
                        \"000000000000%s 00 %s Acer_Aspire_6530G_MCE\""

using namespace std;

class ClientTest : public CppUnit::TestFixture
{
    private:

    public:
        static CppUnit::Test* suite()
        {
            CppUnit::TestSuite* testSuite =
                 new CppUnit::TestSuite( "ClientTest" );
            ADD_TEST("testReceive", testReceive);
            ADD_TEST("testReadConfig", testReadConfig);
            return testSuite;
        };

        static void sendCode(const char* code, const char* symbol, int when)
        // Send symbol and its code after delay "when" microseconds in a
        // separate process.
        {
            char buff[256];
            int status;
            pid_t pid;

            pid = fork();
            if (pid == 0){
                usleep(when);
                sprintf(buff, IRSEND, code, symbol);
                status = system(buff);
                if (status != 0)
                    cout << "Send status: " << status << "\n";
                _exit(0);
            } else if (pid == -1){
                cout << "Cannot fork(!)\n";
            }
        }

        void setUp()
        {
            string path = string("client.log");
            lirc_log_set_file(path.c_str());
            lirc_log_open("ClientTest", 0, 10);

            int status;
            status = system(RUN_LIRCD);
            setenv("LIRC_SOCKET_PATH", "var/lircd.socket", 0);
            lirc_deinit();
            CPPUNIT_ASSERT(lirc_init("client_test", 1) != -1);
        };

        void tearDown()
        {
            ifstream pidfile("var/lircd.pid");
            stringstream buffer;
            buffer << pidfile.rdbuf();
            int pid;
            buffer >> pid;
            if( kill(pid, SIGTERM) == 0)
                usleep(500);
            else
                perror("Cannot kill lircd.");
            lirc_log_close();
        };

        void testReadConfig()
        {
            struct lirc_config* config;

            setenv("LIRC_SOCKET_PATH", abspath("var/lircd.socket"), 1);
            lirc_deinit();
            CPPUNIT_ASSERT(lirc_init("client_test", 1) != -1);
            CPPUNIT_ASSERT(lirc_readconfig(abspath("etc/mythtv.lircrc"),
                                           &config, NULL) == 0);
        }


        void testReceive()
        {
            char* code;

            sendCode("1bf3", "KEY_POWER", 100);

            setenv("LIRC_SOCKET_PATH", "var/lircd.socket", 0);
            CPPUNIT_ASSERT(lirc_nextcode(&code) == 0);
            CPPUNIT_ASSERT(string(code).find("1bf3") != string::npos);
            //cout << "Code: " << code << "\n";
        };

        void testDefaults()
        {
        };

};

#endif

// vim: set expandtab ts=4 sw=4:
