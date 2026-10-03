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
*  Test of albumLayout() -- the exact-aspect, gapless guillotine layout. Almost every case
*  asserts the function's documented INVARIANTS (see albumlayout.hpp) rather than exact
*  rectangles, so the cost weights in albumlayout.cpp can be tuned visually without churning
*  this file; the few cases that pin a shape say why.
*
*/

/****************************************************************************/

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <QElapsedTimer>

#include <uise/test/uise-testthread.hpp>
#include <uise/desktop/utils/albumlayout.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

namespace {

double aspectOf(const QSize& sz)
{
    if (sz.width()<=0 || sz.height()<=0)
    {
        return 1.0;
    }
    return static_cast<double>(sz.width())/sz.height();
}

//! Every rect is non-degenerate, no two rects overlap, the union bounding box equals totalSize,
//! and both budgets hold.
void checkValidGeometry(const std::vector<QRect>& rects, const QSize& totalSize, const AlbumLayoutOptions& options)
{
    UISE_TEST_CHECK(!rects.empty());

    int boundW=0;
    int boundH=0;
    for (size_t i=0;i<rects.size();++i)
    {
        const auto& r=rects[i];
        UISE_TEST_CHECK(r.width()>0);
        UISE_TEST_CHECK(r.height()>0);
        boundW=qMax(boundW,r.x()+r.width());
        boundH=qMax(boundH,r.y()+r.height());

        for (size_t j=i+1;j<rects.size();++j)
        {
            UISE_TEST_CHECK(!r.intersects(rects[j]));
        }
    }

    UISE_TEST_CHECK_EQUAL(boundW,totalSize.width());
    UISE_TEST_CHECK_EQUAL(boundH,totalSize.height());
    UISE_TEST_CHECK_LE(totalSize.width(),options.maxWidth);
    UISE_TEST_CHECK_LE(totalSize.height(),options.maxHeight);
}

//! Every tile has its image's aspect ratio to within the documented rounding: one pixel per
//! dimension, expressed as a tolerance on the log-ratio.
void checkAspects(const std::vector<QRect>& rects, const std::vector<QSize>& sizes)
{
    for (size_t i=0;i<rects.size();++i)
    {
        const auto& r=rects[i];
        auto tileAspect=static_cast<double>(r.width())/r.height();
        auto imageAspect=aspectOf(sizes[i]);
        auto tolerance=std::log(1.0+2.0/qMin(r.width(),r.height()));
        UISE_TEST_CHECK(std::fabs(std::log(tileAspect/imageAspect))<=tolerance);
    }
}

enum class Side
{
    Left,
    Right,
    Top,
    Bottom
};

//! One edge of tile `i` either lies on the album's border, or is fully covered -- along its whole
//! extent, allowing at most `spacing` between consecutive covering neighbours (the crossing of a
//! perpendicular seam) -- by neighbours whose facing edge is exactly `spacing` away.
bool edgeCovered(const std::vector<QRect>& rects, size_t i, Side side, const QSize& total, int spacing)
{
    const auto& r=rects[i];
    switch (side)
    {
        case Side::Left:
            if (r.x()==0)
            {
                return true;
            }
            break;
        case Side::Right:
            if (r.x()+r.width()==total.width())
            {
                return true;
            }
            break;
        case Side::Top:
            if (r.y()==0)
            {
                return true;
            }
            break;
        case Side::Bottom:
            if (r.y()+r.height()==total.height())
            {
                return true;
            }
            break;
    }

    std::vector<std::pair<int,int>> segments;
    for (size_t j=0;j<rects.size();++j)
    {
        if (j==i)
        {
            continue;
        }
        const auto& o=rects[j];
        bool facing=false;
        switch (side)
        {
            case Side::Left:
                facing=(o.x()+o.width()+spacing==r.x());
                break;
            case Side::Right:
                facing=(o.x()==r.x()+r.width()+spacing);
                break;
            case Side::Top:
                facing=(o.y()+o.height()+spacing==r.y());
                break;
            case Side::Bottom:
                facing=(o.y()==r.y()+r.height()+spacing);
                break;
        }
        if (!facing)
        {
            continue;
        }
        if (side==Side::Left || side==Side::Right)
        {
            segments.emplace_back(o.y(),o.y()+o.height());
        }
        else
        {
            segments.emplace_back(o.x(),o.x()+o.width());
        }
    }
    if (segments.empty())
    {
        return false;
    }
    std::sort(segments.begin(),segments.end());

    int lo=(side==Side::Left || side==Side::Right) ? r.y() : r.x();
    int hi=(side==Side::Left || side==Side::Right) ? r.y()+r.height() : r.x()+r.width();
    if (segments.front().first>lo)
    {
        return false;
    }
    int covered=lo;
    for (const auto& seg : segments)
    {
        if (seg.first>covered+spacing && covered<hi)
        {
            return false;
        }
        covered=qMax(covered,seg.second);
    }
    return covered>=hi;
}

//! No gaps other than the spacing seams: see edgeCovered().
void checkGapless(const std::vector<QRect>& rects, const QSize& total, int spacing)
{
    for (size_t i=0;i<rects.size();++i)
    {
        UISE_TEST_CHECK(edgeCovered(rects,i,Side::Left,total,spacing));
        UISE_TEST_CHECK(edgeCovered(rects,i,Side::Right,total,spacing));
        UISE_TEST_CHECK(edgeCovered(rects,i,Side::Top,total,spacing));
        UISE_TEST_CHECK(edgeCovered(rects,i,Side::Bottom,total,spacing));
    }
}

//! Message order: for i<j, tile j is entirely to the right of tile i or entirely below it.
void checkOrder(const std::vector<QRect>& rects, int spacing)
{
    for (size_t i=0;i<rects.size();++i)
    {
        for (size_t j=i+1;j<rects.size();++j)
        {
            const auto& a=rects[i];
            const auto& b=rects[j];
            bool rightOf=b.x()>=a.x()+a.width()+spacing;
            bool below=b.y()>=a.y()+a.height()+spacing;
            UISE_TEST_CHECK(rightOf || below);
        }
    }
}

//! An album the uniform shrink did not touch always reaches one of its two budgets.
void checkBudgetTouched(const QSize& total, const AlbumLayoutOptions& options)
{
    UISE_TEST_CHECK(total.width()==options.maxWidth || total.height()==options.maxHeight);
}

void checkAll(const std::vector<QRect>& rects, const QSize& total, const std::vector<QSize>& sizes, const AlbumLayoutOptions& options)
{
    UISE_TEST_REQUIRE_EQUAL(rects.size(),sizes.size());
    checkValidGeometry(rects,total,options);
    checkAspects(rects,sizes);
    checkGapless(rects,total,options.spacing);
    checkOrder(rects,options.spacing);
}

int shortSide(const QRect& r)
{
    return qMin(r.width(),r.height());
}

double area(const QRect& r)
{
    return static_cast<double>(r.width())*r.height();
}

//! The eight-image message that exercised every path of the previous algorithm at once: two
//! 2048px squares, a tall 1599x2048, three 100x100 thumbnails and two mid-size images.
const std::vector<QSize> RealWorldMix{
    QSize(100,100),QSize(2048,2048),QSize(1599,2048),QSize(100,100),
    QSize(2048,2048),QSize(442,311),QSize(100,100),QSize(473,454)
};

const std::vector<QSize> TenMixed{
    QSize(1600,900),QSize(600,1200),QSize(1500,850),QSize(650,1250),QSize(1000,1000),
    QSize(1200,800),QSize(800,1200),QSize(1600,900),QSize(900,900),QSize(1400,1000)
};

}

