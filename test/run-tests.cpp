#include        <cppunit/TestSuite.h>
#include        <cppunit/TestResult.h>
#include        <cppunit/ui/text/TestRunner.h>

#include        "Util.h"
#include        "IrRemoteTest.h"
#include        "LogTest.h"
#include        "OptionsTest.h"
#include        "ClientTest.h"
#include        "DrvAdminTest.h"
#include        "DecodeTest.h"


int main()
{
        CppUnit::TextUi::TestRunner runner;
        runner.addTest(IrRemoteTest::suite());
        runner.addTest(LogTest::suite());
        runner.addTest(OptionsTest::suite());
        runner.addTest(ClientTest::suite());
        runner.addTest(DrvAdminTest::suite());
        runner.addTest(DecodeTest::suite());
        runner.run();
        system("pkill lircd");
        unlink("var/lircd.pid");
        return 0;
};
