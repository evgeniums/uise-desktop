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

BOOST_AUTO_TEST_SUITE_END()
