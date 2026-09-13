/**
@copyright Evgeny Sidorov 2026

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/test/utils/testflowpack.cpp
*
*  Test of flowPack() -- the wrapping-row packer used by ChatMessageReactionsRow.
*
*/

/****************************************************************************/

#include <boost/test/unit_test.hpp>

#include <uise/test/uise-testthread.hpp>
#include <uise/desktop/utils/flowpack.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

namespace {

//! QRect/QSize have no std::ostream<< (only QDebug's), so UISE_TEST_CHECK_EQUAL cannot be used on
//! them directly -- see testalbumlayout.cpp's own field-by-field comparisons for the same reason.
void checkRect(const QRect& actual, int x, int y, int w, int h)
{
    UISE_TEST_CHECK_EQUAL(actual.x(),x);
    UISE_TEST_CHECK_EQUAL(actual.y(),y);
    UISE_TEST_CHECK_EQUAL(actual.width(),w);
    UISE_TEST_CHECK_EQUAL(actual.height(),h);
}

void checkSize(const QSize& actual, int w, int h)
{
    UISE_TEST_CHECK_EQUAL(actual.width(),w);
    UISE_TEST_CHECK_EQUAL(actual.height(),h);
}

//! No two placed rects overlap -- the same invariant testalbumlayout.cpp checks for albumLayout().
void checkNoOverlap(const std::vector<QRect>& rects)
{
    for (size_t i=0; i<rects.size(); ++i)
    {
        for (size_t j=i+1; j<rects.size(); ++j)
        {
            UISE_TEST_CHECK(!rects[i].intersects(rects[j]));
        }
    }
}

}

BOOST_AUTO_TEST_SUITE(TestFlowPack)

BOOST_AUTO_TEST_CASE(TestEmptyInput)
{
    FlowPackOptions options;
    options.maxWidth=100;

    auto result=flowPack({},options);

    UISE_TEST_CHECK(result.rects.empty());
    UISE_TEST_CHECK_EQUAL(result.placedCount,size_t{0});
    UISE_TEST_CHECK(!result.truncated);
    UISE_TEST_CHECK(result.lastRowRect.isNull());
    checkSize(result.totalSize,0,0);
    UISE_TEST_CHECK(result.tailFits);
}

BOOST_AUTO_TEST_CASE(TestEmptyInputWithReservedTail)
{
    FlowPackOptions fits;
    fits.maxWidth=20;
    fits.reservedTailWidth=10;
    UISE_TEST_CHECK(flowPack({},fits).tailFits);

    FlowPackOptions overflows;
    overflows.maxWidth=20;
    overflows.reservedTailWidth=30;
    UISE_TEST_CHECK(!flowPack({},overflows).tailFits);
}

BOOST_AUTO_TEST_CASE(TestSingleRowWrap)
{
    // 4 items of width 30 at maxWidth=70/hSpacing=6 fit exactly 2 per row (30+6+30=66<=70; a
    // third would need 102). Row heights differ (10 vs 20) to also exercise vertical centring.
    std::vector<QSize> items{{30,10},{30,20},{30,10},{30,10}};

    FlowPackOptions options;
    options.maxWidth=70;
    options.hSpacing=6;
    options.vSpacing=4;

    auto result=flowPack(items,options);

    UISE_TEST_REQUIRE_EQUAL(result.rects.size(),size_t{4});
    UISE_TEST_CHECK_EQUAL(result.placedCount,size_t{4});
    UISE_TEST_CHECK(!result.truncated);
    checkNoOverlap(result.rects);

    // row 1 (height 20): item0 (height 10) centred at y=5; item1 (height 20) at y=0
    checkRect(result.rects[0],0,5,30,10);
    checkRect(result.rects[1],36,0,30,20);

    // row 2 starts at y=0+20+4=24, both items height 10 so no centring offset
    checkRect(result.rects[2],0,24,30,10);
    checkRect(result.rects[3],36,24,30,10);

    checkRect(result.lastRowRect,0,24,66,10);
    checkSize(result.totalSize,66,34);
    UISE_TEST_CHECK(result.tailFits);
}

BOOST_AUTO_TEST_CASE(TestUnboundedWidthNeverWraps)
{
    // maxWidth<=0 means unbounded -- everything stays on one row regardless of total width.
    std::vector<QSize> items{{1000,10},{1000,10}};

    FlowPackOptions options;
    options.maxWidth=0;
    options.hSpacing=6;

    auto result=flowPack(items,options);

    UISE_TEST_REQUIRE_EQUAL(result.rects.size(),size_t{2});
    checkRect(result.rects[0],0,0,1000,10);
    checkRect(result.rects[1],1006,0,1000,10);
    checkRect(result.lastRowRect,0,0,2006,10);
    checkSize(result.totalSize,2006,10);
}

BOOST_AUTO_TEST_CASE(TestReservedTailWidthPeelsLastItem)
{
    // Two 20px items fit one row at maxWidth=50 (20+6+20=46<=50) WITHOUT a reservation, but a
    // 20px tail reservation on top of that overflows (46+6+20=72>50) -- the last item must be
    // peeled onto a row of its own, and the peeled-off row must then satisfy the reservation.
    std::vector<QSize> items{{20,10},{20,10}};

    FlowPackOptions options;
    options.maxWidth=50;
    options.hSpacing=6;
    options.vSpacing=4;
    options.reservedTailWidth=20;

    auto result=flowPack(items,options);

    UISE_TEST_REQUIRE_EQUAL(result.rects.size(),size_t{2});
    checkNoOverlap(result.rects);

    checkRect(result.rects[0],0,0,20,10);
    checkRect(result.rects[1],0,14,20,10);

    checkRect(result.lastRowRect,0,14,20,10);
    checkSize(result.totalSize,20,24);
    UISE_TEST_CHECK(result.tailFits);
}

BOOST_AUTO_TEST_CASE(TestReservedTailWidthDoesNotFit)
{
    // A single item already fills the row close enough that no reservation can be honoured, even
    // alone on its own row -- flowPack() must report this rather than silently overflow maxWidth.
    std::vector<QSize> items{{45,10}};

    FlowPackOptions options;
    options.maxWidth=50;
    options.hSpacing=6;
    options.reservedTailWidth=20;

    auto result=flowPack(items,options);

    UISE_TEST_REQUIRE_EQUAL(result.rects.size(),size_t{1});
    checkRect(result.rects[0],0,0,45,10);
    UISE_TEST_CHECK(!result.tailFits);
}

BOOST_AUTO_TEST_CASE(TestMaxItemsTruncatesWithTailItem)
{
    // 5 items, maxItems=3 -> 2 real items placed, then the tail ("...") chip takes the 3rd slot.
    // The tail's height must come from the tallest of the REAL items actually placed (10 vs 20),
    // not from items that got dropped entirely.
    std::vector<QSize> items{{10,10},{10,20},{10,10},{10,10},{10,10}};

    FlowPackOptions options;
    options.maxWidth=100;
    options.hSpacing=6;
    options.maxItems=3;
    options.tailItemWidth=15;

    auto result=flowPack(items,options);

    UISE_TEST_CHECK(result.truncated);
    UISE_TEST_CHECK_EQUAL(result.placedCount,size_t{3});
    UISE_TEST_REQUIRE_EQUAL(result.rects.size(),size_t{3});
    checkNoOverlap(result.rects);

    checkRect(result.rects[0],0,5,10,10);
    checkRect(result.rects[1],16,0,10,20);
    // tail item (width 15, height inherited from the tallest REAL placed item, 20)
    checkRect(result.rects[2],32,0,15,20);

    checkRect(result.lastRowRect,0,0,47,20);
    checkSize(result.totalSize,47,20);
}

BOOST_AUTO_TEST_CASE(TestMaxItemsOfOneIsTailOnly)
{
    // maxItems==1 leaves no room for any real item -- the lone placed item is the tail chip,
    // whose height falls back to the tallest of the ORIGINAL (not just the placed) items.
    std::vector<QSize> items{{10,10},{10,10}};

    FlowPackOptions options;
    options.maxWidth=100;
    options.maxItems=1;
    options.tailItemWidth=12;

    auto result=flowPack(items,options);

    UISE_TEST_CHECK(result.truncated);
    UISE_TEST_REQUIRE_EQUAL(result.rects.size(),size_t{1});
    checkRect(result.rects[0],0,0,12,10);
}

BOOST_AUTO_TEST_CASE(TestMaxItemsNotExceededMeansNoTruncation)
{
    // maxItems equal to (not less than) the input count must NOT truncate -- overflow is strictly
    // "more than the limit", matching how a host decides whether to show an ellipsis chip.
    std::vector<QSize> items{{10,10},{10,10},{10,10}};

    FlowPackOptions options;
    options.maxWidth=100;
    options.maxItems=3;
    options.tailItemWidth=12;

    auto result=flowPack(items,options);

    UISE_TEST_CHECK(!result.truncated);
    UISE_TEST_CHECK_EQUAL(result.placedCount,size_t{3});
}

BOOST_AUTO_TEST_SUITE_END()
