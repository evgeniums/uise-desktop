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

/** @file uise/test/utils/testalbumlayout.cpp
*
*  Test of albumLayout() -- geometry regression cases from
*  whitemdesktop/todos/closed/todo-album-layout-odd-combinations.md.
*
*/

/****************************************************************************/

#include <algorithm>
#include <set>

#include <boost/test/unit_test.hpp>

#include <uise/test/uise-testthread.hpp>
#include <uise/desktop/utils/albumlayout.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

namespace {

//! Every rect is non-degenerate (positive width/height), no two rects overlap, and the union
//! bounding box equals totalSize. Does NOT assert a minTile floor -- some templates (e.g. the
//! maxHeight rescue, or stackByAspect() under a very tight budget) may legitimately go below it,
//! see their own doc comments; callers that want minTile enforced check it themselves against a
//! budget generous enough for it to hold.
void checkValidGeometry(const std::vector<QRect>& rects, const QSize& totalSize)
{
    UISE_TEST_CHECK(!rects.empty());

    int boundW=0;
    int boundH=0;
    for (size_t i=0;i<rects.size();++i)
    {
        const auto& r=rects[i];
        UISE_TEST_CHECK_GT(r.width(),0);
        UISE_TEST_CHECK_GT(r.height(),0);
        boundW=qMax(boundW,r.x()+r.width());
        boundH=qMax(boundH,r.y()+r.height());

        for (size_t j=i+1;j<rects.size();++j)
        {
            UISE_TEST_CHECK(!r.intersects(rects[j]));
        }
    }

    UISE_TEST_CHECK_EQUAL(boundW,totalSize.width());
    UISE_TEST_CHECK_EQUAL(boundH,totalSize.height());
}

//! Multiset of (width,height) pairs, order-independent -- used to compare two layouts run on a
//! permutation of the same input sizes.
std::multiset<std::pair<int,int>> rectSizeMultiset(const std::vector<QRect>& rects)
{
    std::multiset<std::pair<int,int>> result;
    for (const auto& r : rects)
    {
        result.insert({r.width(),r.height()});
    }
    return result;
}

}

BOOST_AUTO_TEST_SUITE(TestAlbumLayout)

BOOST_AUTO_TEST_CASE(TestEmptyAndSingle)
{
    AlbumLayoutOptions options;

    QSize totalSize;
    auto rects=albumLayout({},options,&totalSize);
    UISE_TEST_CHECK(rects.empty());
    UISE_TEST_CHECK_EQUAL(totalSize.width(),0);
    UISE_TEST_CHECK_EQUAL(totalSize.height(),0);

    // wide single image -- fills the width budget
    rects=albumLayout({QSize(1600,900)},options,&totalSize);
    UISE_TEST_REQUIRE_EQUAL(rects.size(),static_cast<size_t>(1));
    UISE_TEST_CHECK_EQUAL(rects[0].x(),0);
    UISE_TEST_CHECK_EQUAL(rects[0].y(),0);
    UISE_TEST_CHECK_EQUAL(rects[0].width(),options.maxWidth);
    checkValidGeometry(rects,totalSize);

    // tall single image -- fills the height budget
    rects=albumLayout({QSize(900,1600)},options,&totalSize);
    UISE_TEST_REQUIRE_EQUAL(rects.size(),static_cast<size_t>(1));
    UISE_TEST_CHECK_EQUAL(rects[0].height(),options.maxHeight);
    checkValidGeometry(rects,totalSize);

    // square single image
    rects=albumLayout({QSize(500,500)},options,&totalSize);
    UISE_TEST_REQUIRE_EQUAL(rects.size(),static_cast<size_t>(1));
    UISE_TEST_CHECK_EQUAL(rects[0].width(),rects[0].height());
    checkValidGeometry(rects,totalSize);

    // unknown size -- treated as square (aspect 1:1), not degenerate
    rects=albumLayout({QSize(-1,-1)},options,&totalSize);
    UISE_TEST_REQUIRE_EQUAL(rects.size(),static_cast<size_t>(1));
    UISE_TEST_CHECK_EQUAL(rects[0].width(),rects[0].height());
    checkValidGeometry(rects,totalSize);
}

BOOST_AUTO_TEST_CASE(TestTwoImages)
{
    AlbumLayoutOptions options;
    QSize totalSize;

    // both wide -- two stacked full-width rows
    auto rects=albumLayout({QSize(1600,900),QSize(1800,700)},options,&totalSize);
    UISE_TEST_REQUIRE_EQUAL(rects.size(),static_cast<size_t>(2));
    UISE_TEST_CHECK_EQUAL(rects[0].width(),options.maxWidth);
    UISE_TEST_CHECK_EQUAL(rects[1].width(),options.maxWidth);
    checkValidGeometry(rects,totalSize);

    // not both wide -- side-by-side columns summing exactly to maxWidth
    rects=albumLayout({QSize(1200,900),QSize(900,1200)},options,&totalSize);
    UISE_TEST_REQUIRE_EQUAL(rects.size(),static_cast<size_t>(2));
    UISE_TEST_CHECK_EQUAL(rects[0].width()+rects[1].width()+options.spacing,options.maxWidth);
    UISE_TEST_CHECK_EQUAL(rects[0].height(),rects[1].height());
    checkValidGeometry(rects,totalSize);

    // order independence: in the both-wide template each row's height is a function of its own
    // aspect only, so swapping the two images must produce the exact same set of tile sizes (the
    // side-by-side template's own rounding-remainder tile can legitimately differ by 1px
    // depending on which image absorbs it, so that branch is not asserted here).
    auto sizesA=rectSizeMultiset(albumLayout({QSize(1600,900),QSize(1800,700)},options,nullptr));
    auto sizesB=rectSizeMultiset(albumLayout({QSize(1800,700),QSize(1600,900)},options,nullptr));
    UISE_TEST_CHECK(sizesA==sizesB);
}

BOOST_AUTO_TEST_CASE(TestThreeImagesMajority)
{
    AlbumLayoutOptions options;
    QSize totalSize;

    // wide majority (2 of 3) -- ONE hero full-width on top, the other two side by side below.
    // Explicitly NOT a stack of three full-width rows: that shape wastes the bubble's horizontal
    // budget and grows the album into a narrow tall column (observed regression).
    auto rects=albumLayout({QSize(1600,900),QSize(1800,700),QSize(1000,1000)},options,&totalSize);
    UISE_TEST_REQUIRE_EQUAL(rects.size(),static_cast<size_t>(3));
    int fullWidthCount=0;
    for (const auto& r : rects)
    {
        if (r.width()==options.maxWidth)
        {
            ++fullWidthCount;
        }
    }
    UISE_TEST_CHECK_EQUAL(fullWidthCount,1);
    // the two non-hero tiles share a row: same y, same height, widths summing to maxWidth
    {
        std::vector<QRect> row;
        for (const auto& r : rects)
        {
            if (r.width()!=options.maxWidth)
            {
                row.push_back(r);
            }
        }
        UISE_TEST_REQUIRE_EQUAL(row.size(),static_cast<size_t>(2));
        UISE_TEST_CHECK_EQUAL(row[0].y(),row[1].y());
        UISE_TEST_CHECK_EQUAL(row[0].height(),row[1].height());
        UISE_TEST_CHECK_EQUAL(row[0].width()+row[1].width()+options.spacing,options.maxWidth);
    }
    checkValidGeometry(rects,totalSize);

    // tall majority (2 of 3) -- three columns at a shared height, summing to maxWidth
    rects=albumLayout({QSize(600,1200),QSize(700,1300),QSize(1000,1000)},options,&totalSize);
    UISE_TEST_REQUIRE_EQUAL(rects.size(),static_cast<size_t>(3));
    UISE_TEST_CHECK_EQUAL(rects[0].height(),rects[1].height());
    UISE_TEST_CHECK_EQUAL(rects[1].height(),rects[2].height());
    int sumW=rects[0].width()+rects[1].width()+rects[2].width()+2*options.spacing;
    UISE_TEST_CHECK_EQUAL(sumW,options.maxWidth);
    checkValidGeometry(rects,totalSize);

    // the hero slot goes to the FIRST image, always -- tiles are displayed in message order (see
    // HeroIndex). Asserted by index, not by looking for the biggest tile.
    UISE_TEST_CHECK_EQUAL(rects[0].width(),options.maxWidth);
    UISE_TEST_CHECK_EQUAL(rects[0].y(),0);
    UISE_TEST_CHECK_GT(rects[1].y(),rects[0].y());
    UISE_TEST_CHECK_EQUAL(rects[1].x(),0);
    UISE_TEST_CHECK_GT(rects[2].x(),rects[1].x());

    // order independence of the TEMPLATE (not of slot assignment): the same three images in a
    // different order must still pick the same shape -- one full-width hero plus a two-tile row --
    // rather than flipping to a different template the way branching on a[0] alone used to. Which
    // image lands in which slot follows the message, so the individual tile sizes do differ.
    // Given a height budget it cannot exceed, so the maxHeight rescue (which scales the hero below
    // maxWidth) does not obscure the shape being asserted -- a square first image makes this
    // template tall.
    AlbumLayoutOptions tallBudget;
    tallBudget.maxHeight=4000;
    auto reordered=albumLayout({QSize(1000,1000),QSize(1800,700),QSize(1600,900)},tallBudget,nullptr);
    UISE_TEST_REQUIRE_EQUAL(reordered.size(),static_cast<size_t>(3));
    UISE_TEST_CHECK_EQUAL(reordered[0].width(),tallBudget.maxWidth);
    UISE_TEST_CHECK_EQUAL(reordered[1].y(),reordered[2].y());
    UISE_TEST_CHECK_EQUAL(reordered[1].width()+reordered[2].width()+tallBudget.spacing,tallBudget.maxWidth);
}

BOOST_AUTO_TEST_CASE(TestThreeImagesMixed)
{
    AlbumLayoutOptions options;
    QSize totalSize;

    // one clearly wide, one clearly tall, one square-ish -- no majority, so this falls to the
    // mixed template: a big left tile with the other two stacked on its right.
    QSize wideImg(2000,600);   // aspect ~3.33
    QSize tallImg(700,1000);   // aspect 0.7
    QSize squareImg(1000,1050); // aspect ~0.95

    auto order1=albumLayout({wideImg,tallImg,squareImg},options,&totalSize);
    checkValidGeometry(order1,totalSize);

    // the big left tile is the FIRST image (see HeroIndex), and the remaining two stack to its
    // right in message order -- top one first
    UISE_TEST_CHECK_EQUAL(order1[0].x(),0);
    UISE_TEST_CHECK_EQUAL(order1[0].y(),0);
    UISE_TEST_CHECK_GT(order1[1].x(),order1[0].x());
    UISE_TEST_CHECK_EQUAL(order1[1].x(),order1[2].x());
    UISE_TEST_CHECK_GT(order1[2].y(),order1[1].y());

    // reordering keeps the same template shape (big left + stacked pair), just with the images in
    // their new positions -- the first one is the big tile again
    auto order2=albumLayout({tallImg,squareImg,wideImg},options,&totalSize);
    checkValidGeometry(order2,totalSize);
    UISE_TEST_CHECK_EQUAL(order2[0].x(),0);
    UISE_TEST_CHECK_EQUAL(order2[0].y(),0);
    UISE_TEST_CHECK_EQUAL(order2[1].x(),order2[2].x());
    UISE_TEST_CHECK_GT(order2[1].x(),order2[0].x());
}

BOOST_AUTO_TEST_CASE(TestFourImagesMajority)
{
    AlbumLayoutOptions options;
    QSize totalSize;

    // wide majority (3 of 4) -- ONE hero full-width on top, the other three side by side below
    // (not four stacked full-width rows, see the n==3 case's own comment)
    auto rects=albumLayout(
        {QSize(1600,900),QSize(1800,700),QSize(1500,850),QSize(1000,1000)},
        options,&totalSize
    );
    UISE_TEST_REQUIRE_EQUAL(rects.size(),static_cast<size_t>(4));
    int fullWidthCount=0;
    std::vector<QRect> bottomRow;
    for (const auto& r : rects)
    {
        UISE_TEST_CHECK_GT(r.width(),0);
        UISE_TEST_CHECK_GT(r.height(),0);
        if (r.width()==options.maxWidth)
        {
            ++fullWidthCount;
        }
        else
        {
            bottomRow.push_back(r);
        }
    }
    UISE_TEST_CHECK_EQUAL(fullWidthCount,1);
    UISE_TEST_REQUIRE_EQUAL(bottomRow.size(),static_cast<size_t>(3));
    int rowW=bottomRow[0].width()+bottomRow[1].width()+bottomRow[2].width()+2*options.spacing;
    UISE_TEST_CHECK_EQUAL(rowW,options.maxWidth);
    checkValidGeometry(rects,totalSize);

    // the hero slot is the FIRST image and the other three follow it left to right in message
    // order (see HeroIndex)
    UISE_TEST_CHECK_EQUAL(rects[0].width(),options.maxWidth);
    UISE_TEST_CHECK_EQUAL(rects[0].y(),0);
    UISE_TEST_CHECK_EQUAL(rects[1].x(),0);
    UISE_TEST_CHECK_GT(rects[2].x(),rects[1].x());
    UISE_TEST_CHECK_GT(rects[3].x(),rects[2].x());
    UISE_TEST_CHECK_EQUAL(rects[1].y(),rects[2].y());
    UISE_TEST_CHECK_EQUAL(rects[2].y(),rects[3].y());

    // tall majority (3 of 4)
    rects=albumLayout(
        {QSize(600,1200),QSize(700,1300),QSize(650,1250),QSize(1000,1000)},
        options,&totalSize
    );
    UISE_TEST_REQUIRE_EQUAL(rects.size(),static_cast<size_t>(4));
    int sumW=0;
    for (const auto& r : rects)
    {
        sumW+=r.width();
        UISE_TEST_CHECK_GT(r.width(),0);
    }
    sumW+=3*options.spacing;
    UISE_TEST_CHECK_EQUAL(sumW,options.maxWidth);
    checkValidGeometry(rects,totalSize);
}

BOOST_AUTO_TEST_CASE(TestFourImagesMixed)
{
    AlbumLayoutOptions options;
    QSize totalSize;

    // no majority (2 wide, 2 tall) -- 2x2 grid: two rows, each summing to maxWidth
    auto rects=albumLayout(
        {QSize(1600,900),QSize(600,1200),QSize(1500,850),QSize(650,1250)},
        options,&totalSize
    );
    UISE_TEST_REQUIRE_EQUAL(rects.size(),static_cast<size_t>(4));
    UISE_TEST_CHECK_EQUAL(rects[0].y(),rects[1].y());
    UISE_TEST_CHECK_EQUAL(rects[2].y(),rects[3].y());
    UISE_TEST_CHECK_EQUAL(rects[0].width()+rects[1].width()+options.spacing,options.maxWidth);
    UISE_TEST_CHECK_EQUAL(rects[2].width()+rects[3].width()+options.spacing,options.maxWidth);
    checkValidGeometry(rects,totalSize);
}

BOOST_AUTO_TEST_CASE(TestJustifiedRows)
{
    AlbumLayoutOptions options;
    // This case is about the justified packing arithmetic -- rows summing to exactly maxWidth --
    // so both of the passes that legitimately break that invariant are kept out of the way:
    //  * the natural-size cap (it shrinks individual tiles by design), switched off here;
    //  * the maxHeight rescue (it scales the whole album, including row widths), avoided by
    //    giving this album a height budget it cannot exceed.
    // Both have their own coverage -- TestNaturalSizeCap/TestAllThumbnails and
    // TestMaxHeightScaleDown respectively.
    options.devicePixelRatio=0;
    options.maxHeight=4000;
    QSize totalSize;

    // seven images of varied aspect -- every row must sum exactly to maxWidth, no degenerate
    // tiles even though rowHeight clamping is exercised
    std::vector<QSize> seven;
    for (int i=0;i<7;++i)
    {
        int w=200+(i%3)*90;
        int h=200+((i+1)%3)*90;
        seven.emplace_back(w,h);
    }
    auto rects=albumLayout(seven,options,&totalSize);
    UISE_TEST_REQUIRE_EQUAL(rects.size(),static_cast<size_t>(7));
    checkValidGeometry(rects,totalSize);

    // group rects into rows by shared y and check each row sums exactly to maxWidth
    std::vector<int> rowYs;
    for (const auto& r : rects)
    {
        if (std::find(rowYs.begin(),rowYs.end(),r.y())==rowYs.end())
        {
            rowYs.push_back(r.y());
        }
    }
    for (auto y : rowYs)
    {
        int rowW=-options.spacing;
        for (const auto& r : rects)
        {
            if (r.y()==y)
            {
                rowW+=r.width()+options.spacing;
            }
        }
        UISE_TEST_CHECK_EQUAL(rowW,options.maxWidth);
    }

    // one extreme panorama among normal images -- must not produce a degenerate width for
    // itself or for its row siblings (regression for the greedy-fill / last-tile-absorbs bugs)
    std::vector<QSize> withPanorama{
        QSize(3000,1000), // aspect 3.0 -- very wide
        QSize(400,300),QSize(400,300),QSize(400,300),QSize(400,300),QSize(400,300)
    };
    auto rects2=albumLayout(withPanorama,options,&totalSize);
    UISE_TEST_REQUIRE_EQUAL(rects2.size(),static_cast<size_t>(6));
    checkValidGeometry(rects2,totalSize);
}

