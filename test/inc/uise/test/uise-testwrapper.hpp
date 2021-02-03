/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/test/inc/testwrapper.hpp
*
*  Test wrapper.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_TEST_WRAPPER_HPP
#define UISE_DESKTOP_TEST_WRAPPER_HPP

#include <iostream>
#include <atomic>

#include <QApplication>
#include <QMainWindow>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

#define BOOST_TEST_MODULE UISE_TEST_MODULE
#define BOOST_TEST_NO_MAIN
#include <boost/test/unit_test.hpp>

#include <uise/test/uise-test.hpp>
#include <uise/test/uise-testthread.hpp>

#include <uise/desktop/utils/singleshottimer.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

UISE_TEST_NAMESPACE_BEGIN

//--------------------------------------------------------------------------
inline std::string testAppName()
{
    return QFileInfo(QCoreApplication::applicationFilePath()).baseName().toStdString();
}

//--------------------------------------------------------------------------
inline int testConsole()
{
    auto appName=testAppName();
    std::cerr<<"Running test "<<appName<<std::endl;

    const char* argv[]={appName.c_str(),"--log_level=test_suite"};
    int argc = 2;
    return boost::unit_test::unit_test_main(init_unit_test, argc, const_cast<char**>(argv));
}

//--------------------------------------------------------------------------
inline int testJUnit()
{
    auto appName=testAppName();
    std::cerr<<"Running test "<<appName<<std::endl;

    auto junitPath=std::string(UISE_TEST_JUNIT_PATH);
    auto junitLogger=std::string("--logger=JUNIT,all,")+junitPath+appName+".xml";
    std::cerr<<"JUnit logger "<<junitLogger<<std::endl;

    int argc=6;
    const char* argv[]={
                  appName.c_str(),
                  "--logger=HRF,test_suite",
                  junitLogger.c_str(),
                  "--report_level=no",
                  "--result_code=no",
                  "--detect_memory_leaks=0"};
    return boost::unit_test::unit_test_main(init_unit_test, argc, const_cast<char**>(argv));
}

//--------------------------------------------------------------------------
inline int runTest(int argc, char *argv[])
{
    QApplication app(argc,argv);
    app.setQuitOnLastWindowClosed(false);

    std::atomic<int> ret{0};

    TestThread::instance()->postTestThread(
        [&ret]()
        {
        #ifdef UISE_TEST_JUNIT
            ret=testJUnit();
        #else
            ret=testConsole();
        #endif
        }
    );

    app.exec();

    TestThread::free();
    return ret;
}

//--------------------------------------------------------------------------
struct TestGlobalFixture
{

  TestGlobalFixture()
  {
  }
  ~TestGlobalFixture()
  {
  }

  void setup()
  {
  }

  void teardown()
  {
      TestThread::instance()->postGuiThread(
        []()
        {
            qApp->quit();
        }
      );
  }
};
BOOST_TEST_GLOBAL_FIXTURE(TestGlobalFixture);

#endif

UISE_TEST_NAMESPACE_END
