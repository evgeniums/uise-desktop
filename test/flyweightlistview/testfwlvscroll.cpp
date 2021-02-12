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

size_t FrontIDBeforeScroll=0;
size_t BackIDBeforeScroll=0;
size_t CountBeforeScroll=0;

int signedDelta(FwlvTestContext* ctx, int delta) noexcept
{
    return ctx->stickMode==Direction::END?-delta:delta;
}

int idOffset(FwlvTestContext* ctx, int offset) noexcept
{
    return ctx->stickMode==Direction::END?offset:-offset;
}

void visibleItemsChangedShort(FwlvTestContext* ctx, const HelloWorldItemWrapper* begin,const HelloWorldItemWrapper* end)
{
    if (BlockCheckVisibleItemsChanged)
    {
        return;
    }
    ++ctx->visibleItemsChangedCount;

    if (ctx->step==1 || ctx->step==3)
    {
        ctx->fillExpectedAfterLoad(ctx->step==3);
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

void visibleItemsChangedLong(FwlvTestContext* ctx, const HelloWorldItemWrapper* begin,const HelloWorldItemWrapper* end)
{
    if (BlockCheckVisibleItemsChanged)
    {
        return;
    }
    ++ctx->visibleItemsChangedCount;

    std::string msg=std::string("visibleItemsChanged, step ")+std::to_string(ctx->step);

    if (ctx->step==2)
    {
        ctx->scrollAtEdge=false;
        ctx->fillExpectedIds(ctx->frontID(),ctx->backID(),idOffset(ctx,CountBeforeScroll/2));

        BOOST_TEST_CONTEXT(msg.c_str())
        {
            ctx->doChecks();

            UISE_TEST_REQUIRE(begin!=nullptr);
            UISE_TEST_REQUIRE(end!=nullptr);

            UISE_TEST_CHECK_EQUAL(begin->id(),ctx->expectedFirstVisibleItemId);
            UISE_TEST_CHECK_EQUAL(end->id(),ctx->expectedLastVisibleItemId);
        }
    }
}

void itemRangeChangedLong(FwlvTestContext* ctx, const HelloWorldItemWrapper* begin,const HelloWorldItemWrapper* end)
{
    ++ctx->itemRangeChangedCount;
    std::string msg=std::string("itemRangeChangedLong, step ")+std::to_string(ctx->step);

    if (ctx->step==2)
    {
        ctx->scrollAtEdge=false;
        if (!ctx->flyweightMode)
        {
            ctx->fillExpectedIds(ctx->frontID(),ctx->backID(),idOffset(ctx,CountBeforeScroll/2));
        }
        else
        {
            ctx->expectedItemCount=CountBeforeScroll+ctx->view->prefetchItemCount();
            if (ctx->stickMode==Direction::END)
            {
                ctx->fillExpectedIds(FrontIDBeforeScroll+ctx->view->prefetchItemCount(),BackIDBeforeScroll,idOffset(ctx,CountBeforeScroll/2));
            }
            else
            {
                ctx->fillExpectedIds(FrontIDBeforeScroll,BackIDBeforeScroll-ctx->view->prefetchItemCount(),idOffset(ctx,CountBeforeScroll/2));
            }
        }

        BOOST_TEST_CONTEXT(msg.c_str())
        {ctx->doChecks();}
    }
    else if (ctx->step==3)
    {
        ctx->scrollAtEdge=true;
        if (!ctx->flyweightMode)
        {
            ctx->fillExpectedAfterLoad();
        }
        else
        {
            ctx->expectedItemCount=ctx->view->visibleItemCount()+ctx->view->maxHiddenItemCount();
            if (ctx->stickMode==Direction::END)
            {
                ctx->fillExpectedIds(BackIDBeforeScroll+ctx->expectedItemCount,BackIDBeforeScroll);
            }
            else
            {
                ctx->fillExpectedIds(FrontIDBeforeScroll,FrontIDBeforeScroll-ctx->expectedItemCount);
            }
        }

        BOOST_TEST_CONTEXT(msg.c_str())
        {ctx->doChecks();}
    }
}

void init(FwlvTestContext* ctx)
{
    ++ctx->step;
    ctx->testWidget->loadItems();    
}

void checkScrollShort(std::function<void (FwlvTestContext* ctx, int delta)> scrollHandler)
{
    auto handler=[scrollHandler](FwlvTestContext* ctx)
    {
        init(ctx);

        ctx->view->setViewportChangedCb(
            [ctx](const HelloWorldItemWrapper* begin,const HelloWorldItemWrapper* end)
            {
                visibleItemsChangedShort(ctx,begin,end);
            }
        );

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
                        UISE_TEST_CHECK_EQUAL(ctx->visibleItemsChangedCount,3);
                    }
                    ctx->endTestCase();
               });
            });
        });
    };
    FwlvTestContext::execAllModes(handler);