BOOST_AUTO_TEST_CASE(TestAllThumbnails)
{
    // regression case from the todo: a batch of small (thumbnail-sized) images. Every tile must
    // be capped to its own image's size (never blown up to fill the width budget), so the album
    // as a whole ends up far narrower than maxWidth.
    AlbumLayoutOptions options;
    QSize totalSize;

    std::vector<QSize> thumbs;
    for (int i=0;i<5;++i)
    {
        thumbs.emplace_back(100+(i%2)*20,75+(i%3)*10);
    }
    auto rects=albumLayout(thumbs,options,&totalSize);
    UISE_TEST_REQUIRE_EQUAL(rects.size(),static_cast<size_t>(5));
    checkValidGeometry(rects,totalSize);

    for (size_t i=0;i<rects.size();++i)
    {
        // no tile wider than its own image (allowing the minTile floor -- minCappedTile is unset
        // here, so it falls back to minTile -- and 1px of rounding)
        auto limit=qMax(options.minTile,thumbs[i].width())+1;
        UISE_TEST_CHECK(rects[i].width()<=limit);
    }
    UISE_TEST_CHECK(totalSize.width()<options.maxWidth);
}

BOOST_AUTO_TEST_CASE(TestNaturalSizeCap)
{
    // The reported case: a 100x100 thumbnail sent together with a 2048x2048 photo. They share an
    // aspect ratio, so the aspect-only template gives them identical tiles -- the cap must shrink
    // the thumbnail's tile to its own size while leaving the photo's tile alone, and the row must
    // close up behind it rather than leaving a gap.
    //
    // options.minCappedTile is left at its default (0, "use minTile") throughout this test --
    // see TestMinCappedTileFloor below for the configurable floor todo-album-layout-small-tile-
    // packing.md added, which this same cap mechanism now also serves.
    AlbumLayoutOptions options;
    QSize totalSize;

    const QSize small(100,100);
    const QSize big(2048,2048);
    auto rects=albumLayout({small,big},options,&totalSize);
    UISE_TEST_REQUIRE_EQUAL(rects.size(),static_cast<size_t>(2));
    checkValidGeometry(rects,totalSize);

    // the thumbnail's tile is its own size (dpr 1 here), the photo's is not capped at all
    UISE_TEST_CHECK_EQUAL(rects[0].width(),small.width());
    UISE_TEST_CHECK_EQUAL(rects[0].height(),small.height());
    UISE_TEST_CHECK_GT(rects[1].width(),rects[0].width());

    // no gap left where the small tile shrank, and the album is narrower than the full budget
    UISE_TEST_CHECK_EQUAL(rects[1].x(),rects[0].width()+options.spacing);
    UISE_TEST_CHECK(totalSize.width()<options.maxWidth);

    // the photo alone gets exactly the same tile it gets in the pair -- sending a photo with a
    // thumbnail must not shrink the photo (the "one small image drags the whole grid down"
    // failure), nor grow it
    QSize soloTotal;
    auto solo=albumLayout({big},options,&soloTotal);
    UISE_TEST_REQUIRE_EQUAL(solo.size(),static_cast<size_t>(1));
    UISE_TEST_CHECK_GE(solo[0].width(),rects[1].width());

    // on a 2x display the same thumbnail covers half as many logical px, so its tile halves too
    // -- floored at minTile
    options.devicePixelRatio=2.0;
    auto hidpi=albumLayout({small,big},options,&totalSize);
    UISE_TEST_REQUIRE_EQUAL(hidpi.size(),static_cast<size_t>(2));
    UISE_TEST_CHECK_EQUAL(hidpi[0].width(),qMax(options.minTile,small.width()/2));
    checkValidGeometry(hidpi,totalSize);

    // a tile is never shrunk below minTile even for a 1x1 image
    options.devicePixelRatio=1.0;
    auto tiny=albumLayout({QSize(4,4),big},options,&totalSize);
    UISE_TEST_REQUIRE_EQUAL(tiny.size(),static_cast<size_t>(2));
    UISE_TEST_CHECK_GE(tiny[0].width(),options.minTile);
    UISE_TEST_CHECK_GE(tiny[0].height(),options.minTile);
    checkValidGeometry(tiny,totalSize);
}

