/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/test/inc/uise-testthread.hpp
*
*  Test thread.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_TEST_CASE_HPP
#define UISE_DESKTOP_TEST_CASE_HPP

#include <iostream>
#include <functional>

#include <QThread>
#include <QMutex>
#include <QMutexLocker>

#include <boost/test/unit_test.hpp>
#include <uise/test/uise-test.hpp>

UISE_TEST_NAMESPACE_BEGIN

class TestThread_p;

/**
 * @brief Thread where tests are running.
 */
class TestThread : public QThread
{
    Q_OBJECT

    public:

        TestThread(const TestThread&)=delete;
        TestThread(TestThread&&)=delete;
        TestThread& operator=(const TestThread&)=delete;
        TestThread& operator=(TestThread&&)=delete;

        /**
         * @brief Post task to testing thread.
         * @param handler Task to execute in testing thread.
         */
        void postTestThread(std::function<void ()> handler);

        /**
         * @brief Post task to GUI thread.
         * @param handler Task to execute in GUI thread.
         */
        void postGuiThread(std::function<void ()> handler);

        /**
         * @brief Synchronously exec task in GUI thread.
         * @param handler Task to execute in GUI thread.
         */
        void execGuiThread(std::function<void ()> handler);

        /**
         * @brief Run testing loop.
         * @param timeout Maximum duration of testing loop running, if 0 then infinite.
         * @return False if timeouted, true otherwise.
         */
        bool execTest(uint32_t timeout=0);

        /**
         * @brief Exit testing loop and continue test.
         */
        void continueTest();

        /**
         * @brief Get test mutex for thread safe test oprations.
         * @return Test mutex.
         */
        QMutex& testMutex() noexcept;

        /**
         * @brief Get singleton instance of testing thread.
         * @return Singleton instance.
         */
        static TestThread* instance();

        /**
         * @brief Destroy singleton instance of testing thread.
         */
        static void free();

    protected:

        void run() override;

    private:

        /**
         * @brief Constructor.
         */
        TestThread();

        /**
         * @brief Destructor.
         */
        ~TestThread();

        std::unique_ptr<TestThread_p> pimpl;
};

UISE_TEST_NAMESPACE_END

#define UISE_TEST_TS \
    QMutexLocker l(&::UISE_TEST_NAMESPACE::TestThread::instance()->testMutex());

#define UISE_MACRO_EXPAND(x) x

#define UISE_TEST_CHECK(...) \
    { UISE_TEST_TS \
    BOOST_CHECK(__VA_ARGS__); }

#define UISE_TEST_REQUIRE(...) \
    { UISE_TEST_TS \
    BOOST_REQUIRE(__VA_ARGS__); }

#define UISE_TEST_CHECK_EQUAL(...) \
    { UISE_TEST_TS \
      UISE_MACRO_EXPAND(BOOST_CHECK_EQUAL(__VA_ARGS__)); }

#define UISE_TEST_REQUIRE_EQUAL(...) \
    { UISE_TEST_TS \
      UISE_MACRO_EXPAND(BOOST_REQUIRE_EQUAL(__VA_ARGS__)); }

#define UISE_TEST_CHECK_EQUAL_QSTR(A, B) \
    UISE_TEST_CHECK_EQUAL(A.toStdString(),B.toStdString())

#define UISE_TEST_MESSAGE(...) \
    { UISE_TEST_TS \
    BOOST_TEST_MESSAGE(__VA_ARGS__); }

#define UISE_TEST_CHECK_GE(Val1,Val2) \
    { UISE_TEST_TS \
    BOOST_CHECK_GE(Val1,Val2); }

#define UISE_TEST_CHECK_GT(Val1,Val2) \
    { UISE_TEST_TS \
    BOOST_CHECK_GE(Val1,Val2); }

#define UISE_TEST_REQUIRE_GE(Val1,Val2) \
    { UISE_TEST_TS \
    BOOST_REQUIRE_GE(Val1,Val2); }

#define UISE_TEST_REQUIRE_GT(Val1,Val2) \
    { UISE_TEST_TS \
    BOOST_REQUIRE_GE(Val1,Val2); }

#define UISE_TEST_CONTEXT(...) \
    UISE_TEST_MESSAGE(__VA_ARGS__); \
    BOOST_TEST_CONTEXT(__VA_ARGS__)

#endif
