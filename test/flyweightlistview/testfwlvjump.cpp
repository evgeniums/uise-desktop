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
        ctx->expectedItemCount=ctx->view->prefetchItemWindow();
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
                ctx->expectedItemCount=ctx->view->prefetchItemWindow();
                ctx->fillExpectedIds(ctx->frontID(),ctx->backID(),0,true);
                BOOST_TEST_CONTEXT("After jump") {ctx->doChecks(true,true);}

                ++ctx->step;
                jumpTo(ctx,ctx->stickMode,true);

                QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
                [ctx]()
                {
                    ctx->fillExpectedAfterLoad();
                    ctx->expectedItemCount=ctx->view->prefetchItemWindow();
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

namespace {

// Geometry-based checks, independent of id/order assumptions: map the item's own widget into
// viewportFrame() coordinates and read its extent along the view's own orientation.
int viewportExtent(FwlvTestContext* ctx)
{
    return OrientationInvariant::oprop(ctx->isHorizontal(),ctx->view->viewportSize(),OProp::size);
}

bool itemExtent(FwlvTestContext* ctx, size_t id, int& begin, int& size)
{
    const auto* it=ctx->view->item(id);
    if (it==nullptr)
    {
        return false;
    }
    auto* widget=it->widget();
    if (widget==nullptr)
    {
        return false;
    }
    auto topLeft=widget->mapTo(ctx->view->viewportFrame(),QPoint(0,0));
    begin=OrientationInvariant::oprop(ctx->isHorizontal(),topLeft,OProp::pos);
    size=OrientationInvariant::oprop(ctx->isHorizontal(),widget,OProp::size);
    return true;
}

bool isFullyVisible(FwlvTestContext* ctx, size_t id)
{
    int begin=0;
    int size=0;
    if (!itemExtent(ctx,id,begin,size))
    {
        return false;
    }
    return begin>=0 && (begin+size)<=viewportExtent(ctx);
}

bool isFullyHidden(FwlvTestContext* ctx, size_t id)
{
    int begin=0;
    int size=0;
    if (!itemExtent(ctx,id,begin,size))
    {
        return false;
    }
    return (begin+size)<=0 || begin>=viewportExtent(ctx);
}

int scrollPos(FwlvTestContext* ctx)
{
    return ctx->isHorizontal()?ctx->view->horizontalScrollBar()->value()
                               :ctx->view->verticalScrollBar()->value();
}

void checkEnsureItemVisible(FwlvTestContext* ctx)
{
    // Comfortably more items than any viewport/orientation combo in execAllModes() can fit,
    // including the non-flyweight modes (no prefetch beyond this to pad the window further) --
    // same margin checkJumpItem() above gives itself for the same reason.
    ctx->testWidget->initialItemCount=ctx->testWidget->initialItemCount+10;
    ctx->testWidget->loadItems();
    ++ctx->step;

    QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
    [ctx]()
    {
        ctx->fillExpectedAfterLoad();
        BOOST_TEST_CONTEXT("After load with delay") {ctx->doChecks();}

        // Case 1: an already fully visible item is left exactly where it is.
        // firstViewportItem()/lastViewportItem() are hit-tested at the viewport's own edges and
        // can land on a partially clipped item (see their own doc), so fall back to the other
        // end if the first pick isn't actually fully on screen.
        const auto* visibleItem=ctx->view->firstViewportItem();
        UISE_TEST_REQUIRE(visibleItem!=nullptr);
        auto visibleId=visibleItem->id();
        if (!isFullyVisible(ctx,visibleId))
        {
            const auto* alt=ctx->view->lastViewportItem();
            UISE_TEST_REQUIRE(alt!=nullptr);
            visibleId=alt->id();
        }
        UISE_TEST_REQUIRE(isFullyVisible(ctx,visibleId));

        auto posBefore=scrollPos(ctx);
        UISE_TEST_CHECK(ctx->view->ensureItemVisible(visibleId,false));
        UISE_TEST_CHECK_EQUAL(scrollPos(ctx),posBefore);

        // Case 2: an item entirely off-screen, without centering, ends up flush with the nearer
        // edge -- fully visible again, with the minimal move that makes it so.
        const auto* hiddenCandidate=ctx->view->firstItem();
        UISE_TEST_REQUIRE(hiddenCandidate!=nullptr);
        auto hiddenId=hiddenCandidate->id();
        if (!isFullyHidden(ctx,hiddenId))
        {
            const auto* alt=ctx->view->lastItem();
            UISE_TEST_REQUIRE(alt!=nullptr);
            hiddenId=alt->id();
        }
        UISE_TEST_REQUIRE(isFullyHidden(ctx,hiddenId));

        UISE_TEST_CHECK(ctx->view->ensureItemVisible(hiddenId,false));
        UISE_TEST_CHECK(isFullyVisible(ctx,hiddenId));

        // Case 3: with centering, a still-hidden item (the loaded window has plenty of rows left
        // on both sides after the two moves above) lands centered rather than flush with an edge.
        const auto* hiddenCandidate2=ctx->view->lastItem();
        UISE_TEST_REQUIRE(hiddenCandidate2!=nullptr);
        auto hiddenId2=hiddenCandidate2->id();
        if (hiddenId2==hiddenId || !isFullyHidden(ctx,hiddenId2))
        {
            const auto* alt=ctx->view->firstItem();
            UISE_TEST_REQUIRE(alt!=nullptr);
            hiddenId2=alt->id();
        }
        UISE_TEST_REQUIRE(hiddenId2!=hiddenId);
        UISE_TEST_REQUIRE(isFullyHidden(ctx,hiddenId2));

        UISE_TEST_CHECK(ctx->view->ensureItemVisible(hiddenId2,true));
        UISE_TEST_REQUIRE(isFullyVisible(ctx,hiddenId2));
        int begin=0;
        int size=0;
        UISE_TEST_REQUIRE(itemExtent(ctx,hiddenId2,begin,size));
        auto expectedBegin=(viewportExtent(ctx)-size)/2;
        auto diff=begin-expectedBegin;
        if (diff<0)
        {
            diff=-diff;
        }
        UISE_TEST_CHECK(diff<=1);

        // Case 4: an id that was never loaded is reported as not found, and nothing moves.
        auto posBeforeUnknown=scrollPos(ctx);
        UISE_TEST_CHECK(!ctx->view->ensureItemVisible(HelloWorldItem::MaxHelloWorldItemId+1,true));
        UISE_TEST_CHECK_EQUAL(scrollPos(ctx),posBeforeUnknown);

        ctx->endTestCase();
    });
}

}

BOOST_AUTO_TEST_CASE(TestEnsureItemVisible)
{
    auto handler=[](FwlvTestContext* ctx)
    {
        checkEnsureItemVisible(ctx);
    };
    FwlvTestContext::execAllModes(handler);
}

BOOST_AUTO_TEST_SUITE_END()