BOOST_AUTO_TEST_CASE(TestMinCappedTileFloor)
{
    // todo-album-layout-small-tile-packing.md: rather than a 2D packing pass grouping small tiles
    // together, a small image's tile is floored at a configurable, larger-than-minTile size and
    // the image is scaled up to fill it (aspect preserved) -- exercised here via the same tiny
    // 4x4-vs-2048 pairing TestNaturalSizeCap uses for the minTile floor above.
    AlbumLayoutOptions options;
    const QSize tinyImg(4,4);
    const QSize big(2048,2048);

    // default (0) still falls back to minTile -- reproduces TestNaturalSizeCap's own floor
    // exactly, proving the new field is opt-in and changes nothing when left unset.
    QSize defaultTotal;
    auto atDefault=albumLayout({tinyImg,big},options,&defaultTotal);
    UISE_TEST_REQUIRE_EQUAL(atDefault.size(),static_cast<size_t>(2));
    UISE_TEST_CHECK_EQUAL(atDefault[0].width(),options.minTile);
    UISE_TEST_CHECK_EQUAL(atDefault[0].height(),options.minTile);
    checkValidGeometry(atDefault,defaultTotal);

    // explicit minTile-equal value reproduces the exact same geometry (the "second run with
    // minCappedTile=60" case) -- the knob is a genuine substitute for minTile, not a second,
    // independently-behaving mechanism.
    options.minCappedTile=options.minTile;
    QSize sameTotal;
    auto atSameFloor=albumLayout({tinyImg,big},options,&sameTotal);
    UISE_TEST_REQUIRE_EQUAL(atSameFloor.size(),static_cast<size_t>(2));
    UISE_TEST_CHECK(atSameFloor[0]==atDefault[0]);
    UISE_TEST_CHECK(atSameFloor[1]==atDefault[1]);
    UISE_TEST_CHECK(sameTotal==defaultTotal);

    // a larger floor (100, the default ChatMessageImages ships -- see chatmessagefiles.qss)
    // scales the tiny image's tile up accordingly, aspect preserved (1:1 source -> square tile),
    // still without dragging the photo's own tile down and still closing the row's gap.
    options.minCappedTile=100;
    QSize totalSize;
    auto rects=albumLayout({tinyImg,big},options,&totalSize);
    UISE_TEST_REQUIRE_EQUAL(rects.size(),static_cast<size_t>(2));
    checkValidGeometry(rects,totalSize);
    UISE_TEST_CHECK_EQUAL(rects[0].width(),options.minCappedTile);
    UISE_TEST_CHECK_EQUAL(rects[0].height(),options.minCappedTile);
    UISE_TEST_CHECK_EQUAL(rects[1].x(),rects[0].width()+options.spacing);
    UISE_TEST_CHECK_GT(rects[1].width(),rects[0].width());
    UISE_TEST_CHECK(totalSize.width()<options.maxWidth);

    // the photo alone still gets exactly the same tile regardless of minCappedTile -- the floor
    // must never drag an already-adequately-sized neighbour's tile with it.
    QSize soloTotal;
    auto solo=albumLayout({big},options,&soloTotal);
    UISE_TEST_REQUIRE_EQUAL(solo.size(),static_cast<size_t>(1));
    UISE_TEST_CHECK_GE(solo[0].width(),rects[1].width());
}

BOOST_AUTO_TEST_CASE(TestNormalPhotosNotCapped)
{
    // Guard for the regression this cap could cause: ordinary camera-sized photos are far larger
    // than any tile, so the cap must never fire for them -- their templates (and the full-width
    // rows those produce) must survive untouched.
    AlbumLayoutOptions options;
    QSize capped;
    QSize uncapped;

    std::vector<QSize> photos{QSize(4000,3000),QSize(3800,2900),QSize(4032,3024),QSize(3600,2700)};

    auto withCap=albumLayout(photos,options,&capped);

    options.devicePixelRatio=0; // disables the cap entirely
    auto withoutCap=albumLayout(photos,options,&uncapped);

    UISE_TEST_REQUIRE_EQUAL(withCap.size(),withoutCap.size());
    for (size_t i=0;i<withCap.size();++i)
    {
        UISE_TEST_CHECK(withCap[i]==withoutCap[i]);
    }
    UISE_TEST_CHECK(capped==uncapped);

    // and the album still spends the whole horizontal budget -- the regression reported after the
    // first attempt at this todo was ordinary photos being stacked into a narrow tall column.
    // Not asserted as an exact equality: the maxHeight rescue can shave a pixel off the total.
    UISE_TEST_CHECK_GE(capped.width(),options.maxWidth-2);
}

