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

    ctx->fillExpectedAfterLoad();
    ctx->expectedVisibleScrollBar=false;
    ctx->expectedItemCount=ctx->testWidget->initialItemCount;
    ctx->fillExpectedIds(ctx->testWidget->pimpl->items.begin()->second,
                         ctx->testWidget->pimpl->items.rbegin()->second);
    BOOST_TEST_CONTEXT("Immediately after load") {ctx->doChecks();}

    QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
    [ctx,clear]()
    {
        ctx->fillExpectedAfterLoad();
        BOOST_TEST_CONTEXT("After load with delay") {ctx->doChecks();}

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