BOOST_AUTO_TEST_SUITE(TestAlbumLayout)

BOOST_AUTO_TEST_CASE(TestEmpty)
{
    AlbumLayoutOptions options;
    QSize totalSize;
    auto rects=albumLayout({},options,&totalSize);
    UISE_TEST_CHECK(rects.empty());
    UISE_TEST_CHECK_EQUAL(totalSize.width(),0);
    UISE_TEST_CHECK_EQUAL(totalSize.height(),0);
}

BOOST_AUTO_TEST_CASE(TestSingleImage)
{
    AlbumLayoutOptions options;
    QSize totalSize;

    // wide -- fills the width budget
    std::vector<QSize> wide{QSize(1600,900)};
    auto rects=albumLayout(wide,options,&totalSize);
    checkAll(rects,totalSize,wide,options);
    UISE_TEST_CHECK_EQUAL(rects[0].x(),0);
    UISE_TEST_CHECK_EQUAL(rects[0].y(),0);
    UISE_TEST_CHECK_EQUAL(rects[0].width(),options.maxWidth);

    // tall -- fills the height budget, narrower than the width budget
    std::vector<QSize> tall{QSize(700,1000)};
    rects=albumLayout(tall,options,&totalSize);
    checkAll(rects,totalSize,tall,options);
    UISE_TEST_CHECK_EQUAL(rects[0].height(),options.maxHeight);
    UISE_TEST_CHECK(rects[0].width()<options.maxWidth);

    // square -- the whole (square) box
    std::vector<QSize> square{QSize(2000,2000)};
    rects=albumLayout(square,options,&totalSize);
    checkAll(rects,totalSize,square,options);
    UISE_TEST_CHECK_EQUAL(rects[0].width(),options.maxWidth);
    UISE_TEST_CHECK_EQUAL(rects[0].height(),options.maxHeight);

    // unknown size -- treated as square, not degenerate, and not shrunk (nothing to shrink to)
    std::vector<QSize> unknown{QSize(-1,-1)};
    rects=albumLayout(unknown,options,&totalSize);
    checkAll(rects,totalSize,unknown,options);
    UISE_TEST_CHECK_EQUAL(rects[0].width(),rects[0].height());
    UISE_TEST_CHECK_EQUAL(rects[0].width(),options.maxWidth);
}