BOOST_AUTO_TEST_CASE(TestMixedKnownAndUnknownSize)
{
    // one item with an unresolved/unknown pixel size mixed with known ones -- must not crash or
    // produce degenerate geometry (the unknown one is treated as aspect 1:1)
    AlbumLayoutOptions options;
    QSize totalSize;

    auto rects=albumLayout({QSize(1600,900),QSize(-1,-1),QSize(900,1200)},options,&totalSize);
    UISE_TEST_REQUIRE_EQUAL(rects.size(),static_cast<size_t>(3));
    checkValidGeometry(rects,totalSize);
}

BOOST_AUTO_TEST_CASE(TestRealWorldMixAcrossBudgets)
{
    // A real 8-image message that exercised every path at once: two 2048px squares, a tall
    // 1599x2048, three 100x100 thumbnails and two mid-size images. Laid out across the range of
    // bubble budgets a resizing view actually produces, the geometry must stay valid throughout.
    //
    // Note for anyone touching ChatMessageImages::updateMaximumBubbleWidth(): the album's own
    // width is NOT a fixed point of this function. Laying this set out at budget W yields an
    // album narrower than W, and re-running it at that narrower width yields a different (often
    // much narrower again) album, because the number of tiles that fit per row decides how tall
    // the album is, which decides how hard the maxHeight rescue shrinks it. That is why the
    // widget keeps the layout its negotiation settled on instead of re-running it against the
    // bubble width it just produced -- see that function's own comment. Deliberately not asserted
    // here as an invariant: making the layout a fixed point would be an improvement, not a
    // regression, and this test should not stand in its way.
    const std::vector<QSize> mix{
        QSize(100,100),QSize(2048,2048),QSize(1599,2048),QSize(100,100),
        QSize(2048,2048),QSize(442,311),QSize(100,100),QSize(473,454)
    };

    for (int budget : {200,250,300,400,500,600,700,800})
    {
        AlbumLayoutOptions options;
        options.maxWidth=budget;
        options.devicePixelRatio=2.0;

        QSize totalSize;
        auto rects=albumLayout(mix,options,&totalSize);
        UISE_TEST_REQUIRE_EQUAL(rects.size(),mix.size());
        checkValidGeometry(rects,totalSize);

        UISE_TEST_CHECK(totalSize.width()<=options.maxWidth);
        UISE_TEST_CHECK(totalSize.height()<=options.maxHeight);

        // every tile capped to its own image's logical size, floored at minTile (minCappedTile
        // is left at its default here, i.e. 0 -> falls back to minTile -- see
        // TestMinCappedTileFloor for the configurable floor itself)
        for (size_t i=0;i<rects.size();++i)
        {
            auto natW=qMax(options.minTile,qRound(mix[i].width()/options.devicePixelRatio));
            UISE_TEST_CHECK(rects[i].width()<=natW+1);
        }
    }
}

BOOST_AUTO_TEST_CASE(TestMaxHeightScaleDown)
{
    // a two-row stack far taller than maxHeight -- must be scaled down uniformly, stay flush, and
    // re-report totalSize from the SCALED rects rather than from the pre-scale estimate
    AlbumLayoutOptions options;
    options.maxHeight=200; // deliberately small to force the rescue path
    QSize totalSize;

    // both wide (so the stacked two-row template runs) and large enough that the natural-size cap
    // never fires -- this case is specifically about the maxHeight rescue
    auto rects=albumLayout({QSize(1600,1200),QSize(1800,1300)},options,&totalSize);
    UISE_TEST_REQUIRE_EQUAL(rects.size(),static_cast<size_t>(2));
    UISE_TEST_CHECK_EQUAL(totalSize.height(),options.maxHeight);
    UISE_TEST_CHECK(rects[0].y()==0);
    UISE_TEST_CHECK_GT(rects[1].y(),rects[0].y());
    checkValidGeometry(rects,totalSize);
}

BOOST_AUTO_TEST_SUITE_END()
