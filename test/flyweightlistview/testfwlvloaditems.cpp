/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/test/flyweightlistview/testfwlvloaditems.cpp
*
*  Test loading items into FlyweightListView.
*
*/

/****************************************************************************/

#include <boost/test/unit_test.hpp>

#include <uise/test/uise-testthread.hpp>

#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/utils/destroywidget.hpp>

#include "fwlvtestwidget.hpp"
#include "fwlvtestcontext.hpp"

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

BOOST_AUTO_TEST_SUITE(TestFlyWeightListView)

namespace {

void doChecks(FwlvTestContext* ctx, size_t actualCount)
{
    UISE_TEST_CHECK_EQUAL(ctx->view->itemCount(),actualCount);

    const auto* firstItem=ctx->view->firstItem();
    UISE_TEST_REQUIRE(firstItem!=nullptr);
    const auto* firstViewportItem=ctx->view->firstViewportItem();
    UISE_TEST_REQUIRE(firstViewportItem!=nullptr);

    const auto* lastItem=ctx->view->lastItem();
    UISE_TEST_REQUIRE(lastItem!=nullptr);
    const auto* lastViewportItem=ctx->view->lastViewportItem();
    UISE_TEST_REQUIRE(lastViewportItem!=nullptr);

    UISE_TEST_CHECK(ctx->view->hasItem(firstItem->id()));
    UISE_TEST_CHECK(ctx->view->hasItem(lastItem->id()));
    UISE_TEST_CHECK(ctx->view->item(firstItem->id())==firstItem);
    UISE_TEST_CHECK(ctx->view->item(lastItem->id())==lastItem);

    auto frontID=ctx->testWidget->pimpl->items.begin()->second;
    auto backID=ctx->testWidget->pimpl->items.rbegin()->second;

    if (ctx->stickMode==Direction::END)
    {
        UISE_TEST_CHECK(firstItem!=firstViewportItem);
        UISE_TEST_CHECK(lastItem==lastViewportItem);
        UISE_TEST_CHECK_EQUAL(firstItem->id(),backID+actualCount-1);
        UISE_TEST_CHECK_EQUAL(lastItem->id(),backID);

        if (ctx->isHorizontal())
        {
            UISE_TEST_CHECK_GE(ctx->view->visibleItemCount(),3);
            UISE_TEST_CHECK_EQUAL(firstViewportItem->id(),backID+ctx->view->visibleItemCount()-1);
        }
        else
        {
            UISE_TEST_CHECK_GE(ctx->view->visibleItemCount(),3);
            UISE_TEST_CHECK_EQUAL(firstViewportItem->id(),backID+ctx->view->visibleItemCount()-1);
        }

        UISE_TEST_CHECK(ctx->view->isScrollAtEdge(Direction::END));
        UISE_TEST_CHECK(!ctx->view->isScrollAtEdge(Direction::HOME));
    }
    else
    {
        UISE_TEST_CHECK(firstItem==firstViewportItem);
        UISE_TEST_CHECK(lastItem!=lastViewportItem);
        UISE_TEST_CHECK_EQUAL(firstItem->id(),frontID);
        UISE_TEST_CHECK_EQUAL(lastItem->id(),frontID-actualCount+1);

        if (ctx->isHorizontal())
        {
            UISE_TEST_CHECK_GE(ctx->view->visibleItemCount(),3);
            UISE_TEST_CHECK_EQUAL(lastViewportItem->id(),frontID-ctx->view->visibleItemCount()+1);
        }
        else
        {
            UISE_TEST_CHECK_GE(ctx->view->visibleItemCount(),3);
            UISE_TEST_CHECK_EQUAL(lastViewportItem->id(),frontID-ctx->view->visibleItemCount()+1);
        }

        UISE_TEST_CHECK(!ctx->view->isScrollAtEdge(Direction::END));
        UISE_TEST_CHECK(ctx->view->isScrollAtEdge(Direction::HOME));
    }
}

void checkLoadItems(FwlvTestContext* ctx, bool clear);

void checkClearItems(FwlvTestContext* ctx)
{
    ctx->testWidget->pimpl->clearButton->click();

    QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
    [ctx]()
    {
        checkLoadItems(ctx,false);
    });
}

void checkLoadItems(FwlvTestContext* ctx, bool clear)
{
    UISE_TEST_CHECK(!ctx->view->horizontalScrollBar()->isVisible());
    UISE_TEST_CHECK(!ctx->view->verticalScrollBar()->isVisible());
    UISE_TEST_CHECK_EQUAL(ctx->view->itemCount(),0);
    UISE_TEST_CHECK_EQUAL(ctx->view->visibleItemCount(),0);
    const auto* firstItem=ctx->view->firstItem();
    UISE_TEST_CHECK(firstItem==nullptr);
    const auto* firstViewportItem=ctx->view->firstViewportItem();
    UISE_TEST_CHECK(firstViewportItem==nullptr);
    const auto* lastItem=ctx->view->lastItem();
    UISE_TEST_CHECK(lastItem==nullptr);
    const auto* lastViewportItem=ctx->view->lastViewportItem();
    UISE_TEST_CHECK(lastViewportItem==nullptr);

    ctx->testWidget->loadItems();
    doChecks(ctx,ctx->testWidget->initialItemCount);

    QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
    [ctx,clear]()
    {
        if (ctx->isHorizontal())
        {
            UISE_TEST_CHECK(ctx->view->horizontalScrollBar()->isVisible());
            UISE_TEST_CHECK(!ctx->view->verticalScrollBar()->isVisible());
        }
        else
        {
            UISE_TEST_CHECK(!ctx->view->horizontalScrollBar()->isVisible());
            UISE_TEST_CHECK(ctx->view->verticalScrollBar()->isVisible());
        }

        if (ctx->flyweightMode)
        {
            doChecks(ctx,ctx->testWidget->initialItemCount+ctx->view->prefetchItemCount());
        }
        else
        {
            doChecks(ctx,ctx->testWidget->initialItemCount);
        }

        if (clear)
        {
            checkClearItems(ctx);
        }
        else
        {
            ctx->endTestCase();
        }
    });
}

}

BOOST_AUTO_TEST_CASE(TestLoadItems)
{
    auto handler=[](FwlvTestContext* ctx)
    {
        checkLoadItems(ctx,true);
    };
    FwlvTestContext::execAllModes(handler);
}

BOOST_AUTO_TEST_SUITE_END()