//    FwlvTestContext::execSingleMode(handler,Qt::Vertical,Direction::END,true);
}

void checkScrollLong(std::function<void (FwlvTestContext* ctx, int delta)> scrollHandler)
{
    auto handler=[scrollHandler](FwlvTestContext* ctx)
    {
        init(ctx);

        ctx->view->setViewportChangedCb(
            [ctx](const HelloWorldItemWrapper* begin,const HelloWorldItemWrapper* end)
            {
                visibleItemsChangedLong(ctx,begin,end);
            }
        );

        ctx->view->setItemRangeChangedCb(
            [ctx](const HelloWorldItemWrapper* begin,const HelloWorldItemWrapper* end)
            {
                itemRangeChangedLong(ctx,begin,end);
            }
        );

        QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
        [ctx,scrollHandler]()
        {
            ctx->fillExpectedAfterLoad();
            BOOST_TEST_CONTEXT("After load with delay") {ctx->doChecks();}

            FrontIDBeforeScroll=ctx->frontID();
            BackIDBeforeScroll=ctx->backID();
            CountBeforeScroll=ctx->view->itemCount();

            ++ctx->step;
            int delta=OrientationInvariant::oprop(ctx->isHorizontal(),ctx->itemSize(),OProp::size)*ctx->view->itemCount()/2+10;
            scrollHandler(ctx,signedDelta(ctx,delta));

            QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
            [ctx,scrollHandler,delta]()
            {
                ++ctx->step;

                ctx->scrollAtEdge=false;
                if (!ctx->flyweightMode)
                {
                    ctx->fillExpectedIds(ctx->frontID(),ctx->backID(),idOffset(ctx,CountBeforeScroll/2));
                }
                else
                {
                    ctx->expectedItemCount=CountBeforeScroll+ctx->view->prefetchItemCount();
                    if (ctx->stickMode==Direction::END)
                    {
                        ctx->fillExpectedIds(FrontIDBeforeScroll+ctx->view->prefetchItemCount(),BackIDBeforeScroll,idOffset(ctx,CountBeforeScroll/2));
                    }
                    else
                    {
                        ctx->fillExpectedIds(FrontIDBeforeScroll,BackIDBeforeScroll-ctx->view->prefetchItemCount(),idOffset(ctx,CountBeforeScroll/2));
                    }
                }
                BOOST_TEST_CONTEXT("After scroll") {ctx->doChecks();}

                scrollHandler(ctx,signedDelta(ctx,-delta));

                QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
                [ctx]()
                {
                    ctx->scrollAtEdge=true;
                    ++ctx->step;

                    if (!ctx->flyweightMode)
                    {
                        ctx->fillExpectedAfterLoad();
                    }
                    else
                    {
                        ctx->expectedItemCount=ctx->view->visibleItemCount()+ctx->view->maxHiddenItemCount();
                        size_t frontID=0;
                        size_t backID=0;
                        if (ctx->stickMode==Direction::END)
                        {
                            frontID=BackIDBeforeScroll+ctx->expectedItemCount;
                            backID=BackIDBeforeScroll;
                        }
                        else
                        {
                            frontID=FrontIDBeforeScroll;
                            backID=FrontIDBeforeScroll-ctx->expectedItemCount;
                        }

                        ctx->fillExpectedIds(frontID,backID);
                    }

                    BOOST_TEST_CONTEXT("After scroll back") {ctx->doChecks();}

                    if (!BlockCheckVisibleItemsChanged)
                    {
                        UISE_TEST_CHECK_EQUAL(ctx->visibleItemsChangedCount,3);
                    }
                    if (ctx->flyweightMode)
                    {
                        UISE_TEST_CHECK_EQUAL(ctx->itemRangeChangedCount,3);
                    }
                    else
                    {
                        UISE_TEST_CHECK_EQUAL(ctx->itemRangeChangedCount,0);
                    }

                    QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
                    [ctx]()
                    {
                        ctx->endTestCase();
                    });
               });
            });
        });
    };
    FwlvTestContext::execAllModes(handler);
//    FwlvTestContext::execSingleMode(handler,Qt::Vertical,Direction::END,true);
}

