/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/test/utils/testmiscutils.cpp
*
*  Test miscellaneous utils.
*
*/

/****************************************************************************/

#include <boost/test/unit_test.hpp>

#include <QFrame>

#include <uise/test/uise-testthread.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/directchildwidget.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

BOOST_AUTO_TEST_SUITE(TestMiscUtils)

BOOST_AUTO_TEST_CASE(TestDirectChildWidget)
{
    auto handler=[]()
    {
        auto parent=new QFrame();
        parent->resize(800,600);
        auto parentLayout=Layout::vertical(parent);

        auto child=new QFrame(parent);
        parentLayout->addWidget(child,1);
        auto childLayout=Layout::vertical(child);

        auto subChild=new QFrame(child);
        childLayout->addWidget(subChild,1);

        auto notChild=new QFrame();
        notChild->resize(800,600);

        auto result=directChildWidget(parent,notChild);
        UISE_TEST_CHECK(result==nullptr);

        result=directChildWidget(parent,child);
        UISE_TEST_CHECK(result==child);

        result=directChildWidget(parent,subChild);
        UISE_TEST_CHECK(result==child);

        result=directChildWidgetAt(parent,QPoint(0,0));
        UISE_TEST_CHECK(result==child);

        delete parent;
        delete notChild;
        TestThread::instance()->continueTest();
    };

    TestThread::instance()->postGuiThread(handler);
    TestThread::instance()->execTest();
}

BOOST_AUTO_TEST_CASE(TestDestroyWidget)
{
    std::atomic<bool> widgetDestroyed{false};

    auto handler=[&widgetDestroyed]()
    {
        auto parent=new QFrame();
        parent->resize(800,600);
        auto parentLayout=Layout::vertical(parent);

        auto child=new QFrame(parent);
        parentLayout->addWidget(child,1);
        QObject::connect(
            child,
            &QFrame::destroyed,
            [parent,&widgetDestroyed]()
            {
                widgetDestroyed.store(true);
                parent->deleteLater();
                TestThread::instance()->continueTest();
            }
        );

        parent->show();
        parent->raise();

        UISE_TEST_CHECK(parent->isVisible());
        UISE_TEST_CHECK(child->isVisible());
        UISE_TEST_CHECK(child->parent()==parent);

        child->deleteLater();
        destroyWidget(child);
        UISE_TEST_CHECK(!child->isVisible());
        UISE_TEST_CHECK(child->parent()==nullptr);
    };

    TestThread::instance()->postGuiThread(handler);
    auto ret=TestThread::instance()->execTest(1000);
    UISE_TEST_CHECK(ret);
    UISE_TEST_CHECK(widgetDestroyed.load());
}

BOOST_AUTO_TEST_CASE(TestSingleShotTimer)
{
    std::atomic<int> value{0};
    std::atomic<int> value1{0};

    auto handler=[&value,&value1]()
    {
        UISE_TEST_MESSAGE("In handler")

        auto timerClear=new SingleShotTimer();
        timerClear->shot(1,
            [&value1]()
            {
                UISE_TEST_MESSAGE("In timer clear")
                value1.store(1);
            }
        );
        timerClear->clear();

        auto timer=new SingleShotTimer();

        timer->shot(300,
            [timer,&value,timerClear]()
            {
                UISE_TEST_MESSAGE("In timer 10")

                delete timerClear;

                value.store(10);

                timer->deleteLater();
                TestThread::instance()->continueTest();
            }
        );

        timer->shot(500,
            [timer,&value,timerClear]()
            {
                UISE_TEST_MESSAGE("In timer 100")

                delete timerClear;

                value.store(100);

                timer->deleteLater();
                TestThread::instance()->continueTest();
            }
        );

        timer->shot(10000,
            [timer,&value,timerClear]()
            {
                UISE_TEST_MESSAGE("In timer 10000")

                delete timerClear;

                value.store(10000);

                timer->deleteLater();
                TestThread::instance()->continueTest();
            }
        );
    };

    UISE_TEST_MESSAGE("Posting handler")
    TestThread::instance()->postGuiThread(handler);
    UISE_TEST_MESSAGE("Executing test")
    auto ret=TestThread::instance()->execTest(15000);
    UISE_TEST_MESSAGE("Return execTest")

    UISE_TEST_CHECK(ret);
    UISE_TEST_CHECK_EQUAL(value.load(),10000);
    UISE_TEST_CHECK_EQUAL(value1.load(),0);
}

BOOST_AUTO_TEST_SUITE_END()
