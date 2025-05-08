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

constexpr const int ID_DELTA=5;
int idOffset(FwlvTestContext* ctx, int offset) noexcept
{
    return ctx->stickMode==Direction::END?offset:-offset;
}

void checkJumpItem(FwlvTestContext* ctx)
{
    ctx->testWidget->initialItemCount=ctx->testWidget->initialItemCount+ID_DELTA;
    ctx->testWidget->loadItems();
    ++ctx->step;

    QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
    [ctx]()
    {
        QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
        [ctx]()
        {
            ctx->fillExpectedAfterLoad();
            BOOST_TEST_CONTEXT("After load with delay") {ctx->doChecks();}

            auto itemSize=OrientationInvariant::oprop(ctx->isHorizontal(),ctx->itemSize(),OProp::size);
            auto backDeltaBefore=ctx->backID()-ctx->view->lastViewportItem()->id();

            ++ctx->step;
            ctx->testWidget->pimpl->jumpMode->setCurrentIndex(ctx->testWidget->pimpl->jumpMode->findData(static_cast<int>(Direction::NONE)));
            ctx->testWidget->pimpl->jumpItem->setValue(ctx->view->firstViewportItem()->id()+idOffset(ctx,ID_DELTA));
            ctx->testWidget->pimpl->jumpOffset->setValue(idOffset(ctx,10));

            QTimer::singleShot(1,ctx->mainWindow,
            [ctx,itemSize,backDeltaBefore]()
            {
                ctx->testWidget->pimpl->jumpToButton->click();

                QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
                [ctx,itemSize,backDeltaBefore]()
                {
                    auto viewSize=OrientationInvariant::oprop(ctx->isHorizontal(),ctx->view,OProp::size);
                    auto visibleCount=ceil(static_cast<float>(viewSize)/static_cast<float>(itemSize));
                    ctx->expectedVisibleItemCount=visibleCount;
                    if (ctx->stickMode==Direction::END)
                    {
                        ctx->fillExpectedIds(ctx->frontID(),ctx->backID(),idOffset(ctx,ID_DELTA)+backDeltaBefore);
                    }
                    else
                    {
                        ctx->fillExpectedIds(ctx->frontID(),ctx->backID(),idOffset(ctx,ID_DELTA)+1);
                    }
                    ctx->scrollAtEdge=false;

                    BOOST_TEST_CONTEXT("After jump") {ctx->doChecks(true,false,true);}

                    ctx->endTestCase();
                });
            });
        });
    });
}

}

BOOST_AUTO_TEST_CASE(TestJumpToEdge)
{
    auto handler=[](FwlvTestContext* ctx)
    {
        checkJumpEdge(ctx);
    };
    FwlvTestContext::execAllModes(handler);
}

BOOST_AUTO_TEST_CASE(TestJumpToItem)
{
    auto handler=[](FwlvTestContext* ctx)
    {
        checkJumpItem(ctx);
    };
//    FwlvTestContext::execSingleMode(handler,Qt::Vertical,Direction::END,true);
    FwlvTestContext::execAllModes(handler);
}

BOOST_AUTO_TEST_SUITE_END()
