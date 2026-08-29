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

/** @file uise/desktop/src/albumlayout.cpp
*
*  Defines albumLayout().
*
*/

/****************************************************************************/

#include <algorithm>

#include <QtGlobal>

#include <uise/desktop/utils/albumlayout.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

qreal aspectOf(const QSize& sz)
{
    if (sz.width()<=0 || sz.height()<=0)
    {
        return 1.0;
    }
    return static_cast<qreal>(sz.width())/static_cast<qreal>(sz.height());
}

//! Aspect-ratio class used to pick a 3/4-image template from the MULTISET of classes across all
//! images in the album, rather than from a single image's aspect -- see classify()'s own
//! rationale in albumLayout()'s header doc comment.
enum class AspectClass
{
    Tall,
    Square,
    Wide
};

constexpr qreal TallThreshold=0.85;
constexpr qreal WideThreshold=1.2;

AspectClass classify(qreal a)
{
    if (a<TallThreshold)
    {
        return AspectClass::Tall;
    }
    if (a>WideThreshold)
    {
        return AspectClass::Wide;
    }
    return AspectClass::Square;
}

int countClass(const std::vector<AspectClass>& classes, AspectClass c)
{
    return static_cast<int>(std::count(classes.begin(),classes.end(),c));
}

qreal clampAspect(qreal a, qreal lo, qreal hi)
{
    return qBound(lo,a,hi);
}

//! Which image gets a template's "hero" slot (the full-width row, or the big left tile): always
//! the FIRST one, so every tile ends up where the message put it -- images are shown in the order
//! they were sent, left to right and top to bottom, and a caption referring to "the first photo"
//! keeps meaning the first tile.
//!
//! An earlier version picked the aspect farthest from square instead, on the grounds that it fits
//! the big slot best. Dropped: it let an image visibly jump position relative to how it was sent,
//! and it was never what made this layout order-independent in the first place -- that comes from
//! choosing the TEMPLATE by the aspect-class multiset over all images (see classify()), which is
//! unaffected by this. Reordering the same images therefore still picks the same shape; only
//! which image occupies which slot follows the message, as it should.
constexpr int HeroIndex=0;

/**
 * @brief Stack tiles of the same width across totalHeight, proportioned by each tile's own
 *  aspect ratio rather than an even split.
 *
 * Each tile's height is solved so that width/height reproduces its own aspect ratio as closely
 * as the minTile floor and the shared totalHeight budget allow (weight_i proportional to
 * 1/aspect_i -- a taller/narrower image asks for more height at a fixed width), floored at
 * minTile, with the last tile absorbing the rounding remainder so the stack still sums to
 * totalHeight whenever the budget allows it. Supersedes the previous even split, which assumed
 * mismatched aspects were absorbed by a center-crop at paint time -- real content is fitted
 * inside its tile and padded, not cropped (see utils/pixmapscale.hpp's scaledToFitPadded()), so
 * an even split just meant a wrong-shaped tile letterboxing harder than it needed to.
 */
std::vector<QRect> stackByAspect(int x, int y, int width, int totalHeight, const std::vector<qreal>& aspects, int spacing, int minTile)
{
    std::vector<QRect> rects;
    auto count=static_cast<int>(aspects.size());
    if (count<=0)
    {
        return rects;
    }

    auto available=qMax(minTile*count,totalHeight-spacing*(count-1));

    std::vector<qreal> weight(static_cast<size_t>(count));
    qreal sumWeight=0;
    for (int i=0;i<count;++i)
    {
        auto asp=aspects[static_cast<size_t>(i)]>0 ? aspects[static_cast<size_t>(i)] : 1.0;
        weight[static_cast<size_t>(i)]=1.0/asp;
        sumWeight+=weight[static_cast<size_t>(i)];
    }

    std::vector<int> heights(static_cast<size_t>(count));
    int used=0;
    for (int i=0;i<count-1;++i)
    {
        auto hh=qMax(minTile,qRound(available*weight[static_cast<size_t>(i)]/sumWeight));
        heights[static_cast<size_t>(i)]=hh;
        used+=hh;
    }
    heights[static_cast<size_t>(count-1)]=qMax(minTile,available-used);

    auto yy=y;
    for (int i=0;i<count;++i)
    {
        rects.emplace_back(x,yy,width,heights[static_cast<size_t>(i)]);
        yy+=heights[static_cast<size_t>(i)]+spacing;
    }
    return rects;
}

}

