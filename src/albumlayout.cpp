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

bool isWide(qreal a)
{
    return a>1.2;
}

qreal clampAspect(qreal a, qreal lo, qreal hi)
{
    return qBound(lo,a,hi);
}

/**
 * @brief Stack `count` tiles of the same width evenly across `totalHeight`.
 *
 * Proportions come from an even split of totalHeight/count, not from each tile's own aspect
 * ratio -- consistent with this file's crop-absorbs-the-mismatch design (see the header
 * comment on albumLayout()): the "N stacked" side of a big-tile template does not need to
 * solve for each image's exact aspect, since the tile is center-cropped to whatever rect it
 * gets anyway.
 */
std::vector<QRect> stackEven(int x, int y, int width, int totalHeight, int count, int spacing)
{
    std::vector<QRect> rects;
    if (count<=0)
    {
        return rects;
    }

    auto h=(totalHeight-spacing*(count-1))/count;
    auto yy=y;
    for (int i=0;i<count;++i)
    {
        rects.emplace_back(x,yy,width,h);
        yy+=h+spacing;
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
        if (isWide(a[0]) && isWide(a[1]))
        {
            // both wide -- two stacked full-width rows, each at its own natural height
            auto h0=qMax(options.minTile,qRound(w/a[0]));
            auto h1=qMax(options.minTile,qRound(w/a[1]));
            rects.emplace_back(0,0,w,h0);
            rects.emplace_back(0,h0+s,w,h1);
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
        if (isWide(a[0]))
        {
            // image 0 full width on top, images 1-2 in a proportional row below
            auto h0=qMax(options.minTile,qRound(w/clampAspect(a[0],0.5,2.5)));
            auto h1=qMax(options.minTile,qRound((w-s)/(a[1]+a[2])));
            auto w1=qRound(a[1]*h1);
            auto w2=(w-s)-w1;
            rects.emplace_back(0,0,w,h0);
            rects.emplace_back(0,h0+s,w1,h1);
            rects.emplace_back(w1+s,h0+s,w2,h1);
        }
        else
        {
            // image 0 at full height on the left, images 1-2 stacked on the right
            auto leftWidth=qRound((w-s)*2.0/3.0);
            auto rightWidth=(w-s)-leftWidth;
            auto leftHeight=qMax(options.minTile,qRound(leftWidth/clampAspect(a[0],0.6,1.6)));
            rects.emplace_back(0,0,leftWidth,leftHeight);
            auto rightRects=stackEven(leftWidth+s,0,rightWidth,leftHeight,2,s);
            rects.insert(rects.end(),rightRects.begin(),rightRects.end());
        }
    }
    else if (n==4)
    {
        if (isWide(a[0]))
        {
            // image 0 full width on top, images 1-3 in one proportional row below
            auto h0=qMax(options.minTile,qRound(w/clampAspect(a[0],0.5,2.5)));
            auto sumA=a[1]+a[2]+a[3];
            auto h1=qMax(options.minTile,qRound((w-2*s)/sumA));

            rects.emplace_back(0,0,w,h0);

            int x=0;
            for (int i=1;i<3;++i)
            {
                auto tileWidth=qRound(a[static_cast<size_t>(i)]*h1);
                rects.emplace_back(x,h0+s,tileWidth,h1);
                x+=tileWidth+s;
            }
            auto wLast=(w-2*s)-(rects[1].width()+rects[2].width());
            rects.emplace_back(x,h0+s,wLast,h1);
        }
        else
        {
            // image 0 at full height on the left, images 1-3 stacked on the right
            auto leftWidth=qRound((w-s)*0.55);
            auto rightWidth=(w-s)-leftWidth;
            auto leftHeight=qMax(options.minTile,qRound(leftWidth/clampAspect(a[0],0.6,1.6)));
            rects.emplace_back(0,0,leftWidth,leftHeight);
            auto rightRects=stackEven(leftWidth+s,0,rightWidth,leftHeight,3,s);
            rects.insert(rects.end(),rightRects.begin(),rightRects.end());
        }
    }
    else
    {
        // justified-rows fallback: greedily fill each row until it would reach/exceed w at the
        // target row height, then rescale that row's tiles to sum to exactly w -- a standard
        // justified-gallery packing, the only template here that genuinely scales to any count
        auto targetRowHeight=qBound(options.minTile,qRound(w/3.0),options.maxHeight/2);

        int i=0;
        int y=0;
        while (i<n)
        {
            auto rowStart=i;
            qreal sumA=0;
            while (i<n && (sumA+a[static_cast<size_t>(i)])*targetRowHeight<w)
            {
                sumA+=a[static_cast<size_t>(i)];
                ++i;
            }
            if (i==rowStart)
            {
                // a single image alone already reaches/exceeds w at the target height --
                // take it anyway so the loop always makes progress
                sumA+=a[static_cast<size_t>(i)];
                ++i;
            }

            auto rowCount=i-rowStart;
            auto rowHeight=qBound(options.minTile,qRound((w-s*(rowCount-1))/sumA),options.maxHeight);

            int x=0;
            for (int k=rowStart;k<i;++k)
            {
                int tileWidth;
                if (k==i-1)
                {
                    // last tile of the row absorbs rounding so the row sums exactly to w
                    tileWidth=w-x;
                }
                else
                {
                    tileWidth=qRound(a[static_cast<size_t>(k)]*rowHeight);
                }
                rects.emplace_back(x,y,tileWidth,rowHeight);
                x+=tileWidth+s;
            }

            y+=rowHeight+s;
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
    // clip -- keeps every tile's own proportions from its template intact
    if (totalH>options.maxHeight && totalH>0)
    {
        auto factor=static_cast<qreal>(options.maxHeight)/static_cast<qreal>(totalH);
        for (auto& r : rects)
        {
            r=QRect(qRound(r.x()*factor),qRound(r.y()*factor),qRound(r.width()*factor),qRound(r.height()*factor));
        }
        totalW=qRound(totalW*factor);
        totalH=options.maxHeight;
    }

    if (totalSize!=nullptr)
    {
        *totalSize=QSize(totalW,totalH);
    }

    return rects;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
