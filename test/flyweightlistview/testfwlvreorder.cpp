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

/** @file uise/test/flyweightlistview/testfwlvreorder.cpp
*
*  Test reordering of an already inserted item in FlyweightListView after its sort value was
*  changed in place (the chat list does this when the last message of a chat is deleted).
*
*  A row that newly becomes the first/last one of the loaded window must be dropped only when the
*  window does not reach the edge of the data (there may be unfetched rows in between, so the
*  prefetch brings the row back in place). When the owner has pinned the marker at the loaded edge
*  nothing would ever fetch it back, so the row must be kept.
*
*/

/****************************************************************************/

#include <functional>

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

enum class Pick : int
{
    Middle,
    Last
};

enum class Expect : int
{
    Dropped,
    KeptInside,
    KeptFirst,
    KeptLast
};

struct ReorderSpec
{
    // pin the marker at the edge of the loaded window, as ChatList::insertFetched() does once a
    // fetch comes back short/empty ("the loaded edge IS the edge of the data")
    bool pinMax=false;
    bool pinMin=false;

    bool adjustMinMax=false;

    Pick pick=Pick::Middle;

    // new sort value of the picked item, given the sort values of the loaded window's edges
    std::function<size_t (size_t firstSeq, size_t lastSeq)> newSeq;

    Expect expect=Expect::Dropped;
};

void checkReorder(FwlvTestContext* ctx, ReorderSpec spec)
{
    ctx->testWidget->loadItems();
    ++ctx->step;

    QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
    [ctx,spec]()
    {
        auto* view=ctx->view;

        const auto* first=view->firstItem();
        const auto* last=view->lastItem();
        UISE_TEST_REQUIRE(first);
        UISE_TEST_REQUIRE(last);

        const auto firstSeq=first->sortValue();
        const auto lastSeq=last->sortValue();
        const auto countBefore=view->itemCount();
        UISE_TEST_REQUIRE(countBefore>=5);

        if (spec.pinMax)
        {
            view->setMaxSortValue(lastSeq);
        }
        if (spec.pinMin)
        {
            view->setMinSortValue(firstSeq);
        }

        // the loaded window is contiguous, so the sort value maps to the item id via the test data
        const auto pickedSeq=spec.pick==Pick::Last?lastSeq:firstSeq+(lastSeq-firstSeq)/2;
        const auto id=ctx->testWidget->pimpl->items[pickedSeq];
        const auto* item=view->item(id);
        UISE_TEST_REQUIRE(item);

        const auto newSeq=spec.newSeq(firstSeq,lastSeq);

        // change the live sort value in place and reorder, the same way ChatList does
        view->beginUpdate();
        item->item()->setSeqNum(newSeq);
        view->reorderItem(*item,spec.adjustMinMax);
        view->endUpdate();

        // 'item' is not valid after this point: a dropped row is destroyed
        if (spec.expect==Expect::Dropped)
        {
            UISE_TEST_CHECK_EQUAL(view->itemCount(),countBefore-1);
            UISE_TEST_CHECK(view->item(id)==nullptr);
        }
        else
        {
            UISE_TEST_CHECK_EQUAL(view->itemCount(),countBefore);
            const auto* kept=view->item(id);
            UISE_TEST_REQUIRE(kept);
            UISE_TEST_CHECK(kept->widget()!=nullptr);
            UISE_TEST_CHECK_EQUAL(kept->sortValue(),newSeq);

            if (spec.expect==Expect::KeptFirst)
            {
                UISE_TEST_REQUIRE(view->firstItem());
                UISE_TEST_CHECK_EQUAL(view->firstItem()->id(),id);
                // the pinned marker follows the row, it is still the edge of the data
                UISE_TEST_CHECK_EQUAL(view->minSortValue(),newSeq);
            }
            else if (spec.expect==Expect::KeptLast)
            {
                UISE_TEST_REQUIRE(view->lastItem());
                UISE_TEST_CHECK_EQUAL(view->lastItem()->id(),id);
                UISE_TEST_CHECK_EQUAL(view->maxSortValue(),newSeq);
            }
        }

        ++ctx->step;

        // let the deferred prefetch/eviction run: a kept row must survive it
        QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
        [ctx,spec,id,countBefore]()
        {
            if (spec.expect!=Expect::Dropped)
            {
                UISE_TEST_CHECK_EQUAL(ctx->view->itemCount(),countBefore);
                UISE_TEST_CHECK(ctx->view->item(id)!=nullptr);
            }

            ctx->endTestCase();
        });
    });
}

