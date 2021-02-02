/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/test/inc/testcase.hpp
*
*  Test case.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_TEST_CASE_HPP
#define UISE_DESKTOP_TEST_CASE_HPP

#include <iostream>
#include <functional>

#include <QThread>

#include <boost/test/unit_test.hpp>
#include <uise/test/uise-test.hpp>

UISE_TEST_NAMESPACE_BEGIN

class TestThread_p;

class TestThread : public QThread
{
    Q_OBJECT

    public:

        TestThread(const TestThread&)=delete;
        TestThread(TestThread&&)=delete;
        TestThread& operator=(const TestThread&)=delete;
        TestThread& operator=(TestThread&&)=delete;

        void postTestThread(std::function<void ()> handler);
        void postGuiThread(std::function<void ()> handler);

        bool execTest(uint32_t timeout=0);
        void continueTest();

        static TestThread* instance();
        static void free();

    protected:

        void run() override;

    private:

        TestThread();
        ~TestThread();

        std::unique_ptr<TestThread_p> pimpl;
};

UISE_TEST_NAMESPACE_END

#endif
