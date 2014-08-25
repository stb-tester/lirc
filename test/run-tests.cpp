#include        <cppunit/TestSuite.h>
#include        <cppunit/TestResult.h>
#include        <cppunit/ui/text/TestRunner.h>

#include        "Util.h"
#include        "IrRemoteTest.h"
#include        "LogTest.h"
#include        "OptionsTest.h"
#include        "ClientTest.h"
#include        "DrvAdminTest.h"

int main()
{
        CppUnit::TextUi::TestRunner runner;
        runner.addTest(IrRemoteTest::suite());
        runner.addTest(LogTest::suite());
        runner.addTest(OptionsTest::suite());
        runner.addTest(ClientTest::suite());
        runner.addTest(DrvAdminTest::suite());
        runner.run();
        return 0;
};
