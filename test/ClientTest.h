#ifndef  CLIENT_TEST
#define  CLIENT_TEST

#include	<stdio.h>
#include	<signal.h>
#include 	<netinet/in.h>
#include	<sys/socket.h>
#include	<sys/types.h>
#include	<sys/un.h>

#include    <iostream>
#include    <unordered_map>
#include    <cppunit/TestFixture.h>
#include    <cppunit/TestSuite.h>
#include    <cppunit/TestCaller.h>

#include	"../lib/lirc_private.h"
#include	"../lib/lirc_client.h"

#define     SEND_DELAY    2000000

#undef      ADD_TEST
#define     ADD_TEST(id, func) \
    testSuite->addTest(new CppUnit::TestCaller<ClientTest>( \
                       id,  &ClientTest::func))

#define     RUN_LIRCD   "../daemons/lircd -O client_test.conf \
                        etc/lircd.conf.Aspire_6530G"

#define     RUN_LIRCRCD "../tools/lircrcd -o var/lircrcd.socket \
                        etc/mythtv.lircrc"

#define IRSEND         " ../tools/irsend -d var/lircd.socket  SIMULATE  \
                        \"000000000000%s 00 %s Acer_Aspire_6530G_MCE\""

using namespace std;

class ClientTest : public CppUnit::TestFixture
{
    private:
        int fd;

    public:
        static CppUnit::Test* suite()
        {
            CppUnit::TestSuite* testSuite =
                 new CppUnit::TestSuite( "ClientTest" );
            ADD_TEST("testReceive", testReceive);
            ADD_TEST("testReadConfig", testReadConfig);
            ADD_TEST("testReadConfigOnly", testReadConfig);
            ADD_TEST("testCode2Char", testCode2Char);
            ADD_TEST("testSetMode", testSetMode);
            ADD_TEST("testGetMode", testSetMode);
            return testSuite;
        };

        void sendCode(int fd,  int code, const char* symbol, int when)
        // Send symbol and its code after delay "when" microseconds in a
        // separate process.
        {
            char buff[256];
            int status;
            pid_t pid;

            pid = fork();
            if (pid == 0){
                usleep(when);
                lirc_simulate(fd,
                              "Acer_Aspire_6530G_MCE",
                              symbol, code, 0);
                exit(0);
            } else if (pid == -1){
                cout << "Cannot fork(!)\n";
            }
        }

        void setUp()
        {
            char s[128];
            if (access("var/lircd.pid", R_OK) == 0) {
                ifstream pidfile("var/lircd.pid");
                stringstream buffer;

                buffer << pidfile.rdbuf();
                int pid;
                buffer >> pid;

                if (kill(pid, SIGTERM) == 0)
                    usleep(100);
            }
            string path = string("client.log");
            lirc_log_set_file(path.c_str());
            lirc_log_open("ClientTest", 0, LIRC_STALK);

            unlink("var/file-driver.out");

            int status;
            status = system(RUN_LIRCD);
            setenv("LIRC_SOCKET_PATH", "var/lircd.socket", 1);
            lirc_deinit();
            fd = lirc_init("mythtv", 1);
                usleep(2000);
            CPPUNIT_ASSERT(fd != -1);
        };

        void tearDown()
        {
            char s[128];
            ifstream pidfile("var/lircd.pid");
            stringstream buffer;

            usleep(10000);
            buffer << pidfile.rdbuf();
            int pid;
            buffer >> pid;
            if( kill(pid, SIGTERM) == 0)
                usleep(500);
            else {
                snprintf(s, sizeof(s), "Cannot kill lircd (%d).", pid);
                perror(s);
            }
            lirc_log_close();
        };


        void testReadConfig()
        {
            struct lirc_config* config;

            CPPUNIT_ASSERT(lirc_readconfig(abspath("etc/mythtv.lircrc"),
                                           &config, NULL) == 0);
        }


        void testReceive()
        {
            char* code = NULL;
            int status = 0;

            sendCode(fd, 0x1bf3, "KEY_POWER", SEND_DELAY);

            setenv("LIRC_SOCKET_PATH", "var/lircd.socket", 0);
            while(code == NULL && status == 0) {
                status = lirc_nextcode(&code);
            }
            CPPUNIT_ASSERT(status == 0);
            if (getenv("LIRC_TEST_DEBUG") != NULL)
                cout << "Code: " << code << "\n";
            else if (string(code).find("1bf3") == string::npos)
                cout << "Strange packet: \"" << code << "\"\n";
            CPPUNIT_ASSERT(string(code).find("1bf3") != string::npos);
            free(code);
        };


        void testReadConfigOnly()
        {
            struct lirc_config* config;
            CPPUNIT_ASSERT(
                lirc_readconfig_only("etc/mythtv.lircrc", &config, NULL) == 0);
        }


        void testCode2Char()
        {
            char* code_txt =
                (char*) "0000000000001bde 00 KEY_RIGHT Acer_Aspire_6530G_MCE";
            struct lirc_config* config;
            char* chars = NULL;

            CPPUNIT_ASSERT(lirc_readconfig(abspath("etc/mythtv.lircrc"),
                                           &config, NULL) == 0);
            CPPUNIT_ASSERT(fd != -1);
            config->sockfd = -1;
            lirc_code2char(config, code_txt, (char**)&chars);
            if (getenv("LIRC_TEST_DEBUG") != NULL)
                cout << "String: " << chars << "\n";
            CPPUNIT_ASSERT(string(chars) == "Right" );

        }


        void testSetMode()
        {
            struct lirc_config* config;
            int status;
            int fd;
            const char* retval;

            status = system(RUN_LIRCRCD);
            CPPUNIT_ASSERT(status == 0);
            fd  = lirc_get_local_socket("var/lircrcd.socket", 0);
            CPPUNIT_ASSERT(fd != -1);
            status = lirc_readconfig("etc/mythtv.lircrc", &config, NULL);
            CPPUNIT_ASSERT(status == 0);
            config->sockfd = fd;
            retval = lirc_setmode(config, "numeric");
            CPPUNIT_ASSERT(retval != NULL);
            CPPUNIT_ASSERT(string(retval) == "numeric");
        }

        void testGetMode()
        {
            struct lirc_config* config;
            int status;
            int fd;
            const char* retval;

            status = system(RUN_LIRCRCD);
            CPPUNIT_ASSERT(status == 0);
            setenv("LIRC_SOCKET_PATH", "var/lircd.socket", 1);
            fd  = lirc_get_local_socket(NULL, 0);
            CPPUNIT_ASSERT(fd != -1);
            status = lirc_readconfig("etc/mythtv.lircrc", &config, NULL);
            CPPUNIT_ASSERT(status == 0);
            config->sockfd = fd;
            retval = lirc_getmode(config);
            CPPUNIT_ASSERT(retval != NULL);
            CPPUNIT_ASSERT(string(retval) == "numeric");
        }



        void testDefaults()
        {
        };

};

#endif

// vim: set expandtab ts=4 sw=4:
