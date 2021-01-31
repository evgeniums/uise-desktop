#ifndef UISE_DESKTOP_TEST_WRAPPER_HPP
#define UISE_DESKTOP_TEST_WRAPPER_HPP

#include <iostream>

#include <QApplication>
#include <QMainWindow>
#include <QFileInfo>
#include <QDir>

#define BOOST_TEST_ALTERNATIVE_INIT_API
#include <boost/test/unit_test.hpp>

#include <uise/desktop/uisedesktop.hpp>
using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------
inline bool init_unit_test()
{
  return true;
}

//--------------------------------------------------------------------------
inline const char* testAppName()
{
    return QFileInfo(QCoreApplication::applicationFilePath()).baseName().toStdString().c_str();
}

//--------------------------------------------------------------------------
inline int testConsole()
{
    auto appName=testAppName();
    std::cerr<<"Running test "<<appName<<std::endl;

    const char* argv[]={appName,"--log_level=test_suite"};
    int argc = 2;
    return boost::unit_test::unit_test_main(::init_unit_test, argc, const_cast<char**>(argv));
}

//--------------------------------------------------------------------------
inline int testJUnit()
{
    auto appName=testAppName();
    std::cerr<<"Running test "<<appName<<std::endl;

    auto dir=QDir(QCoreApplication::applicationDirPath());
    dir.cdUp();
    auto junitPath=dir.path()+"/junit";
    auto path=std::string("--log_sink=")+junitPath.toStdString()+"/"+appName+".xml";
    std::cerr<<"JUnit path "<<path<<std::endl;

    int argc=6;
    const char* argv[]={
                appName,
                "--log_format=JUNIT",
                  path.c_str(),
                  "--log_level=all",
                  "--report_level=no",
                  "--result_code=no"};
    return boost::unit_test::unit_test_main(::init_unit_test, argc, const_cast<char**>(argv));
}

//--------------------------------------------------------------------------
int runTest(int argc, char *argv[])
{
    QApplication app(argc,argv);
    QMainWindow w;
    w.show();

#ifdef UISE_TEST_JUNIT
    auto ret=testJUnit();
#else
    auto ret=testConsole();
#endif

    app.exec();
    return ret;
}

#endif