void checkScrollPage()
{
    auto handler=[](FwlvTestContext* ctx)
    {
        auto scrollHandler=[ctx](int pages)
        {
            Qt::Key key=pages<0?Qt::Key_PageUp:Qt::Key_PageDown;
            for (size_t i=0;i<qAbs(pages);i++)
            {
                QTest::keyPress(ctx->view,key,Qt::NoModifier,1);
            }
            QTest::keyRelease(ctx->view,key,Qt::NoModifier,1);
        };

        init(ctx);

        ctx->view->setViewportChangedCb(
            [ctx](const HelloWorldItemWrapper* begin,const HelloWorldItemWrapper* end)
            {
            }
        );

        QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
        [ctx,scrollHandler]()
        {
            ctx->fillExpectedAfterLoad();
            BOOST_TEST_CONTEXT("After load with delay") {ctx->doChecks();}

            ++ctx->step;
            scrollHandler(signedDelta(ctx,1));

            QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
            [ctx,scrollHandler]()
            {
                ++ctx->step;

                auto delta=ctx->view->pageScrollStep();
                auto itemSize=OrientationInvariant::oprop(ctx->isHorizontal(),ctx->itemSize(),OProp::size);
                auto idDelta=floor(static_cast<float>(delta)/itemSize);

                auto frontID=ctx->frontID();
                auto backID=ctx->backID();

                auto viewSize=OrientationInvariant::oprop(ctx->isHorizontal(),ctx->view,OProp::size);
                auto visibleCount=ceil(static_cast<float>(viewSize)/static_cast<float>(itemSize));

                if (ctx->isHorizontal())
                {
                    // when the first and the last items partially fit into viewport then there are +1 item
                    // this is very likely to happen for horizontal orientation
                    // it might happen for vertical orientation also but very unlikely, so we ignore that case for vertical orientation
                    if (viewSize%itemSize!=0)
                    {
                        ++visibleCount;
                    }
                }
                ctx->expectedVisibleItemCount=visibleCount;
                ctx->fillExpectedIds(frontID,backID,idOffset(ctx,idDelta));
                ctx->scrollAtEdge=false;
                BOOST_TEST_CONTEXT("After scroll") {ctx->doChecks();}

                scrollHandler(signedDelta(ctx,-1));

                QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
                [ctx]()
                {
                    ++ctx->step;

                    ctx->fillExpectedAfterLoad();
                    BOOST_TEST_CONTEXT("After scroll back") {ctx->doChecks();}

                    ctx->endTestCase();
               });
            });
        });
    };

    FwlvTestContext::execAllModes(handler);
}

void directScrollHandler(FwlvTestContext* ctx,int delta)
{
    ctx->view->scroll(delta);
}

void keyScrollHandler(FwlvTestContext* ctx,int delta)
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
}

void scrollBarScrollHandler(FwlvTestContext* ctx,int delta)
{
    QScrollBar* bar=ctx->isHorizontal()?ctx->view->horizontalScrollBar():ctx->view->verticalScrollBar();
    int value=bar->value()+delta;

    std::string msg=std::string("scrollBarScrollHandler delta=")+std::to_string(delta) + std::string(" size=")
            +std::to_string(OrientationInvariant::oprop(ctx->isHorizontal(),ctx->view,OProp::size))
            +std::string(" old value=")+std::to_string(bar->value())
            +std::string(" new value=")+std::to_string(value);
    UISE_TEST_MESSAGE(msg.c_str());

    bar->setValue(value);
}

}

BOOST_AUTO_TEST_CASE(TestScrollShortDirect)
{
    checkScrollShort(directScrollHandler);
}

BOOST_AUTO_TEST_CASE(TestScrollShortKey)
{
    BlockCheckVisibleItemsChanged=true;
    checkScrollShort(keyScrollHandler);
    BlockCheckVisibleItemsChanged=false;
}

BOOST_AUTO_TEST_CASE(TestScrollShortScrollBar)
{
    checkScrollShort(scrollBarScrollHandler);
}

BOOST_AUTO_TEST_CASE(TestScrollLongDirect)
{
    checkScrollLong(directScrollHandler);
}

BOOST_AUTO_TEST_CASE(TestScrollLongScrollBar)
{
    checkScrollLong(scrollBarScrollHandler);
}

BOOST_AUTO_TEST_CASE(TestScrollPage)
{
    checkScrollPage();
}

BOOST_AUTO_TEST_SUITE_END()
