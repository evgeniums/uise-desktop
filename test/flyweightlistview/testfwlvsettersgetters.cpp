/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/test/flyweightlistview/testfwlvsettersgetters.cpp
*
*  Test setters/getters of FlyweightListView.
*
*/

/****************************************************************************/

#include <boost/test/unit_test.hpp>

#include <uise/test/uise-testthread.hpp>

#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/utils/destroywidget.hpp>

#include "fwlvtestwidget.hpp"

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

BOOST_AUTO_TEST_SUITE(TestFlyWeightListView1)

namespace {
void checkSettersGetters(FlwListType* view)
{
    UISE_TEST_CHECK_EQUAL(view->prefetchItemCountHint(),FlwListType::PrefetchItemCountHint);
    UISE_TEST_CHECK_EQUAL(view->prefetchItemCount(),FlwListType::PrefetchItemCountHint);
    UISE_TEST_CHECK_EQUAL(view->visibleItemCount(),0);
    UISE_TEST_CHECK_EQUAL(view->itemCount(),0);
    UISE_TEST_CHECK_EQUAL(view->hasItem(0),false);
    UISE_TEST_CHECK_EQUAL(view->item(0),nullptr);
    UISE_TEST_CHECK_EQUAL(view->firstItem(),nullptr);
    UISE_TEST_CHECK_EQUAL(view->lastItem(),nullptr);

    UISE_TEST_CHECK_EQUAL(view->orientation(),Qt::Vertical);
    view->setOrientation(Qt::Horizontal);
    UISE_TEST_CHECK_EQUAL(view->orientation(),Qt::Horizontal);

    UISE_TEST_CHECK_EQUAL(view->isFlyweightEnabled(),true);
    view->setFlyweightEnabled(false);
    UISE_TEST_CHECK_EQUAL(view->isFlyweightEnabled(),false);

    UISE_TEST_CHECK_EQUAL(static_cast<int>(view->stickMode()),static_cast<int>(Direction::END));
    view->setStickMode(Direction::HOME);
    UISE_TEST_CHECK_EQUAL(static_cast<int>(view->stickMode()),static_cast<int>(Direction::HOME));

    UISE_TEST_CHECK_EQUAL(view->singleScrollStep(),1);
    view->setSingleScrollStep(5);
    UISE_TEST_CHECK_EQUAL(view->singleScrollStep(),5);

    UISE_TEST_CHECK_EQUAL(view->minPageScrollStep(),FlwListType::DefaultPageStep);
    UISE_TEST_CHECK_EQUAL(view->pageScrollStep(),FlwListType::DefaultPageStep);
    view->setMinPageScrollStep(25);
    UISE_TEST_CHECK_EQUAL(view->minPageScrollStep(),25);
    UISE_TEST_CHECK_EQUAL(view->pageScrollStep(),25);

    UISE_TEST_CHECK_EQUAL(view->horizontalScrollBarPolicy(),Qt::ScrollBarAsNeeded);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    UISE_TEST_CHECK_EQUAL(view->horizontalScrollBarPolicy(),Qt::ScrollBarAlwaysOn);
    UISE_TEST_CHECK_EQUAL(view->verticalScrollBarPolicy(),Qt::ScrollBarAsNeeded);
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    UISE_TEST_CHECK_EQUAL(view->verticalScrollBarPolicy(),Qt::ScrollBarAlwaysOff);

    UISE_TEST_CHECK(view->verticalScrollBar()!=nullptr);
    UISE_TEST_CHECK(view->horizontalScrollBar()!=nullptr);

    UISE_TEST_CHECK_EQUAL(view->maxSortValue(),0);
    UISE_TEST_CHECK_EQUAL(view->minSortValue(),0);
    view->setMaxSortValue(1000);
    view->setMinSortValue(100);
    UISE_TEST_CHECK_EQUAL(view->maxSortValue(),1000);
    UISE_TEST_CHECK_EQUAL(view->minSortValue(),100);
}
}

BOOST_AUTO_TEST_CASE(TestSettersGetters)
{
    auto handler=[]()
    {
        auto v=new FwlvTestWidget();
        checkSettersGetters(v->pimpl->view);
        QTimer::singleShot(
            0,
            v,
            [v]
            {
                destroyWidget(v);
                TestThread::instance()->continueTest();
            }
        );
    };

    TestThread::instance()->postGuiThread(handler);
    auto ret=TestThread::instance()->execTest(15000);
    UISE_TEST_CHECK(ret);
}

BOOST_AUTO_TEST_SUITE_END()
