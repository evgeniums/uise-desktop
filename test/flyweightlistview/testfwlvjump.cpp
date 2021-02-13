/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/test/flyweightlistview/testfwlvjump.cpp
*
*  Test jump in FlyweightListView.
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

void visibleItemsChanged(FwlvTestContext* ctx, const HelloWorldItemWrapper* begin,const HelloWorldItemWrapper* end)
{
    ++ctx->visibleItemsChangedCount;

    std::string msg=std::string("visibleItemsChanged, step ")+std::to_string(ctx->step);
    BOOST_TEST_CONTEXT(msg.c_str())
    {
        ctx->fillExpectedAfterLoad();
        ctx->expectedItemCount=ctx->view->prefetchItemCount();
        if (ctx->step==2)
        {
            ctx->fillExpectedIds(ctx->frontID(),ctx->backID(),0,true);
            BOOST_TEST_CONTEXT("After jump") {ctx->doChecks(false,true);}
        }
        else if (ctx->step==3)
        {
            ctx->fillExpectedIds(ctx->frontID(),ctx->backID());
            BOOST_TEST_CONTEXT("After jump back") {ctx->doChecks(false);}
        }

        UISE_TEST_REQUIRE(begin!=nullptr);
        UISE_TEST_REQUIRE(end!=nullptr);

        UISE_TEST_CHECK_EQUAL(begin->id(),ctx->expectedFirstVisibleItemId);
        UISE_TEST_CHECK_EQUAL(end->id(),ctx->expectedLastVisibleItemId);
    }
}

void jumpTo(FwlvTestContext* ctx, Direction direction, bool inverseDirection=false)
{
    if (direction==Direction::NONE)
    {
        direction=inverseDirection?Direction::HOME:Direction::END;
    }
    ctx->testWidget->pimpl->jumpMode->setCurrentIndex(ctx->testWidget->pimpl->jumpMode->findData(static_cast<int>(direction)));
    ctx->testWidget->pimpl->jumpToButton->click();
}

void checkJumpEdge(FwlvTestContext* ctx)
{
    ctx->testWidget->loadItems();
    ++ctx->step;

    QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
    [ctx]()
    {
        ctx->view->setViewportChangedCb(
            [ctx](const HelloWorldItemWrapper* begin,const HelloWorldItemWrapper* end)
            {
                visibleItemsChanged(ctx,begin,end);
            }
        );

        QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
        [ctx]()
        {
            ctx->fillExpectedAfterLoad();
            BOOST_TEST_CONTEXT("After load with delay") {ctx->doChecks();}

            ++ctx->step;
            if (ctx->stickMode==Direction::END)
            {
                jumpTo(ctx,Direction::HOME);
            }
            else
            {
                jumpTo(ctx,Direction::END);
            }

            QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
            [ctx]()
            {
                ctx->fillExpectedAfterLoad();
                ctx->expectedItemCount=ctx->view->prefetchItemCount();
                ctx->fillExpectedIds(ctx->frontID(),ctx->backID(),0,true);
                BOOST_TEST_CONTEXT("After jump") {ctx->doChecks(true,true);}

                ++ctx->step;
                jumpTo(ctx,ctx->stickMode,true);

                QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
                [ctx]()
                {
                    ctx->fillExpectedAfterLoad();
                    ctx->expectedItemCount=ctx->view->prefetchItemCount();
                    ctx->fillExpectedIds(ctx->frontID(),ctx->backID());
                    BOOST_TEST_CONTEXT("After jump back") {ctx->doChecks();}

                    UISE_TEST_CHECK_GE(ctx->visibleItemsChangedCount,2);

                    ctx->endTestCase();
               });
            });
        });
    });
}

}

BOOST_AUTO_TEST_CASE(TestJumpEdge)
{
    auto handler=[](FwlvTestContext* ctx)
    {
        checkJumpEdge(ctx);
    };
//    FwlvTestContext::execSingleMode(handler,Qt::Vertical,Direction::END,true);
    FwlvTestContext::execAllModes(handler);
}

BOOST_AUTO_TEST_SUITE_END()