//--------------------------------------------------------------------------

std::vector<QRect> albumLayout(
        const std::vector<QSize>& pixelSizes,
        const AlbumLayoutOptions& options,
        QSize* totalSize
    )
{
    std::vector<QRect> rects;
    const auto n=static_cast<int>(pixelSizes.size());
    const auto w=options.maxWidth;
    const auto s=options.spacing;

    if (n==0)
    {
        if (totalSize!=nullptr)
        {
            *totalSize=QSize(0,0);
        }
        return rects;
    }

    std::vector<qreal> a(static_cast<size_t>(n));
    for (int i=0;i<n;++i)
    {
        a[static_cast<size_t>(i)]=aspectOf(pixelSizes[static_cast<size_t>(i)]);
    }

    // Which visual row each tile belongs to -- filled in by every template below, and used only
    // by the natural-size cap's re-flow pass (see its own comment). Templates whose tiles are
    // nested rather than strictly rowed (the n==3 mixed hero + stacked pair) report the grouping
    // the re-flow should fall back to, since the cap re-flow cannot preserve nesting.
    std::vector<int> rowIndex(static_cast<size_t>(n),0);

    if (n==1)
    {
        int width;
        int height;
        if (a[0]>=1.0)
        {
            width=w;
            height=qMax(options.minTile,qRound(width/a[0]));
            if (height>options.maxHeight)
            {
                height=options.maxHeight;
                width=qMax(options.minTile,qRound(height*a[0]));
            }
        }
        else
        {
            height=options.maxHeight;
            width=qMax(options.minTile,qRound(height*a[0]));
            if (width>w)
            {
                width=w;
                height=qMax(options.minTile,qRound(width/a[0]));
            }
        }
        rects.emplace_back(0,0,width,height);
    }
    else if (n==2)
    {
        if (classify(a[0])==AspectClass::Wide && classify(a[1])==AspectClass::Wide)
        {
            // both wide -- two stacked full-width rows, each at its own natural height
            auto h0=qMax(options.minTile,qRound(w/a[0]));
            auto h1=qMax(options.minTile,qRound(w/a[1]));
            rects.emplace_back(0,0,w,h0);
            rects.emplace_back(0,h0+s,w,h1);
            rowIndex[0]=0;
            rowIndex[1]=1;
        }
        else
        {
            // side-by-side columns, widths proportional to aspect at a shared height -- this
            // also covers "both tall" (near-equal aspects produce near-equal columns)
            auto h=qMax(options.minTile,qRound((w-s)/(a[0]+a[1])));
            auto w0=qRound(a[0]*h);
            auto w1=(w-s)-w0; // last tile absorbs rounding so the row sums exactly to w
            rects.emplace_back(0,0,w0,h);
            rects.emplace_back(w0+s,0,w1,h);
        }
    }
    else if (n==3)
    {
        std::vector<AspectClass> cls{classify(a[0]),classify(a[1]),classify(a[2])};
        rects.assign(3,QRect());

        if (countClass(cls,AspectClass::Wide)>=2)
        {
            // wide majority -- ONE hero image full width on top, the other two side by side in a
            // proportional row below. Deliberately NOT three stacked full-width rows: stacking
            // every wide image turns the album into a narrow tall column that wastes the bubble's
            // horizontal budget (and, once the bubble's own minimum width kicks in, leaves visible
            // dead space beside it). Prefer spending horizontal space over growing vertically.
            //
            // The hero is the first image (see HeroIndex) and the other two follow it in message
            // order, so the tiles read exactly as the message was sent.
            const auto hero=HeroIndex;
            std::vector<int> others;
            for (int i=0;i<3;++i)
            {
                if (i!=hero)
                {
                    others.push_back(i);
                }
            }

            auto h0=qMax(options.minTile,qRound(w/clampAspect(a[static_cast<size_t>(hero)],0.5,2.5)));
            rects[static_cast<size_t>(hero)]=QRect(0,0,w,h0);

            auto aLeft=a[static_cast<size_t>(others[0])];
            auto aRight=a[static_cast<size_t>(others[1])];
            auto h1=qMax(options.minTile,qRound((w-s)/(aLeft+aRight)));
            auto wLeft=qMax(1,qRound(aLeft*h1));
            // last tile absorbs rounding so the row sums exactly to w; floored at 1 so an
            // extreme-aspect image (h1 pinned at minTile far below what aLeft would otherwise
            // demand) can never drive this to zero/negative width -- same guard the n==4 wide
            // template's own last-tile branch already has, missing here until this fix
            auto wRight=qMax(1,(w-s)-wLeft);
            rects[static_cast<size_t>(others[0])]=QRect(0,h0+s,wLeft,h1);
            rects[static_cast<size_t>(others[1])]=QRect(wLeft+s,h0+s,wRight,h1);

            rowIndex[static_cast<size_t>(hero)]=0;
            rowIndex[static_cast<size_t>(others[0])]=1;
            rowIndex[static_cast<size_t>(others[1])]=1;
        }
        else if (countClass(cls,AspectClass::Tall)>=2)
        {
            // tall majority -- three columns at a shared height, widths proportional to aspect
            auto sumA=a[0]+a[1]+a[2];
            auto h=qMax(options.minTile,qRound((w-2*s)/sumA));
            int x=0;
            for (int i=0;i<2;++i)
            {
                auto tw=qRound(a[static_cast<size_t>(i)]*h);
                rects[static_cast<size_t>(i)]=QRect(x,0,tw,h);
                x+=tw+s;
            }
            auto usedW=rects[0].width()+rects[1].width();
            rects[2]=QRect(x,0,(w-2*s)-usedW,h);
            // all three share the single row -- rowIndex is already 0 for every tile
        }
        else
        {
            // mixed -- the first image becomes the big tile on the left (see HeroIndex); the other
            // two stack on the right in message order, proportioned by their own aspect via
            // stackByAspect()
            const auto hero=HeroIndex;
            std::vector<int> others;
            for (int i=0;i<3;++i)
            {
                if (i!=hero)
                {
                    others.push_back(i);
                }
            }

            auto leftWidth=qRound((w-s)*2.0/3.0);
            auto rightWidth=(w-s)-leftWidth;
            auto leftHeight=qMax(options.minTile,qRound(leftWidth/clampAspect(a[static_cast<size_t>(hero)],0.6,1.6)));
            rects[static_cast<size_t>(hero)]=QRect(0,0,leftWidth,leftHeight);

            std::vector<qreal> otherAspects{a[static_cast<size_t>(others[0])],a[static_cast<size_t>(others[1])]};
            auto rightRects=stackByAspect(leftWidth+s,0,rightWidth,leftHeight,otherAspects,s,options.minTile);
            rects[static_cast<size_t>(others[0])]=rightRects[0];
            rects[static_cast<size_t>(others[1])]=rightRects[1];

            // This is the one nested template (a full-height hero beside a 2-tile column), which
            // the cap's re-flow cannot reproduce -- report the hero and the pair as two rows, so
            // that IF the cap fires the layout degrades to hero-on-top + pair-below rather than
            // producing overlapping rects. With no capping the nested geometry above stands.
            rowIndex[static_cast<size_t>(hero)]=0;
            rowIndex[static_cast<size_t>(others[0])]=1;
            rowIndex[static_cast<size_t>(others[1])]=1;
        }
    }
    else if (n==4)
    {
        std::vector<AspectClass> cls{classify(a[0]),classify(a[1]),classify(a[2]),classify(a[3])};
        rects.assign(4,QRect());

        if (countClass(cls,AspectClass::Wide)>=3)
        {
            // wide majority -- ONE hero image full width on top, the other three side by side in
            // a proportional row below. Same rationale as the n==3 wide-majority template above:
            // never stack every wide image into a narrow tall column, and the hero is the first
            // image (see HeroIndex) so the tiles keep message order.
            const auto hero=HeroIndex;
            std::vector<int> others;
            for (int i=0;i<4;++i)
            {
                if (i!=hero)
                {
                    others.push_back(i);
                }
            }

            auto h0=qMax(options.minTile,qRound(w/clampAspect(a[static_cast<size_t>(hero)],0.5,2.5)));
            rects[static_cast<size_t>(hero)]=QRect(0,0,w,h0);

            qreal sumA=0;
            for (auto idx : others)
            {
                sumA+=a[static_cast<size_t>(idx)];
            }
            auto h1=qMax(options.minTile,qRound((w-2*s)/sumA));

            int x=0;
            int usedW=0;
            for (size_t k=0;k<others.size()-1;++k)
            {
                auto tw=qMax(1,qRound(a[static_cast<size_t>(others[k])]*h1));
                rects[static_cast<size_t>(others[k])]=QRect(x,h0+s,tw,h1);
                x+=tw+s;
                usedW+=tw;
            }
            // last tile absorbs rounding so the row sums exactly to w; floored at 1 so a minTile
            // clamp on h1 can never drive it to zero/negative width
            auto wLast=qMax(1,(w-2*s)-usedW);
            rects[static_cast<size_t>(others.back())]=QRect(x,h0+s,wLast,h1);

            rowIndex[static_cast<size_t>(hero)]=0;
            for (auto idx : others)
            {
                rowIndex[static_cast<size_t>(idx)]=1;
            }
        }
        else if (countClass(cls,AspectClass::Tall)>=3)
        {
            // tall majority -- four columns at a shared height, widths proportional to aspect
            auto sumA=a[0]+a[1]+a[2]+a[3];
            auto h=qMax(options.minTile,qRound((w-3*s)/sumA));
            int x=0;
            for (int i=0;i<3;++i)
            {
                auto tw=qRound(a[static_cast<size_t>(i)]*h);
                rects[static_cast<size_t>(i)]=QRect(x,0,tw,h);
                x+=tw+s;
            }
            auto usedW=rects[0].width()+rects[1].width()+rects[2].width();
            rects[3]=QRect(x,0,(w-3*s)-usedW,h);
            // all four share the single row -- rowIndex is already 0 for every tile
        }
        else
        {
            // mixed -- 2x2 grid: two proportional rows, each a side-by-side pair in display
            // order (images 0,1 on top, 2,3 below), same proportion rule as the n==2 side-by-side
            // template. Unlike the n==3 mixed template this stays purely positional -- with no
            // single class holding a majority there is no data-driven way to pick which pair
            // belongs together, so display order is the least surprising choice.
            auto rowRects=[w,s,&options](int y, qreal aLeft, qreal aRight)
            {
                auto h=qMax(options.minTile,qRound((w-s)/(aLeft+aRight)));
                auto wLeft=qRound(aLeft*h);
                auto wRight=(w-s)-wLeft;
                return std::make_pair(QRect(0,y,wLeft,h),QRect(wLeft+s,y,wRight,h));
            };

            auto row0=rowRects(0,a[0],a[1]);
            rects[0]=row0.first;
            rects[1]=row0.second;

            auto row1Y=qMax(rects[0].height(),rects[1].height())+s;
            auto row1=rowRects(row1Y,a[2],a[3]);
            rects[2]=row1.first;
            rects[3]=row1.second;

            rowIndex[0]=0;
            rowIndex[1]=0;
            rowIndex[2]=1;
            rowIndex[3]=1;
        }
    }
    else
    {
        // justified-rows fallback: greedily fill each row until one more image would exceed the
        // width budget at the target row height (spacing-aware, so the greedy decision matches
        // what the row will actually be rescaled to below), then rescale that row's tiles to sum
        // to exactly w -- a standard justified-gallery packing, the only template here that
        // genuinely scales to any count
        auto targetRowHeight=qBound(options.minTile,qRound(w/3.0),options.maxHeight/2);

        int i=0;
        int y=0;
        int row=0;
        while (i<n)
        {
            auto rowStart=i;
            qreal sumA=0;
            int rowCount=0;
            while (i<n)
            {
                auto nextCount=rowCount+1;
                auto budget=w-s*(nextCount-1);
                if (rowCount>0 && (sumA+a[static_cast<size_t>(i)])*targetRowHeight>=budget)
                {
                    break;
                }
                sumA+=a[static_cast<size_t>(i)];
                ++rowCount;
                ++i;
            }

            auto rowHeight=qBound(options.minTile,qRound((w-s*(rowCount-1))/sumA),options.maxHeight);
            auto rowBudget=w-s*(rowCount-1);

            std::vector<int> widths(static_cast<size_t>(rowCount));
            int sumWidths=0;
            for (int k=0;k<rowCount-1;++k)
            {
                auto tw=qMax(1,qRound(a[static_cast<size_t>(rowStart+k)]*rowHeight));
                widths[static_cast<size_t>(k)]=tw;
                sumWidths+=tw;
            }
            auto lastWidth=rowBudget-sumWidths;
            if (lastWidth<1)
            {
                // rowHeight's minTile/maxHeight clamping pushed the row over budget -- redistribute
                // proportionally across the whole row instead of dumping all the error onto the
                // last tile (which could otherwise go zero/negative width)
                qreal rowSumA=0;
                for (int k=0;k<rowCount;++k)
                {
                    rowSumA+=a[static_cast<size_t>(rowStart+k)];
                }
                int used=0;
                for (int k=0;k<rowCount-1;++k)
                {
                    auto tw=qMax(1,qRound(rowBudget*a[static_cast<size_t>(rowStart+k)]/rowSumA));
                    widths[static_cast<size_t>(k)]=tw;
                    used+=tw;
                }
                lastWidth=qMax(1,rowBudget-used);
            }
            widths[static_cast<size_t>(rowCount-1)]=lastWidth;

            int x=0;
            for (int k=0;k<rowCount;++k)
            {
                rects.emplace_back(x,y,widths[static_cast<size_t>(k)],rowHeight);
                rowIndex[static_cast<size_t>(rowStart+k)]=row;
                x+=widths[static_cast<size_t>(k)]+s;
            }

            y+=rowHeight+s;
            ++row;
        }
    }

    // Floor every tile is guaranteed to reach on BOTH of its axes (see below and
    // AlbumLayoutOptions::minCappedTile). Clamped to the width budget: a floor larger than the
    // album's whole width could never be honoured by any tile, and pretending otherwise only
    // pushes tiles off the edge of a bubble sized from totalSize.
    const auto cappedFloor=qMin((options.minCappedTile>0) ? options.minCappedTile : options.minTile,w);

    // Re-flow every row left to right after a per-tile rescale, so no gap is left where a tile
    // shrank and no overlap where one grew, and so the album's own width collapses to what its
    // tiles actually occupy (the caller sizes the bubble from totalSize, letting it hug a small
    // album). Tiles keep their template order within a row; rows keep their template order.
    // Row-mates are top-aligned -- after a per-tile rescale they legitimately differ in height,
    // which is the whole point: their sizes now reflect the images' real sizes rather than a
    // shared row height.
    //
    // A row that grew past the width budget WRAPS onto a further line rather than overflowing:
    // unlike maxHeight, the width budget is hard -- ChatMessageImages::bubbleWidthHint() clamps
    // the bubble to it (std::min), so a tile sticking out past maxWidth would simply be cut off.
    auto reflowRows=[&rects,&rowIndex,n,w,s](const std::vector<qreal>& scale)
    {
        auto rowCount=(*std::max_element(rowIndex.begin(),rowIndex.end()))+1;
        std::vector<int> order(static_cast<size_t>(n));
        for (int i=0;i<n;++i)
        {
            order[static_cast<size_t>(i)]=i;
        }
        std::stable_sort(order.begin(),order.end(),
            [&rects](int lhs, int rhs)
            {
                return rects[static_cast<size_t>(lhs)].x()<rects[static_cast<size_t>(rhs)].x();
            }
        );

        int y=0;
        for (int row=0;row<rowCount;++row)
        {
            int x=0;
            int lineHeight=0;
            for (auto idx : order)
            {
                if (rowIndex[static_cast<size_t>(idx)]!=row)
                {
                    continue;
                }
                const auto& r=rects[static_cast<size_t>(idx)];
                auto f=scale[static_cast<size_t>(idx)];
                auto tw=qMax(1,qRound(r.width()*f));
                auto th=qMax(1,qRound(r.height()*f));
                if (x>0 && x+tw>w)
                {
                    y+=lineHeight+s;
                    x=0;
                    lineHeight=0;
                }
                rects[static_cast<size_t>(idx)]=QRect(x,y,tw,th);
                x+=tw+s;
                lineHeight=qMax(lineHeight,th);
            }
            if (lineHeight>0)
            {
                y+=lineHeight+s;
            }
        }
    };

    // Per-tile sizing pass, bounded from ABOVE by the image's own resolution and from BELOW by
    // cappedFloor.
    //
    // Upper bound (the natural-size cap): a tile bigger than its image would otherwise be filled
    // by upscaling the content (blurry) or by padding it (the reported "small image gets a big
    // tile with paddings around it"), and it made a 100px thumbnail claim exactly as much room as
    // a 2048px photo whenever they happened to share an aspect ratio.
    //
    // Lower bound (the floor, todo-album-layout-small-tile-packing.md): scaling a rect uniformly
    // by qMax(floor/w,floor/h) puts its SHORT side exactly on the floor and leaves the long side
    // above it, whatever the rect's aspect ratio -- so the floor is always reachable by a pure
    // aspect-preserving scale and NEVER needs a crop (real chat image content is fitted inside its
    // tile and never cropped, see ChatMessageImageItem::updatePreview()).
    //
    // The floor is NOT clamped to <=1.0 and is NOT gated on the natural-size cap having fired. It
    // used to be both, which made this pass shrink-only: a full-resolution photo that a dense
    // justified row packed into a 52x65 slot never had the floor evaluated at all (its own
    // resolution exceeded the slot, so the cap returned early), and a tile the floor should have
    // grown could at best be shrunk less. Both axes of every tile must clear the floor, however
    // small the template packed it.
    //
    // Applied PER TILE, never as a whole-album shrink: an oversized tile must not drag its
    // neighbours' tiles down with it.
    if (options.devicePixelRatio>0)
    {
        std::vector<qreal> scale(static_cast<size_t>(n),1.0);
        bool anyAdjusted=false;

        for (int i=0;i<n;++i)
        {
            const auto& sz=pixelSizes[static_cast<size_t>(i)];
            const auto& r=rects[static_cast<size_t>(i)];
            if (r.width()<=0 || r.height()<=0)
            {
                continue;
            }

            // 1.0 (unbounded) for an unknown resolution: a placeholder has no natural size to cap
            // against, but it still has to honour the floor below -- growing a rect to the floor
            // needs no knowledge of the source, and a placeholder tile is exactly the one that
            // shows a load control and so must not be tiny.
            qreal naturalCap=1.0;
            if (sz.width()>0 && sz.height()>0)
            {
                naturalCap=qMin(1.0,qMin((sz.width()/options.devicePixelRatio)/r.width(),
                                         (sz.height()/options.devicePixelRatio)/r.height()));
            }

            auto floorRequirement=qMax(static_cast<qreal>(cappedFloor)/r.width(),
                                       static_cast<qreal>(cappedFloor)/r.height());

            // the floor wins over natural-size accuracy whenever the two disagree -- a blurry
            // usable tile beats an accurate unusable one
            auto f=qMax(naturalCap,floorRequirement);
            if (f>1.0)
            {
                // ...but never past the width budget, which is hard (see reflowRows() above). A
                // tile whose aspect ratio makes cappedFloor*aspect exceed maxWidth is the one case
                // the floor cannot be honoured without cropping or distorting, so it is not
                // honoured there.
                f=qMax(1.0,qMin(f,static_cast<qreal>(w)/r.width()));
            }

            if (!qFuzzyCompare(f,1.0))
            {
                scale[static_cast<size_t>(i)]=f;
                anyAdjusted=true;
            }
        }

        if (anyAdjusted)
        {
            reflowRows(scale);
        }
    }

    int totalW=0;
    int totalH=0;
    for (const auto& r : rects)
    {
        totalW=qMax(totalW,r.x()+r.width());
        totalH=qMax(totalH,r.y()+r.height());
    }

    // if the album grew taller than the budget, scale every rect down uniformly rather than
    // clip -- keeps every tile's own proportions from its template intact. Scaling each tile's
    // EDGES (not its width/height independently) keeps every row/column's rects flush with the
    // album's own bounding box, which is recomputed from the scaled rects below rather than
    // trusting the pre-scale totalW/totalH times factor.
    if (totalH>options.maxHeight && totalH>0)
    {
        auto factor=static_cast<qreal>(options.maxHeight)/static_cast<qreal>(totalH);
        for (auto& r : rects)
        {
            auto left=qRound(r.x()*factor);
            auto top=qRound(r.y()*factor);
            auto right=qRound((r.x()+r.width())*factor);
            auto bottom=qRound((r.y()+r.height())*factor);
            r=QRect(left,top,qMax(1,right-left),qMax(1,bottom-top));
        }
        totalW=0;
        totalH=0;
        for (const auto& r : rects)
        {
            totalW=qMax(totalW,r.x()+r.width());
            totalH=qMax(totalH,r.y()+r.height());
        }
    }

    // The uniform scale-down above is floor-blind -- it scales every rect by one factor, including
    // tiles the pass above put exactly on cappedFloor, which is how a correctly floored 100px
    // thumbnail used to come back out at 46px. Restore the floor for anything it pushed below, as
    // a grow-only pass through the same re-flow.
    //
    // Deliberately a SECOND pass rather than a clamp on the rescale factor itself: clamping the
    // factor would let one at-floor tile veto the whole shrink, leaving albums up to 2.2x taller
    // than the budget (measured on a real 8-image message). Shrinking everything first and then
    // growing only what fell through the floor lets the big tiles absorb the height and keeps the
    // album at its budget, while still guaranteeing the floor.
    //
    // The album can still end up somewhat taller than maxHeight (by whatever height the floor
    // forces back). That is the intended priority: maxHeight is a soft target -- nothing
    // downstream clips to it, ChatMessageImages::sizeHint()/minimumSizeHint() just report
    // whatever comes back here, and bubble-width negotiation only negotiates width -- whereas a
    // sub-floor tile is the defect the floor exists to prevent.
    if (options.devicePixelRatio>0)
    {
        std::vector<qreal> regrow(static_cast<size_t>(n),1.0);
        bool anyBelowFloor=false;

        for (int i=0;i<n;++i)
        {
            const auto& r=rects[static_cast<size_t>(i)];
            if (r.width()<=0 || r.height()<=0
                || (r.width()>=cappedFloor && r.height()>=cappedFloor))
            {
                continue;
            }
            auto f=qMax(static_cast<qreal>(cappedFloor)/r.width(),
                        static_cast<qreal>(cappedFloor)/r.height());
            regrow[static_cast<size_t>(i)]=qMax(1.0,qMin(f,static_cast<qreal>(w)/r.width()));
            anyBelowFloor=true;
        }

        if (anyBelowFloor)
        {
            reflowRows(regrow);

            totalW=0;
            totalH=0;
            for (const auto& r : rects)
            {
                totalW=qMax(totalW,r.x()+r.width());
                totalH=qMax(totalH,r.y()+r.height());
            }
        }
    }

    // Resolution enters this function ONLY through the per-tile natural-size cap above, never
    // through the templates: which template runs, and the proportions inside it, still come from
    // aspect ratios alone (confirmed requirement -- geometry must never depend on which
    // rung/preview happens to be locally available at render time, and pixelSizes is the
    // ORIGINAL's size from attachment metadata, not a rung's). A whole-album uniform shrink keyed
    // on the worst-fitting member was tried instead of the per-tile cap and reverted: it dragged
    // every other tile down whenever one small image shared an album with normal-sized photos.

    if (totalSize!=nullptr)
    {
        *totalSize=QSize(totalW,totalH);
    }

    return rects;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