void execReorder(ReorderSpec spec, Direction stickMode)
{
    auto handler=[spec](FwlvTestContext* ctx)
    {
        checkReorder(ctx,spec);
    };
    FwlvTestContext::execSingleMode(handler,Qt::Vertical,stickMode,true);
}

}

// The bug: the loaded window reaches the last row of the data (marker pinned there) and a row moves
// past it, e.g. a chat whose last message was deleted falls to the bottom of a fully loaded list.
// The view sticks to HOME, so it used to drop the row and nothing ever fetched it back.
BOOST_AUTO_TEST_CASE(TestReorderToEndPinnedEdgeKeepsItem)
{
    ReorderSpec spec;
    spec.pinMax=true;
    spec.newSeq=[](size_t, size_t lastSeq){return lastSeq+3;};
    spec.expect=Expect::KeptLast;
    execReorder(spec,Direction::HOME);
}

// Same, with adjustMinMax enabled: the marker must not be extended twice or otherwise disturbed.
BOOST_AUTO_TEST_CASE(TestReorderToEndPinnedEdgeAdjustMinMaxKeepsItem)
{
    ReorderSpec spec;
    spec.pinMax=true;
    spec.adjustMinMax=true;
    spec.newSeq=[](size_t, size_t lastSeq){return lastSeq+3;};
    spec.expect=Expect::KeptLast;
    execReorder(spec,Direction::HOME);
}

// Symmetric case at the beginning of the data: the view sticks to END, so it is not at the begin.
BOOST_AUTO_TEST_CASE(TestReorderToBeginPinnedEdgeKeepsItem)
{
    ReorderSpec spec;
    spec.pinMin=true;
    spec.newSeq=[](size_t firstSeq, size_t){return firstSeq-3;};
    spec.expect=Expect::KeptFirst;
    execReorder(spec,Direction::END);
}

// Open edge: the window does not reach the end of the data (the max marker is still far beyond it),
// so there may be unfetched rows between the window and the new position and the row must still
// be dropped, to be fetched back in place by the prefetch.
BOOST_AUTO_TEST_CASE(TestReorderToEndOpenEdgeDropsItem)
{
    ReorderSpec spec;
    spec.newSeq=[](size_t, size_t lastSeq){return lastSeq+3;};
    spec.expect=Expect::Dropped;
    execReorder(spec,Direction::HOME);
}

BOOST_AUTO_TEST_CASE(TestReorderToBeginOpenEdgeDropsItem)
{
    ReorderSpec spec;
    spec.newSeq=[](size_t firstSeq, size_t){return firstSeq-3;};
    spec.expect=Expect::Dropped;
    execReorder(spec,Direction::END);
}

// Open edge with adjustMinMax: the row jumps past the whole known data range, which extends the max
// marker onto the row itself. That must not make the window look like it reaches the end of the
// data -- the marker is judged as the owner left it, before the extension -- so it is still dropped.
BOOST_AUTO_TEST_CASE(TestReorderPastMarkerAdjustMinMaxOpenEdgeDropsItem)
{
    ReorderSpec spec;
    spec.adjustMinMax=true;
    spec.newSeq=[](size_t, size_t){return FwlvTestWidget::count+5;};
    spec.expect=Expect::Dropped;
    execReorder(spec,Direction::HOME);
}

// A row moving around inside the loaded window is never dropped, edges pinned or not.
BOOST_AUTO_TEST_CASE(TestReorderInsideWindowKeepsItem)
{
    ReorderSpec spec;
    spec.pinMax=true;
    spec.pinMin=true;
    spec.newSeq=[](size_t firstSeq, size_t){return firstSeq+1;};
    spec.expect=Expect::KeptInside;
    execReorder(spec,Direction::HOME);
}

// The last row stays the last one (moves further towards the end): it was already at the edge, so
// it is not "newly" the extreme and must never be dropped, even with an open edge.
BOOST_AUTO_TEST_CASE(TestReorderLastItemStaysLast)
{
    ReorderSpec spec;
    spec.pick=Pick::Last;
    spec.newSeq=[](size_t, size_t lastSeq){return lastSeq+2;};
    spec.expect=Expect::KeptInside;
    execReorder(spec,Direction::HOME);
}

BOOST_AUTO_TEST_SUITE_END()
