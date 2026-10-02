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

/** @file uise/test/flyweightlistview/testfwlvsmoothfollow.cpp
*
*  Test smooth following of appended items in FlyweightListView.
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

// well past the nominal duration of the animation
constexpr int SettleMs=1200;

void later(FwlvTestContext* ctx, int ms, std::function<void ()> handler)
{
    QTimer::singleShot(ms,ctx->mainWindow,std::move(handler));
}

// appends one item after the last one, the way a live message is inserted
void append(FwlvTestContext* ctx)
{
    auto seq=ctx->view->lastItem()->sortValue()+1;
    ctx->view->beginUpdate();
    ctx->view->insertItem(HelloWorldItemWrapper(new HelloWorldItem(seq,--HelloWorldItem::HelloWorldItemId)));
    ctx->view->endUpdate();
}

void checkAtEnd(FwlvTestContext* ctx)
{
    UISE_TEST_CHECK(!ctx->view->isSmoothFollowActive());
    UISE_TEST_CHECK(ctx->view->isScrollAtEdge(Direction::END));
}

// loads the items, then runs the handler once the view has settled at the end
void prepare(FwlvTestContext* ctx, bool enable, std::function<void ()> handler)
{
    ctx->view->setSmoothFollowEnabled(enable);
    ctx->testWidget->loadItems();
    later(ctx,FwlvTestContext::PlayStepPeriod,
        [ctx,handler]()
        {
            UISE_TEST_REQUIRE(ctx->view->isScrollAtEdge(Direction::END));
            UISE_TEST_CHECK(!ctx->view->isSmoothFollowActive());
            handler();
        }
    );
}

void execEnd(std::function<void (FwlvTestContext* ctx)> handler)
{
    UISE_TEST_CONTEXT("VerticalStickEndFlyweight") {FwlvTestContext::execSingleMode(handler,Qt::Vertical,Direction::END,true);}
    UISE_TEST_CONTEXT("VerticalStickEndNoFlyweight") {FwlvTestContext::execSingleMode(handler,Qt::Vertical,Direction::END,false);}
}

}

BOOST_AUTO_TEST_CASE(TestSmoothFollowDisabled)
{
    auto handler=[](FwlvTestContext* ctx)
    {
        prepare(ctx,false,
            [ctx]()
            {
                UISE_TEST_CHECK(!ctx->view->isSmoothFollowEnabled());

                auto countBefore=ctx->view->itemCount();
                append(ctx);
                UISE_TEST_CHECK_EQUAL(ctx->view->itemCount(),countBefore+1);

                // snaps at once, nothing is animated
                checkAtEnd(ctx);

                ctx->endTestCase();
            }
        );
    };
    execEnd(handler);
}

BOOST_AUTO_TEST_CASE(TestSmoothFollowAppend)
{
    auto handler=[](FwlvTestContext* ctx)
    {
        prepare(ctx,true,
            [ctx]()
            {
                UISE_TEST_CHECK(ctx->view->isSmoothFollowEnabled());

                auto countBefore=ctx->view->itemCount();
                append(ctx);
                UISE_TEST_CHECK_EQUAL(ctx->view->itemCount(),countBefore+1);

                // the new item is below the viewport and on its way in
                UISE_TEST_CHECK(ctx->view->isSmoothFollowActive());
                UISE_TEST_CHECK(ctx->view->isFollowingStickEdge());
                UISE_TEST_CHECK(!ctx->view->isScrollAtEdge(Direction::END));

                later(ctx,SettleMs,
                    [ctx]()
                    {
                        checkAtEnd(ctx);
                        ctx->endTestCase();
                    }
                );
            }
        );
    };
    execEnd(handler);
}

BOOST_AUTO_TEST_CASE(TestSmoothFollowBurst)
{
    auto handler=[](FwlvTestContext* ctx)
    {
        prepare(ctx,true,
            [ctx]()
            {
                for (int i=0;i<4;i++)
                {
                    append(ctx);
                }
                UISE_TEST_CHECK(ctx->view->isSmoothFollowActive());
                UISE_TEST_CHECK(!ctx->view->isScrollAtEdge(Direction::END));

                // more arrive while it is still scrolling: it carries on instead of restarting or snapping
                later(ctx,60,
                    [ctx]()
                    {
                        UISE_TEST_CHECK(ctx->view->isSmoothFollowActive());
                        append(ctx);
                        append(ctx);
                        UISE_TEST_CHECK(ctx->view->isSmoothFollowActive());
                        UISE_TEST_CHECK(!ctx->view->isScrollAtEdge(Direction::END));

                        later(ctx,SettleMs,
                            [ctx]()
                            {
                                checkAtEnd(ctx);
                                ctx->endTestCase();
                            }
                        );
                    }
                );
            }
        );
    };
    execEnd(handler);
}

BOOST_AUTO_TEST_CASE(TestSmoothFollowUserScrollCancels)
{
    auto handler=[](FwlvTestContext* ctx)
    {
        prepare(ctx,true,
            [ctx]()
            {
                append(ctx);
                UISE_TEST_REQUIRE(ctx->view->isSmoothFollowActive());

                // scrolling away from the end is explicit positioning: the animation stops where it is
                ctx->view->scroll(-30);
                UISE_TEST_CHECK(!ctx->view->isSmoothFollowActive());
                UISE_TEST_CHECK(!ctx->view->isFollowingStickEdge());

                later(ctx,SettleMs,
                    [ctx]()
                    {
                        // it did not carry on to the end by itself
                        UISE_TEST_CHECK(!ctx->view->isSmoothFollowActive());
                        UISE_TEST_CHECK(!ctx->view->isScrollAtEdge(Direction::END));

                        // and the view is no longer following, so a new item does not pull it down
                        append(ctx);
                        UISE_TEST_CHECK(!ctx->view->isSmoothFollowActive());
                        UISE_TEST_CHECK(!ctx->view->isScrollAtEdge(Direction::END));

                        ctx->endTestCase();
                    }
                );
            }
        );
    };
    execEnd(handler);
}

BOOST_AUTO_TEST_CASE(TestSmoothFollowScrollTowardsEndFinishes)
{
    auto handler=[](FwlvTestContext* ctx)
    {
        prepare(ctx,true,
            [ctx]()
            {
                append(ctx);
                UISE_TEST_REQUIRE(ctx->view->isSmoothFollowActive());

                // scrolling towards the end while on the way there just gets there
                ctx->view->scroll(10);
                checkAtEnd(ctx);

                ctx->endTestCase();
            }
        );
    };
    execEnd(handler);
}

BOOST_AUTO_TEST_CASE(TestSmoothFollowExplicitScrollCancels)
{
    auto handler=[](FwlvTestContext* ctx)
    {
        prepare(ctx,true,
            [ctx]()
            {
                append(ctx);
                UISE_TEST_REQUIRE(ctx->view->isSmoothFollowActive());

                // a jump to an item, like to a replied message
                auto first=ctx->view->firstItem();
                UISE_TEST_REQUIRE(first!=nullptr);
                UISE_TEST_CHECK(ctx->view->scrollToItem(first->id(),0));
                UISE_TEST_CHECK(!ctx->view->isSmoothFollowActive());

                later(ctx,SettleMs,
                    [ctx]()
                    {
                        UISE_TEST_CHECK(!ctx->view->isSmoothFollowActive());
                        UISE_TEST_CHECK(!ctx->view->isScrollAtEdge(Direction::END));
                        ctx->endTestCase();
                    }
                );
            }
        );
    };
    execEnd(handler);
}

BOOST_AUTO_TEST_CASE(TestSmoothFollowJumpToEdgeCarriesOn)
{
    auto handler=[](FwlvTestContext* ctx)
    {
        prepare(ctx,true,
            [ctx]()
            {
                append(ctx);
                UISE_TEST_REQUIRE(ctx->view->isSmoothFollowActive());

                // what a sender's own message does: it is inserted, then the view is jumped to the end
                ctx->view->jumpToEdge(Direction::END);
                UISE_TEST_CHECK(ctx->view->isSmoothFollowActive());
                UISE_TEST_CHECK(!ctx->view->isScrollAtEdge(Direction::END));

                later(ctx,SettleMs,
                    [ctx]()
                    {
                        checkAtEnd(ctx);
                        ctx->endTestCase();
                    }
                );
            }
        );
    };
    execEnd(handler);
}

BOOST_AUTO_TEST_CASE(TestSmoothFollowClearAndDisable)
{
    auto handler=[](FwlvTestContext* ctx)
    {
        prepare(ctx,true,
            [ctx]()
            {
                append(ctx);
                UISE_TEST_REQUIRE(ctx->view->isSmoothFollowActive());

                ctx->view->clear();
                UISE_TEST_CHECK(!ctx->view->isSmoothFollowActive());

                ctx->testWidget->loadItems();
                later(ctx,FwlvTestContext::PlayStepPeriod,
                    [ctx]()
                    {
                        append(ctx);
                        UISE_TEST_REQUIRE(ctx->view->isSmoothFollowActive());

                        // switching it off finishes the animation at once
                        ctx->view->setSmoothFollowEnabled(false);
                        checkAtEnd(ctx);

                        ctx->endTestCase();
                    }
                );
            }
        );
    };
    execEnd(handler);
}

BOOST_AUTO_TEST_SUITE_END()