BOOST_AUTO_TEST_CASE(TestSmallAlbumsInvariants)
{
    // The shapes the hand-picked templates used to cover, now all one algorithm -- every one
    // must be exact-aspect, gapless, in order, within budget and touching a budget.
    const std::vector<std::vector<QSize>> albums{
        {QSize(1600,900),QSize(1600,900)},                                   // two wide
        {QSize(1600,900),QSize(700,1000)},                                   // wide + tall
        {QSize(700,1000),QSize(700,1000)},                                   // two tall
        {QSize(700,1000),QSize(700,1000),QSize(1600,900)},                   // 2 portraits + 1 landscape
        {QSize(1000,1000),QSize(1600,900),QSize(1600,900)},                  // square + 2 wide
        {QSize(1600,1200),QSize(1600,1200),QSize(1600,1200)},                // three 4:3
        {QSize(2000,600),QSize(700,1000),QSize(1000,1050)},                  // wide/tall/square
        {QSize(1600,900),QSize(600,1200),QSize(1500,850),QSize(650,1250)},   // 2 wide + 2 tall
        {QSize(1600,1200),QSize(1600,1200),QSize(1600,1200),QSize(1600,1200)}, // four 4:3
        {QSize(700,1000),QSize(700,1000),QSize(700,1000),QSize(700,1000)},   // four portraits
        {QSize(1600,900),QSize(600,1200),QSize(1500,850),QSize(650,1250),QSize(1000,1000)}, // five mixed
        {QSize(200,200),QSize(290,380),QSize(380,290),QSize(200,380),QSize(290,200),QSize(380,200),QSize(200,290)}, // seven (demo generator)
    };

    for (const auto& sizes : albums)
    {
        for (int budget : {320,420,590})
        {
            AlbumLayoutOptions options;
            options.maxWidth=budget;
            QSize totalSize;
            auto rects=albumLayout(sizes,options,&totalSize);
            checkAll(rects,totalSize,sizes,options);
            checkBudgetTouched(totalSize,options);
            // nothing here is small enough to be squeezed under the soft floor
            for (const auto& r : rects)
            {
                UISE_TEST_CHECK_GE(shortSide(r),options.minTile);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(TestTwoLandscapesStack)
{
    // Pinned shape: two 16:9 photos. Side by side they would be a 118px-tall strip across the
    // full width; stacked they are height-bound and a little narrower but four times the area.
    // The cost's area-use term is what prefers the latter -- this guards that preference.
    AlbumLayoutOptions options;
    QSize totalSize;
    std::vector<QSize> sizes{QSize(1600,900),QSize(1600,900)};
    auto rects=albumLayout(sizes,options,&totalSize);
    checkAll(rects,totalSize,sizes,options);
    UISE_TEST_CHECK_EQUAL(rects[0].x(),rects[1].x());
    UISE_TEST_CHECK(rects[1].y()>rects[0].y());
    UISE_TEST_CHECK_EQUAL(totalSize.height(),options.maxHeight);
    UISE_TEST_CHECK(totalSize.width()<options.maxWidth);
}

BOOST_AUTO_TEST_CASE(TestDeterminism)
{
    AlbumLayoutOptions options;
    QSize total1;
    QSize total2;
    auto rects1=albumLayout(RealWorldMix,options,&total1);
    auto rects2=albumLayout(RealWorldMix,options,&total2);
    UISE_TEST_CHECK(rects1==rects2);
    UISE_TEST_CHECK(total1==total2);

    // a permutation may legitimately pick a different structure, but must still be valid
    std::vector<QSize> permuted{QSize(700,1000),QSize(1000,1050),QSize(2000,600)};
    auto rects=albumLayout(permuted,options,&total1);
    checkAll(rects,total1,permuted,options);
}

BOOST_AUTO_TEST_CASE(TestAllThumbnailsShrink)
{
    const std::vector<QSize> thumbs{QSize(60,45),QSize(70,50),QSize(55,40),QSize(65,48),QSize(60,44)};

    // every image smaller than its tile -> the album is shrunk uniformly, down to the floor
    {
        AlbumLayoutOptions options;
        options.devicePixelRatio=1.0;
        QSize totalSize;
        auto rects=albumLayout(thumbs,options,&totalSize);
        checkAll(rects,totalSize,thumbs,options);
        UISE_TEST_CHECK(totalSize.width()<options.maxWidth);
        UISE_TEST_CHECK(totalSize.height()<options.maxHeight);
        for (const auto& r : rects)
        {
            UISE_TEST_CHECK_GE(shortSide(r),options.minTile-1);
        }
    }

    // the shrink floor is configurable separately (ChatMessageImages feeds its minTileSize)
    {
        AlbumLayoutOptions options;
        options.devicePixelRatio=1.0;
        options.shrinkFloor=100;
        QSize totalSize;
        auto rects=albumLayout(thumbs,options,&totalSize);
        checkAll(rects,totalSize,thumbs,options);
        UISE_TEST_CHECK(totalSize.width()<options.maxWidth);
        for (const auto& r : rects)
        {
            UISE_TEST_CHECK_GE(shortSide(r),options.shrinkFloor-1);
        }
    }

    // one normal photo in the set and nothing shrinks -- the thumbnails are upscaled instead
    {
        auto withPhoto=thumbs;
        withPhoto.push_back(QSize(1600,900));
        AlbumLayoutOptions options;
        options.devicePixelRatio=1.0;
        QSize totalSize;
        auto rects=albumLayout(withPhoto,options,&totalSize);
        checkAll(rects,totalSize,withPhoto,options);
        checkBudgetTouched(totalSize,options);
    }

    // devicePixelRatio 0 disables the shrink entirely
    {
        AlbumLayoutOptions options;
        options.devicePixelRatio=0;
        QSize totalSize;
        auto rects=albumLayout(thumbs,options,&totalSize);
        checkAll(rects,totalSize,thumbs,options);
        checkBudgetTouched(totalSize,options);
    }
}

BOOST_AUTO_TEST_CASE(TestSingleSmallImageShrinks)
{
    // the one-image instance of the uniform shrink: shown at natural logical size, floored
    AlbumLayoutOptions options;
    QSize totalSize;

    std::vector<QSize> hundred{QSize(100,100)};
    options.devicePixelRatio=1.0;
    auto rects=albumLayout(hundred,options,&totalSize);
    checkAll(rects,totalSize,hundred,options);
    UISE_TEST_CHECK_EQUAL(rects[0].width(),100);
    UISE_TEST_CHECK_EQUAL(rects[0].height(),100);

    std::vector<QSize> twoHundred{QSize(200,200)};
    options.devicePixelRatio=2.0;
    rects=albumLayout(twoHundred,options,&totalSize);
    checkAll(rects,totalSize,twoHundred,options);
    UISE_TEST_CHECK_EQUAL(rects[0].width(),100);
    UISE_TEST_CHECK_EQUAL(rects[0].height(),100);

    // below the floor -> held at the floor (minTile, since shrinkFloor is 0 here)
    std::vector<QSize> thirty{QSize(30,30)};
    options.devicePixelRatio=1.0;
    rects=albumLayout(thirty,options,&totalSize);
    checkAll(rects,totalSize,thirty,options);
    UISE_TEST_CHECK_EQUAL(rects[0].width(),options.minTile);
    UISE_TEST_CHECK_EQUAL(rects[0].height(),options.minTile);
}

BOOST_AUTO_TEST_CASE(TestThumbnailSteering)
{
    // three 100x100 thumbnails interleaved with three photos: the thumbnails must land in the
    // smaller slots on average, and the largest tile must be a photo's. Not asserted per tile:
    // a thumbnail directly beside a same-aspect photo necessarily shares its size (see
    // albumlayout.hpp), and the structure is free to change with tuning.
    const std::vector<QSize> sizes{
        QSize(100,100),QSize(2048,2048),QSize(100,100),QSize(2048,1365),QSize(100,100),QSize(1600,1200)
    };
    for (qreal dpr : {1.0,2.0})
    {
        AlbumLayoutOptions options;
        options.devicePixelRatio=dpr;
        QSize totalSize;
        auto rects=albumLayout(sizes,options,&totalSize);
        checkAll(rects,totalSize,sizes,options);
        checkBudgetTouched(totalSize,options);

        double thumbArea=(area(rects[0])+area(rects[2])+area(rects[4]))/3.0;
        double photoArea=(area(rects[1])+area(rects[3])+area(rects[5]))/3.0;
        UISE_TEST_CHECK(thumbArea<photoArea);

        size_t largest=0;
        for (size_t i=1;i<rects.size();++i)
        {
            if (area(rects[i])>area(rects[largest]))
            {
                largest=i;
            }
        }
        UISE_TEST_CHECK(largest==1 || largest==3 || largest==5);
    }
}

BOOST_AUTO_TEST_CASE(TestMaxHeightHard)
{
    // portraits are what used to overflow the height budget -- now both budgets are hard and a
    // too-tall structure is simply scaled (and thereby narrowed) to fit
    for (int count : {4,10})
    {
        std::vector<QSize> sizes(static_cast<size_t>(count),QSize(700,1000));
        for (auto box : {QSize(420,420),QSize(200,1000),QSize(1000,300)})
        {
            AlbumLayoutOptions options;
            options.maxWidth=box.width();
            options.maxHeight=box.height();
            QSize totalSize;
            auto rects=albumLayout(sizes,options,&totalSize);
            checkAll(rects,totalSize,sizes,options);
            checkBudgetTouched(totalSize,options);
        }
    }
}

BOOST_AUTO_TEST_CASE(TestMixedKnownAndUnknownSize)
{
    // an unknown size lays out as a square and does not trigger the shrink
    AlbumLayoutOptions options;
    QSize totalSize;
    std::vector<QSize> sizes{QSize(1600,900),QSize(0,0),QSize(700,1000)};
    auto rects=albumLayout(sizes,options,&totalSize);
    checkAll(rects,totalSize,sizes,options);
    checkBudgetTouched(totalSize,options);
    UISE_TEST_CHECK(qAbs(rects[1].width()-rects[1].height())<=1);
}

BOOST_AUTO_TEST_CASE(TestRealWorldMixAcrossBudgets)
{
    for (int budget : {320,420,590,800})
    {
        AlbumLayoutOptions options;
        options.maxWidth=budget;
        options.devicePixelRatio=2.0;
        QSize totalSize;
        auto rects=albumLayout(RealWorldMix,options,&totalSize);
        checkAll(rects,totalSize,RealWorldMix,options);
        // the 2048px squares are photos, so this album never shrinks
        checkBudgetTouched(totalSize,options);
    }
}

BOOST_AUTO_TEST_CASE(TestPlaceholderBudgets)
{
    // mirrors ChatMessageImages::rebuildGrid()'s allPlaceholders branch at the shipped 100px
    // minTileSize: extent 105, box two extents (plus one seam) wide and tall
    const int extent=105;
    const int box=extent*2+2;

    {
        AlbumLayoutOptions options;
        options.maxWidth=extent;
        options.maxHeight=extent;
        QSize totalSize;
        std::vector<QSize> one{QSize(0,0)};
        auto rects=albumLayout(one,options,&totalSize);
        checkAll(rects,totalSize,one,options);
        UISE_TEST_CHECK_EQUAL(rects[0].width(),extent);
        UISE_TEST_CHECK_EQUAL(rects[0].height(),extent);
    }

    {
        AlbumLayoutOptions options;
        options.maxWidth=box;
        options.maxHeight=box;
        QSize totalSize;
        std::vector<QSize> two{QSize(0,0),QSize(0,0)};
        auto rects=albumLayout(two,options,&totalSize);
        checkAll(rects,totalSize,two,options);
        // two squares side by side
        UISE_TEST_CHECK_EQUAL(rects[0].y(),rects[1].y());
        UISE_TEST_CHECK_EQUAL(rects[0].width(),extent);
        UISE_TEST_CHECK_EQUAL(rects[1].width(),extent);
        UISE_TEST_CHECK_EQUAL(totalSize.height(),extent);
    }

    for (int count : {3,4,5})
    {
        AlbumLayoutOptions options;
        options.maxWidth=box;
        options.maxHeight=box;
        QSize totalSize;
        std::vector<QSize> many(static_cast<size_t>(count),QSize(0,0));
        auto rects=albumLayout(many,options,&totalSize);
        checkAll(rects,totalSize,many,options);
        checkBudgetTouched(totalSize,options);
        for (const auto& r : rects)
        {
            UISE_TEST_CHECK_GE(shortSide(r),options.minTile);
        }
    }
}

BOOST_AUTO_TEST_CASE(TestFuzz)
{
    std::mt19937 rng(20261003u);
    std::uniform_int_distribution<int> countDist(1,12);
    std::uniform_real_distribution<double> aspectDist(0.3,3.5);
    std::uniform_int_distribution<int> heightDist(600,3000);
    std::uniform_int_distribution<int> thumbDist(30,120);
    std::uniform_real_distribution<double> unitDist(0.0,1.0);
    const std::vector<QSize> boxes{QSize(320,320),QSize(420,420),QSize(590,420),QSize(200,600),QSize(1000,300)};
    const std::vector<int> spacings{0,2,8};
    const std::vector<qreal> dprs{1.0,2.0};

    for (int iteration=0;iteration<400;++iteration)
    {
        auto count=countDist(rng);
        std::vector<QSize> sizes;
        sizes.reserve(static_cast<size_t>(count));
        for (int i=0;i<count;++i)
        {
            if (unitDist(rng)<0.2)
            {
                sizes.emplace_back(thumbDist(rng),thumbDist(rng));
            }
            else
            {
                auto h=heightDist(rng);
                sizes.emplace_back(qMax(1,qRound(h*aspectDist(rng))),h);
            }
        }

        AlbumLayoutOptions options;
        auto box=boxes[static_cast<size_t>(iteration)%boxes.size()];
        options.maxWidth=box.width();
        options.maxHeight=box.height();
        options.spacing=spacings[static_cast<size_t>(iteration)%spacings.size()];
        options.devicePixelRatio=dprs[static_cast<size_t>(iteration)%dprs.size()];

        QSize totalSize;
        auto rects=albumLayout(sizes,options,&totalSize);
        checkAll(rects,totalSize,sizes,options);
    }
}

BOOST_AUTO_TEST_CASE(TestLargeCountDoesNotCrash)
{
    // far above the default per-message cap, but the cap is configurable -- the layout must stay
    // valid (and finish) however many images it is handed
    for (int count : {25,60})
    {
        std::vector<QSize> sizes;
        for (int i=0;i<count;++i)
        {
            sizes.push_back(TenMixed[static_cast<size_t>(i)%TenMixed.size()]);
        }
        AlbumLayoutOptions options;
        QSize totalSize;
        auto rects=albumLayout(sizes,options,&totalSize);
        checkAll(rects,totalSize,sizes,options);
    }
}

BOOST_AUTO_TEST_CASE(TestNarrowBudget)
{
    // a budget far too small for the soft floor: still valid geometry, within budget
    AlbumLayoutOptions options;
    options.maxWidth=50;
    QSize totalSize;
    std::vector<QSize> sizes{QSize(1600,900),QSize(700,1000),QSize(1000,1000)};
    auto rects=albumLayout(sizes,options,&totalSize);
    checkAll(rects,totalSize,sizes,options);
}

BOOST_AUTO_TEST_CASE(TestTiming)
{
    // albumLayout() runs on every bubble-width negotiation (memoised, but the first pass per
    // width is live) -- keep a ten-image album well inside a frame
    AlbumLayoutOptions options;
    QSize totalSize;
    QElapsedTimer timer;
    timer.start();
    const int runs=100;
    for (int i=0;i<runs;++i)
    {
        auto rects=albumLayout(TenMixed,options,&totalSize);
        UISE_TEST_REQUIRE_EQUAL(rects.size(),TenMixed.size());
    }
    auto meanMs=static_cast<double>(timer.nsecsElapsed())/runs/1e6;
    UISE_TEST_MESSAGE("albumLayout() n=10 mean " << meanMs << " ms");
    UISE_TEST_CHECK(meanMs<50.0);
}

/********************** AlbumLayoutMode::PresetTemplates **********************/

namespace {

//! Preset-mode invariants: valid geometry within budget, gapless seams, message order. Aspects
//! are NOT checked (that mode crops), but every block of two or more images must span the full
//! width budget unless the all-thumbnail shrink applied.
void checkPresets(const std::vector<QRect>& rects, const QSize& total, const std::vector<QSize>& sizes, const AlbumLayoutOptions& options, bool expectFullWidth=true)
{
    UISE_TEST_REQUIRE_EQUAL(rects.size(),sizes.size());
    checkValidGeometry(rects,total,options);
    checkGapless(rects,total,options.spacing);
    checkOrder(rects,options.spacing);
    if (expectFullWidth && sizes.size()>1)
    {
        UISE_TEST_CHECK_EQUAL(total.width(),options.maxWidth);
    }
}

AlbumLayoutOptions presetOptions()
{
    AlbumLayoutOptions options;
    options.mode=AlbumLayoutMode::PresetTemplates;
    return options;
}

}

BOOST_AUTO_TEST_CASE(TestPresetsDispatchAndSingle)
{
    auto options=presetOptions();
    QSize viaAlbumLayout;
    QSize direct;
    auto rects1=albumLayout(RealWorldMix,options,&viaAlbumLayout);
    auto rects2=albumLayoutPresets(RealWorldMix,options,&direct);
    UISE_TEST_CHECK(rects1==rects2);
    UISE_TEST_CHECK(viaAlbumLayout==direct);

    // single wide image: full width, height from the CLAMPED ratio (1.7 here, the image is 1.78)
    std::vector<QSize> wide{QSize(1600,900)};
    auto rects=albumLayout(wide,options,&direct);
    checkPresets(rects,direct,wide,options);
    UISE_TEST_CHECK_EQUAL(rects[0].width(),options.maxWidth);
    UISE_TEST_CHECK_EQUAL(rects[0].height(),options.maxWidth*1000/options.presets.maxRatio);

    // single tall image: height-bound, narrower
    std::vector<QSize> tall{QSize(700,1000)};
    rects=albumLayout(tall,options,&direct);
    checkPresets(rects,direct,tall,options);
    UISE_TEST_CHECK_EQUAL(rects[0].height(),options.maxHeight);
    UISE_TEST_CHECK(rects[0].width()<options.maxWidth);

    // empty
    rects=albumLayout({},options,&direct);
    UISE_TEST_CHECK(rects.empty());
    UISE_TEST_CHECK_EQUAL(direct.width(),0);
}

BOOST_AUTO_TEST_CASE(TestPresetsTemplates)
{
    auto options=presetOptions();
    QSize total;

    // two similar landscapes stack, each full width (then squeezed into the height budget)
    std::vector<QSize> twoWide{QSize(1600,900),QSize(1600,900)};
    auto rects=albumLayout(twoWide,options,&total);
    checkPresets(rects,total,twoWide,options);
    UISE_TEST_CHECK_EQUAL(rects[0].width(),options.maxWidth);
    UISE_TEST_CHECK_EQUAL(rects[1].width(),options.maxWidth);
    UISE_TEST_CHECK(rects[1].y()>rects[0].y());
    UISE_TEST_CHECK_EQUAL(total.height(),options.maxHeight);

    // two similar portraits: an exact half each, side by side
    std::vector<QSize> twoTall{QSize(700,1000),QSize(720,1000)};
    rects=albumLayout(twoTall,options,&total);
    checkPresets(rects,total,twoTall,options);
    UISE_TEST_CHECK_EQUAL(rects[0].y(),rects[1].y());
    UISE_TEST_CHECK(qAbs(rects[0].width()-rects[1].width())<=1);

    // three narrow images: one row of three
    std::vector<QSize> threeNarrow(3,QSize(700,1000));
    rects=albumLayout(threeNarrow,options,&total);
    checkPresets(rects,total,threeNarrow,options);
    UISE_TEST_CHECK_EQUAL(rects[0].y(),rects[1].y());
    UISE_TEST_CHECK_EQUAL(rects[1].y(),rects[2].y());

    // wide first of four: full-width hero on top, a row of three below
    std::vector<QSize> fourWideFirst{QSize(1600,900),QSize(1000,1000),QSize(700,1000),QSize(1200,800)};
    rects=albumLayout(fourWideFirst,options,&total);
    checkPresets(rects,total,fourWideFirst,options);
    UISE_TEST_CHECK_EQUAL(rects[0].width(),options.maxWidth);
    UISE_TEST_CHECK_EQUAL(rects[1].y(),rects[2].y());
    UISE_TEST_CHECK_EQUAL(rects[2].y(),rects[3].y());
    UISE_TEST_CHECK(rects[1].y()>rects[0].y());

    // narrow first of four: full-height column on the left, a stack of three on the right
    std::vector<QSize> fourNarrowFirst{QSize(700,1000),QSize(1600,900),QSize(1000,1000),QSize(1600,900)};
    rects=albumLayout(fourNarrowFirst,options,&total);
    checkPresets(rects,total,fourNarrowFirst,options);
    UISE_TEST_CHECK_EQUAL(rects[0].height(),total.height());
    UISE_TEST_CHECK_EQUAL(rects[1].x(),rects[2].x());
    UISE_TEST_CHECK_EQUAL(rects[2].x(),rects[3].x());
    UISE_TEST_CHECK(rects[1].y()<rects[2].y() && rects[2].y()<rects[3].y());

    // square first of four: 2x2 with one shared vertical seam
    std::vector<QSize> fourSquareFirst{QSize(1000,1000),QSize(1600,900),QSize(600,1200),QSize(1500,850)};
    rects=albumLayout(fourSquareFirst,options,&total);
    checkPresets(rects,total,fourSquareFirst,options);
    UISE_TEST_CHECK_EQUAL(rects[0].x(),rects[2].x());
    UISE_TEST_CHECK_EQUAL(rects[1].x(),rects[3].x());
    UISE_TEST_CHECK_EQUAL(rects[0].width(),rects[2].width());
    UISE_TEST_CHECK_EQUAL(rects[0].y(),rects[1].y());
    UISE_TEST_CHECK_EQUAL(rects[2].y(),rects[3].y());
}

BOOST_AUTO_TEST_CASE(TestPresetsCompositions)
{
    auto options=presetOptions();
    QSize total;

    // seven squares: the documented 3+4 tie-break (lexicographically smaller composition wins)
    std::vector<QSize> seven(7,QSize(1000,1000));
    auto rects=albumLayout(seven,options,&total);
    checkPresets(rects,total,seven,options);
    UISE_TEST_CHECK_EQUAL(rects[0].y(),rects[2].y());
    UISE_TEST_CHECK(rects[3].y()>rects[2].y());
    UISE_TEST_CHECK_EQUAL(rects[3].y(),rects[6].y());

    // ten portraits never exceed the height budget (squeezed if needed)
    std::vector<QSize> tenTall(10,QSize(700,1000));
    rects=albumLayout(tenTall,options,&total);
    checkPresets(rects,total,tenTall,options);

    // more images than maxRows x maxPerRow can hold still lays out validly
    std::vector<QSize> many;
    for (int i=0;i<25;++i)
    {
        many.push_back(TenMixed[static_cast<size_t>(i)%TenMixed.size()]);
    }
    rects=albumLayout(many,options,&total);
    checkPresets(rects,total,many,options);

    // determinism
    QSize total2;
    auto again=albumLayout(TenMixed,options,&total2);
    rects=albumLayout(TenMixed,options,&total);
    UISE_TEST_CHECK(rects==again);
}

BOOST_AUTO_TEST_CASE(TestPresetsThumbnailShrink)
{
    auto options=presetOptions();
    options.devicePixelRatio=1.0;
    QSize total;

    // a lone 100x100 is not blown up to the bubble width
    std::vector<QSize> hundred{QSize(100,100)};
    auto rects=albumLayout(hundred,options,&total);
    checkPresets(rects,total,hundred,options,false);
    UISE_TEST_CHECK_LE(rects[0].width(),100);
    UISE_TEST_CHECK_GE(rects[0].width(),options.minTile);

    // all-thumbnail album shrinks, floored; one photo in it and it does not
    const std::vector<QSize> thumbs{QSize(60,45),QSize(70,50),QSize(55,40),QSize(65,48),QSize(60,44)};
    options.shrinkFloor=100;
    rects=albumLayout(thumbs,options,&total);
    checkPresets(rects,total,thumbs,options,false);
    UISE_TEST_CHECK(total.width()<options.maxWidth);
    for (const auto& r : rects)
    {
        UISE_TEST_CHECK_GE(shortSide(r),options.shrinkFloor-1);
    }

    auto withPhoto=thumbs;
    withPhoto.push_back(QSize(1600,900));
    rects=albumLayout(withPhoto,options,&total);
    checkPresets(rects,total,withPhoto,options);
}

BOOST_AUTO_TEST_CASE(TestPresetsFuzz)
{
    std::mt19937 rng(20261003u);
    std::uniform_int_distribution<int> countDist(1,12);
    std::uniform_real_distribution<double> aspectDist(0.25,4.0);
    std::uniform_int_distribution<int> heightDist(600,3000);
    std::uniform_int_distribution<int> thumbDist(30,120);
    std::uniform_real_distribution<double> unitDist(0.0,1.0);
    const std::vector<QSize> boxes{QSize(320,320),QSize(420,420),QSize(590,420),QSize(200,600),QSize(1000,300),QSize(50,420)};
    const std::vector<int> spacings{0,2,8};

    for (int iteration=0;iteration<400;++iteration)
    {
        auto count=countDist(rng);
        std::vector<QSize> sizes;
        for (int i=0;i<count;++i)
        {
            if (unitDist(rng)<0.2)
            {
                sizes.emplace_back(thumbDist(rng),thumbDist(rng));
            }
            else
            {
                auto h=heightDist(rng);
                sizes.emplace_back(qMax(1,qRound(h*aspectDist(rng))),h);
            }
        }
        auto options=presetOptions();
        auto box=boxes[static_cast<size_t>(iteration)%boxes.size()];
        options.maxWidth=box.width();
        options.maxHeight=box.height();
        options.spacing=spacings[static_cast<size_t>(iteration)%spacings.size()];
        options.devicePixelRatio=(iteration%2==0) ? 1.0 : 2.0;

        QSize total;
        auto rects=albumLayout(sizes,options,&total);
        // the shrink may legitimately narrow an all-thumbnail album, so full width is not asserted
        checkPresets(rects,total,sizes,options,false);
    }
}

BOOST_AUTO_TEST_SUITE_END()
