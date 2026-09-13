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

/** @file uise/desktop/src/flowpack.cpp
*
*  Defines flowPack().
*
*/

/****************************************************************************/

#include <algorithm>

#include <uise/desktop/utils/flowpack.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

//! One completed row's bookkeeping -- an index range into the shared rects vector, its own
//! (already finalized) height, and the y its items are centred against.
struct RowInfo
{
    size_t startIdx=0;
    size_t count=0;
    int y=0;
    int height=0;
};

//! Vertically centres every rect in [startIdx,startIdx+count) against a row of the given height
//! at the given y, then records the row. Uses QRect::moveTop(), which sets the top edge
//! absolutely -- safe to call more than once on the same range (see the reservedTailWidth
//! correction pass below, which re-finalizes a row after shrinking it).
void finalizeRow(std::vector<QRect>& rects, std::vector<RowInfo>& rows,
                  size_t startIdx, size_t count, int y, int height)
{
    for (size_t i=startIdx; i<startIdx+count; ++i)
    {
        auto& r=rects[i];
        r.moveTop(y+(height-r.height())/2);
    }
    rows.push_back(RowInfo{startIdx,count,y,height});
}

} // anonymous namespace

//--------------------------------------------------------------------------

FlowPackResult flowPack(const std::vector<QSize>& items, const FlowPackOptions& options)
{
    FlowPackResult result;

    // Resolve which items are actually placed -- truncating to maxItems-1 real items plus one
    // tail item when the input overflows the limit. maxItems<=0 means unlimited (no truncation).
    std::vector<QSize> packItems;
    if (options.maxItems>0 && items.size()>static_cast<size_t>(options.maxItems))
    {
        auto realCount=static_cast<size_t>(options.maxItems)-1;
        using DiffT=std::vector<QSize>::difference_type;
        packItems.assign(items.begin(),items.begin()+static_cast<DiffT>(realCount));
        int tailHeight=0;
        for (const auto& sz : packItems)
        {
            tailHeight=std::max(tailHeight,sz.height());
        }
        if (packItems.empty())
        {
            // No room for even one real item -- fall back to the tallest input item's height so
            // the lone tail chip is not given a degenerate zero height.
            for (const auto& sz : items)
            {
                tailHeight=std::max(tailHeight,sz.height());
            }
        }
        packItems.emplace_back(options.tailItemWidth,tailHeight);
        result.truncated=true;
    }
    else
    {
        packItems=items;
    }

    // Pass 1: greedy left-to-right wrap, row heights and vertical centring finalized as each row
    // closes (see finalizeRow()).
    std::vector<QRect> rects;
    std::vector<RowInfo> rows;
    rects.reserve(packItems.size());
    {
        int x=0;
        int y=0;
        int rowHeight=0;
        size_t rowStart=0;

        for (const auto& sz : packItems)
        {
            bool isFirstInRow=(rects.size()==rowStart);
            int itemX=isFirstInRow ? 0 : x+options.hSpacing;

            if (!isFirstInRow && options.maxWidth>0 && itemX+sz.width()>options.maxWidth)
            {
                finalizeRow(rects,rows,rowStart,rects.size()-rowStart,y,rowHeight);
                y+=rowHeight+options.vSpacing;
                rowStart=rects.size();
                itemX=0;
                rowHeight=0;
            }

            rects.emplace_back(itemX,y,sz.width(),sz.height());
            x=itemX+sz.width();
            rowHeight=std::max(rowHeight,sz.height());
        }

        if (rects.size()>rowStart)
        {
            finalizeRow(rects,rows,rowStart,rects.size()-rowStart,y,rowHeight);
        }
    }

    // Pass 2: reservedTailWidth correction. While the last row plus the reservation overflows
    // maxWidth and that row holds more than one item, peel its last item off into a new row of
    // its own -- the peeled item and the shrunk row stay index-contiguous in `rects`, so no data
    // needs to move, only RowInfo bookkeeping and the peeled rect's own geometry.
    bool tailFits=true;
    if (options.reservedTailWidth>0 && options.maxWidth>0)
    {
        if (rows.empty())
        {
            tailFits=(options.reservedTailWidth<=options.maxWidth);
        }
        else
        {
            while (true)
            {
                // Copied, not a reference -- the push_back a few lines below can reallocate
                // `rows`, and this value is still needed afterwards to compute the new row's y.
                RowInfo last=rows.back();

                const QRect& lastItemRect=rects[last.startIdx+last.count-1];
                int neededRight=lastItemRect.right()+1+options.hSpacing+options.reservedTailWidth;
                if (neededRight<=options.maxWidth)
                {
                    tailFits=true;
                    break;
                }
                if (last.count<=1)
                {
                    tailFits=false;
                    break;
                }

                auto movedIdx=last.startIdx+last.count-1;
                auto moved=rects[movedIdx];
                auto newCount=last.count-1;

                int newHeight=0;
                for (size_t i=last.startIdx; i<last.startIdx+newCount; ++i)
                {
                    newHeight=std::max(newHeight,rects[i].height());
                }
                // Re-centre the shrunk row's remaining items in place -- no row-vector mutation.
                for (size_t i=last.startIdx; i<last.startIdx+newCount; ++i)
                {
                    auto& r=rects[i];
                    r.moveTop(last.y+(newHeight-r.height())/2);
                }
                rows.back()=RowInfo{last.startIdx,newCount,last.y,newHeight};

                auto newRowY=last.y+newHeight+options.vSpacing;
                rects[movedIdx]=QRect(0,newRowY,moved.width(),moved.height());
                rows.push_back(RowInfo{movedIdx,1,newRowY,moved.height()});
            }
        }
    }

    // Assemble the result.
    result.rects=std::move(rects);
    result.placedCount=packItems.size();
    result.tailFits=tailFits;

    if (!rows.empty())
    {
        const auto& lastRow=rows.back();
        for (size_t i=lastRow.startIdx; i<lastRow.startIdx+lastRow.count; ++i)
        {
            const auto& r=result.rects[i];
            result.lastRowRect=result.lastRowRect.isNull() ? r : result.lastRowRect.united(r);
        }

        int maxRight=0;
        for (const auto& r : result.rects)
        {
            maxRight=std::max(maxRight,r.right()+1);
        }
        result.totalSize=QSize(maxRight,lastRow.y+lastRow.height);
    }

    return result;
}

UISE_DESKTOP_NAMESPACE_END
