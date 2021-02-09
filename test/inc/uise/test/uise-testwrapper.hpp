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
inline void testConsole(std::vector<std::string>& args)
{
    args.push_back("--log_level=test_suite");
}

//--------------------------------------------------------------------------
inline void testJUnit(const std::string& appName,std::vector<std::string>& args)
{
    auto junitPath=std::string(UISE_TEST_JUNIT_PATH);
    auto junitLogger=std::string("--logger=JUNIT,all,")+junitPath+appName+".xml";
    std::cerr<<"JUnit logger "<<junitLogger<<std::endl;

    std::vector<std::string> extraArgs{                  
                  junitLogger,
                  "--logger=HRF,error,stdout",
                  "--report_level=no",
                  "--result_code=no",
                  "--detect_memory_leaks=0"};
    args.insert(std::end(args),std::begin(extraArgs),std::end(extraArgs));
}

//--------------------------------------------------------------------------
inline int runTest(int argc, char *argv[])
{
    QApplication app(argc,argv);
    app.setQuitOnLastWindowClosed(false);

    std::atomic<int> ret{0};

    QString testName;
    auto args=app.arguments();
    foreach(const QString& arg, args)
    {
        if (arg.startsWith("--run_test="))
        {
            testName=arg;
            break;
        }
    }

    TestThread::instance()->postTestThread(
        [&ret,testName]()
        {        
            auto appName=testAppName();
            auto test=testName.toStdString();
            std::cerr<<"Running test "<<appName<<" "<<test<<std::endl;

            std::vector<std::string> args{appName};
            #ifdef UISE_TEST_JUNIT
                testJUnit(appName,args);
            #else
                testConsole(args);
            #endif

            if (!test.empty())
            {
                args.push_back(test);
            }

            auto argv=new const char* [args.size()];
            for (size_t i=0;i<args.size();i++)
            {
                argv[i]=args[i].c_str();
            }
            ret=boost::unit_test::unit_test_main(init_unit_test, args.size(), const_cast<char**>(argv));
            delete [] argv;
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
