/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/test/flyweightlistview/testfwlvresize.cpp
*
*  Test resizing FlyweightListView.
*
*/

/****************************************************************************/

#include <boost/test/unit_test.hpp>

#include <QTest>
#include <QScrollBar>

#include <uise/test/uise-testthread.hpp>

#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/utils/destroywidget.hpp>

#include "fwlvtestwidget.hpp"
#include "fwlvtestcontext.hpp"

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

BOOST_AUTO_TEST_SUITE(TestFlyWeightListView)

namespace {

bool BlockCheckVisibleItemsChanged=false;

int signedDelta(FwlvTestContext* ctx, int delta) noexcept
{
    return ctx->stickMode==Direction::END?-delta:delta;
}

int idOffset(FwlvTestContext* ctx, int offset) noexcept
{
    return ctx->stickMode==Direction::END?offset:-offset;
}

void visibleItemsChanged(FwlvTestContext* ctx, const HelloWorldItemWrapper* begin,const HelloWorldItemWrapper* end)
{
    if (BlockCheckVisibleItemsChanged)
    {
        return;
    }
    ++ctx->visibleItemsChangedCount;

    if (ctx->step==0 || ctx->step==3)
    {
        ctx->fillExpectedAfterLoad();
    }
    else
    {
        ctx->fillExpectedIds(ctx->frontID(),ctx->backID(),idOffset(ctx,1));
        ctx->scrollAtEdge=false;
    }

    std::string msg=std::string("visibleItemsChanged, step ")+std::to_string(ctx->step);
    BOOST_TEST_CONTEXT(msg.c_str())
    {
        ctx->doChecks(false);

        UISE_TEST_REQUIRE(begin!=nullptr);
        UISE_TEST_REQUIRE(end!=nullptr);

        UISE_TEST_CHECK_EQUAL(begin->id(),ctx->expectedFirstVisibleItemId);
        UISE_TEST_CHECK_EQUAL(end->id(),ctx->expectedLastVisibleItemId);
    }
}

void init(FwlvTestContext* ctx)
{
    ctx->testWidget->loadItems();
    ++ctx->step;

    ctx->view->setViewportChangedCb(
        [ctx](const HelloWorldItemWrapper* begin,const HelloWorldItemWrapper* end)
        {
            visibleItemsChanged(ctx,begin,end);
        }
    );
}

void checkScrollShort(std::function<void (FwlvTestContext* ctx, int delta)> scrollHandler)
{
    auto handler=[scrollHandler](FwlvTestContext* ctx)
    {
        init(ctx);

        QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
        [ctx,scrollHandler]()
        {
            ctx->fillExpectedAfterLoad();
            BOOST_TEST_CONTEXT("After load with delay") {ctx->doChecks();}

            ++ctx->step;
            int delta=OrientationInvariant::oprop(ctx->isHorizontal(),ctx->itemSize(),OProp::size)+10;
            scrollHandler(ctx,signedDelta(ctx,delta));

            QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
            [ctx,scrollHandler,delta]()
            {
                ++ctx->step;
                ctx->fillExpectedIds(ctx->frontID(),ctx->backID(),idOffset(ctx,1));
                ctx->scrollAtEdge=false;
                BOOST_TEST_CONTEXT("After scroll") {ctx->doChecks();}

                scrollHandler(ctx,signedDelta(ctx,-delta));

                QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
                [ctx]()
                {
                    ++ctx->step;

                    ctx->fillExpectedAfterLoad();
                    BOOST_TEST_CONTEXT("After scroll back") {ctx->doChecks();}

                    if (!BlockCheckVisibleItemsChanged)
                    {
                        UISE_TEST_CHECK_EQUAL(ctx->visibleItemsChangedCount,2);
                    }
                    ctx->endTestCase();
               });
            });
        });
    };
//    FwlvTestContext::execSingleMode(handler,Qt::Vertical,Direction::END,true);
    FwlvTestContext::execAllModes(handler);
}

}

BOOST_AUTO_TEST_CASE(TestScrollShortDirect)
{
    auto scrollHandler=[](FwlvTestContext* ctx,int delta)
    {
        ctx->view->scroll(delta);
    };
    checkScrollShort(scrollHandler);
}

BOOST_AUTO_TEST_CASE(TestScrollShortKey)
{
    BlockCheckVisibleItemsChanged=true;
    auto scrollHandler=[](FwlvTestContext* ctx,int delta)
    {
        Qt::Key key;
        if (ctx->orientation==Qt::Vertical)
        {
            key=delta<0?Qt::Key_Up:Qt::Key_Down;
        }
        else
        {
            key=delta<0?Qt::Key_Left:Qt::Key_Right;
        }
        for (size_t i=0;i<qAbs(delta);i++)
        {
            QTest::keyPress(ctx->view,key,Qt::NoModifier,1);
        }
        QTest::keyRelease(ctx->view,key,Qt::NoModifier,1);
    };
    checkScrollShort(scrollHandler);
    BlockCheckVisibleItemsChanged=false;
}

BOOST_AUTO_TEST_CASE(TestScrollShortScrollBar)
{
    auto scrollHandler=[](FwlvTestContext* ctx,int delta)
    {
        QScrollBar* bar=ctx->isHorizontal()?ctx->view->horizontalScrollBar():ctx->view->verticalScrollBar();
        int value=bar->value()+delta;
        bar->setValue(value);
    };
    checkScrollShort(scrollHandler);
}

BOOST_AUTO_TEST_SUITE_END()
