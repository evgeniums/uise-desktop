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

/** @file uise/desktop/src/albumlayoutpresets.cpp
*
*  Defines albumLayoutPresets() -- the AlbumLayoutMode::PresetTemplates algorithm.
*
*  Integer-only arithmetic throughout: ratios in thousandths, lengths in logical pixels, every
*  division through fdiv()/rdiv() with a fixed rounding direction, every length distribution
*  through share() whose last piece is the remainder. The same input therefore always produces
*  the same output, byte for byte, on every platform.
*
*/

/****************************************************************************/

#include <algorithm>
#include <cstdint>
#include <vector>

#include <QtGlobal>

#include <uise/desktop/utils/albumlayout.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

using Int=qint64;

constexpr Int Unit=1000;

//! floor(a/b) for b>0 (a may be negative)
Int fdiv(Int a, Int b)
{
    auto q=a/b;
    if ((a%b!=0) && ((a<0)!=(b<0)))
    {
        --q;
    }
    return q;
}

//! round-half-up(a/b) for b>0
Int rdiv(Int a, Int b)
{
    return fdiv(2*a+b,2*b);
}

enum class RatioClass
{
    Narrow,
    Square,
    Wide
};

struct Cell
{
    int id=0;
    Int x=0;
    Int y=0;
    Int w=0;
    Int h=0;
};

struct Block
{
    Int width=0;
    Int height=0;
    std::vector<Cell> cells;
    std::vector<Int> rowHeights;
    //! The left-column templates are not row-shaped (one cell spans every row), so the generic
    //! row squeeze must not touch them -- they bound their own height while being built.
    bool rowShaped=true;
};

/**
 * @brief Distribute `total` over `weights` proportionally, the last piece taking the remainder
 *  so the pieces sum to `total` identically.
 */
std::vector<Int> share(Int total, const std::vector<Int>& weights)
{
    std::vector<Int> out;
    out.reserve(weights.size());
    Int sumW=0;
    for (auto w : weights)
    {
        sumW+=w;
    }
    if (sumW<=0)
    {
        sumW=1;
    }
    Int acc=0;
    Int prev=0;
    for (size_t j=0;j<weights.size();++j)
    {
        acc+=weights[j];
        auto cur=(j+1==weights.size()) ? total : fdiv(total*acc,sumW);
        out.push_back(cur-prev);
        prev=cur;
    }
    return out;
}

class Presets
{
    public:

        Presets(const std::vector<QSize>& pixelSizes, const AlbumLayoutOptions& options, Int width)
            : m_cfg(options.presets),
              m_n(static_cast<int>(pixelSizes.size())),
              m_w(width),
              m_hMax(std::max<Int>(1,options.maxHeight)),
              m_s(std::max(0,options.spacing))
        {
            m_rho.reserve(pixelSizes.size());
            m_r.reserve(pixelSizes.size());
            m_class.reserve(pixelSizes.size());
            for (const auto& sz : pixelSizes)
            {
                Int rho=(sz.width()<=0 || sz.height()<=0) ? Unit : rdiv(Unit*sz.width(),sz.height());
                m_rho.push_back(rho);
                const Int minRatio=m_cfg.minRatio;
                const Int maxRatio=std::max<Int>(minRatio,m_cfg.maxRatio);
                m_r.push_back(std::clamp(rho,minRatio,maxRatio));
                if (rho<=m_cfg.narrowThreshold)
                {
                    m_class.push_back(RatioClass::Narrow);
                }
                else if (rho>=m_cfg.wideThreshold)
                {
                    m_class.push_back(RatioClass::Wide);
                }
                else
                {
                    m_class.push_back(RatioClass::Square);
                }
            }
        }

        Block run() const;

    private:

        void rowCells(const std::vector<int>& idx, Int x0, Int y0, Int width, Block& block, Int& rowHeight) const;
        Block buildRows(const std::vector<int>& composition) const;
        Block columnLeft() const;
        Block topPlusRow() const;
        bool templateFor(Block& out) const;
        bool valid(const Block& block) const;
        void compositions(std::vector<std::vector<int>>& out) const;
        Int penalty(const Block& block) const;
        Int cropLoss(Int rho, Int w, Int h) const;
        void squeeze(Block& block) const;

        const AlbumPresetConfig& m_cfg;
        int m_n;
        Int m_w;
        Int m_hMax;
        Int m_s;
        std::vector<Int> m_rho;
        std::vector<Int> m_r;
        std::vector<RatioClass> m_class;
};

//--------------------------------------------------------------------------

void Presets::rowCells(const std::vector<int>& idx, Int x0, Int y0, Int width, Block& block, Int& rowHeight) const
{
    const auto m=static_cast<Int>(idx.size());
    auto avail=std::max<Int>(1,width-(m-1)*m_s);
    std::vector<Int> weights;
    weights.reserve(idx.size());
    Int sumR=0;
    for (auto i : idx)
    {
        weights.push_back(m_r[static_cast<size_t>(i)]);
        sumR+=m_r[static_cast<size_t>(i)];
    }
    rowHeight=std::max<Int>(1,fdiv(avail*Unit,sumR));
    auto widths=share(avail,weights);
    Int x=x0;
    for (size_t j=0;j<idx.size();++j)
    {
        block.cells.push_back(Cell{idx[j],x,y0,std::max<Int>(1,widths[j]),rowHeight});
        x+=widths[j]+m_s;
    }
}

//--------------------------------------------------------------------------

Block Presets::buildRows(const std::vector<int>& composition) const
{
    Block block;
    block.width=m_w;
    Int y=0;
    int i=0;
    for (auto count : composition)
    {
        std::vector<int> idx;
        for (int k=0;k<count;++k)
        {
            idx.push_back(i++);
        }
        Int h=0;
        rowCells(idx,0,y,m_w,block,h);
        block.rowHeights.push_back(h);
        y+=h+m_s;
    }
    block.height=y-m_s;
    return block;
}

//--------------------------------------------------------------------------

Block Presets::topPlusRow() const
{
    // first image full width on top, the rest in one proportional row below
    Block block;
    block.width=m_w;
    auto h0=std::max<Int>(1,fdiv(m_w*Unit,m_r[0]));
    block.cells.push_back(Cell{0,0,0,m_w,h0});
    block.rowHeights.push_back(h0);
    std::vector<int> idx;
    for (int i=1;i<m_n;++i)
    {
        idx.push_back(i);
    }
    Int h1=0;
    rowCells(idx,0,h0+m_s,m_w,block,h1);
    block.rowHeights.push_back(h1);
    block.height=h0+m_s+h1;
    return block;
}

//--------------------------------------------------------------------------

Block Presets::columnLeft() const
{
    // first image as a full-height column on the left, the other m stacked on the right at a
    // shared width. Solving for the right column's width wR so both sides reach the same height:
    //   H = wL/r0 = wR*q + (m-1)*s,  wL = (W-s) - wR,  q = sum(1/r_j)
    //   => wR = ((W-s) - r0*(m-1)*s) / (1 + r0*q)
    // in thousandths (r0 and q are both thousandths, so r0*q is millionths).
    const auto m=static_cast<Int>(m_n-1);
    std::vector<Int> invR;
    invR.reserve(static_cast<size_t>(m));
    Int q=0;
    for (int j=1;j<m_n;++j)
    {
        auto inv=fdiv(Unit*Unit,m_r[static_cast<size_t>(j)]);
        invR.push_back(inv);
        q+=inv;
    }
    auto num=(m_w-m_s)*Unit*Unit-m_r[0]*(m-1)*m_s*Unit;
    auto den=Unit*Unit+m_r[0]*q;
    auto wR=std::max<Int>(1,fdiv(num,den));
    auto hR=std::max<Int>(1,fdiv(wR*q,Unit));
    auto height=hR+(m-1)*m_s;
    if (height>m_hMax)
    {
        // the one template whose height the generic squeeze cannot fix (the column spans every
        // row): bound it here instead -- keep the same width split rule, shorter stack, more crop
        hR=std::max<Int>(1,m_hMax-(m-1)*m_s);
        height=hR+(m-1)*m_s;
        wR=std::max<Int>(1,fdiv(hR*Unit,std::max<Int>(1,q)));
        wR=std::min(wR,m_w-m_s-1);
    }
    auto wL=std::max<Int>(1,(m_w-m_s)-wR);

    Block block;
    block.width=m_w;
    block.height=height;
    block.rowShaped=false;
    block.rowHeights.push_back(height);
    block.cells.push_back(Cell{0,0,0,wL,height});
    auto heights=share(hR,invR);
    Int y=0;
    for (int j=1;j<m_n;++j)
    {
        auto h=std::max<Int>(1,heights[static_cast<size_t>(j-1)]);
        block.cells.push_back(Cell{j,wL+m_s,y,wR,h});
        y+=h+m_s;
    }
    return block;
}

//--------------------------------------------------------------------------

bool Presets::templateFor(Block& out) const
{
    using C=RatioClass;
    const auto& r=m_r;
    const auto& cls=m_class;

    if (m_n==1)
    {
        // fit by the clamped ratio: full width unless the height budget binds
        auto h=fdiv(m_w*Unit,r[0]);
        Int w=m_w;
        if (h>m_hMax)
        {
            h=m_hMax;
            w=std::max<Int>(1,fdiv(h*r[0],Unit));
        }
        h=std::max<Int>(1,h);
        out=Block{};
        out.width=w;
        out.height=h;
        out.rowHeights.push_back(h);
        out.cells.push_back(Cell{0,0,0,w,h});
        return true;
    }

    if (m_n==2)
    {
        auto similar=qAbs(r[0]-r[1])<=m_cfg.similarThreshold;
        if (cls[0]==C::Wide && cls[1]==C::Wide && fdiv(r[0]+r[1],2)>=m_cfg.stackAverage && similar)
        {
            // two similar landscapes -- stacked, each full width
            out=buildRows({1,1});
            return true;
        }
        if (cls[0]==cls[1] && similar)
        {
            // same class, similar -- an exact half each, the ratio difference goes to the crop
            auto u=fdiv(m_w-m_s,2);
            auto v=(m_w-m_s)-u;
            auto h=std::max<Int>(1,std::min(fdiv(u*Unit,r[0]),fdiv(v*Unit,r[1])));
            out=Block{};
            out.width=m_w;
            out.height=h;
            out.rowHeights.push_back(h);
            out.cells.push_back(Cell{0,0,0,std::max<Int>(1,u),h});
            out.cells.push_back(Cell{1,u+m_s,0,std::max<Int>(1,v),h});
            return true;
        }
        out=buildRows({2});
        return true;
    }

    if (m_n==3)
    {
        if (cls[0]==C::Narrow && cls[1]==C::Narrow && cls[2]==C::Narrow)
        {
            out=buildRows({3});
        }
        else if (cls[0]==C::Wide)
        {
            out=topPlusRow();
        }
        else
        {
            out=columnLeft();
        }
        return true;
    }

    if (m_n==4)
    {
        if (cls[0]==C::Wide)
        {
            out=topPlusRow();
        }
        else if (cls[0]==C::Narrow)
        {
            out=columnLeft();
        }
        else
        {
            // 2x2 grid with one shared vertical seam: the column split follows the summed
            // ratios of each column, each row's height is the smaller of what its two cells want
            auto avail=m_w-m_s;
            auto u=std::max<Int>(1,fdiv(avail*(r[0]+r[2]),r[0]+r[1]+r[2]+r[3]));
            auto v=std::max<Int>(1,avail-u);
            auto hT=std::max<Int>(1,std::min(fdiv(u*Unit,r[0]),fdiv(v*Unit,r[1])));
            auto hB=std::max<Int>(1,std::min(fdiv(u*Unit,r[2]),fdiv(v*Unit,r[3])));
            out=Block{};
            out.width=m_w;
            out.height=hT+m_s+hB;
            out.rowHeights={hT,hB};
            out.cells.push_back(Cell{0,0,0,u,hT});
            out.cells.push_back(Cell{1,u+m_s,0,v,hT});
            out.cells.push_back(Cell{2,0,hT+m_s,u,hB});
            out.cells.push_back(Cell{3,u+m_s,hT+m_s,v,hB});
        }
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------

bool Presets::valid(const Block& block) const
{
    // Minimum cell size only. Height is deliberately NOT checked here: the template table is a
    // preference, and a template that merely runs taller than the budget is squeezed to fit
    // (see squeeze()) rather than thrown away for a composition that may look worse.
    auto minW=fdiv(m_w*m_cfg.minCellWidth,Unit);
    auto minH=fdiv(m_w*m_cfg.minCellHeight,Unit);
    for (const auto& c : block.cells)
    {
        if (c.w<minW || c.h<minH)
        {
            return false;
        }
    }
    return true;
}

//--------------------------------------------------------------------------

void Presets::compositions(std::vector<std::vector<int>>& out) const
{
    // every way of splitting m_n images into at most maxRows rows of at most maxPerRow cells,
    // in lexicographic order (which is also the tie-break order of the search)
    std::vector<int> acc;
    const int maxRows=std::max(1,m_cfg.maxRows);
    const int maxPerRow=std::max(1,m_cfg.maxPerRow);
    struct Recurse
    {
        int maxRows;
        int maxPerRow;
        std::vector<std::vector<int>>& out;
        void operator()(int rest, int depth, std::vector<int>& acc) const
        {
            if (rest==0)
            {
                out.push_back(acc);
                return;
            }
            if (depth==maxRows)
            {
                return;
            }
            if (static_cast<qint64>(rest)>static_cast<qint64>(maxRows-depth)*maxPerRow)
            {
                return;
            }
            for (int p=1;p<=std::min(maxPerRow,rest);++p)
            {
                acc.push_back(p);
                (*this)(rest-p,depth+1,acc);
                acc.pop_back();
            }
        }
    };
    Recurse{maxRows,maxPerRow,out}(m_n,0,acc);
}

//--------------------------------------------------------------------------

Int Presets::cropLoss(Int rho, Int w, Int h) const
{
    auto a=(h>0) ? rdiv(Unit*w,h) : Unit;
    auto lo=std::max<Int>(1,std::min(a,rho));
    auto hi=std::max<Int>(1,std::max(a,rho));
    return Unit-fdiv(Unit*lo,hi);
}

//--------------------------------------------------------------------------

Int Presets::penalty(const Block& block) const
{
    // all terms in thousandths; weights as configured
    auto a=(block.height>0) ? rdiv(Unit*block.width,block.height) : Unit;
    const Int target=std::max<Int>(1,m_cfg.targetAspect);
    auto pAspect=fdiv(Unit*std::max(a,target),std::max<Int>(1,std::min(a,target)))-Unit;

    Int minRow=block.rowHeights.front();
    Int maxRow=minRow;
    Int sumRows=0;
    for (auto h : block.rowHeights)
    {
        minRow=std::min(minRow,h);
        maxRow=std::max(maxRow,h);
        sumRows+=h;
    }
    auto avgRow=std::max<Int>(1,sumRows/static_cast<Int>(block.rowHeights.size()));
    auto pEven=fdiv(Unit*(maxRow-minRow),avgRow);

    Int cropSum=0;
    Int pMin=0;
    const auto minW=std::max<Int>(1,fdiv(m_w*m_cfg.minCellWidth,Unit));
    const auto minH=std::max<Int>(1,fdiv(m_w*m_cfg.minCellHeight,Unit));
    for (const auto& c : block.cells)
    {
        cropSum+=cropLoss(m_rho[static_cast<size_t>(c.id)],c.w,c.h);
        pMin+=fdiv(Unit*std::max<Int>(0,minW-c.w),minW)+fdiv(Unit*std::max<Int>(0,minH-c.h),minH);
    }
    auto pCrop=cropSum/static_cast<Int>(block.cells.size());
    auto pOver=fdiv(Unit*std::max<Int>(0,block.height-m_hMax),m_hMax);

    return m_cfg.weightAspect*pAspect
         + m_cfg.weightEven*pEven
         + m_cfg.weightCrop*pCrop
         + m_cfg.weightMinCell*pMin
         + m_cfg.weightOver*pOver;
}

//--------------------------------------------------------------------------

void Presets::squeeze(Block& block) const
{
    // Row-shaped blocks only (see Block::rowShaped): redistribute the height budget over the
    // rows in proportion to their current heights; widths are untouched, so the fit grows crop.
    if (!block.rowShaped || block.height<=m_hMax)
    {
        return;
    }

    std::vector<Int> ys;
    for (const auto& c : block.cells)
    {
        if (std::find(ys.begin(),ys.end(),c.y)==ys.end())
        {
            ys.push_back(c.y);
        }
    }
    std::sort(ys.begin(),ys.end());

    std::vector<Int> rowHeights;
    for (auto y : ys)
    {
        Int h=0;
        for (const auto& c : block.cells)
        {
            if (c.y==y)
            {
                h=std::max(h,c.h);
            }
        }
        rowHeights.push_back(h);
    }

    auto avail=std::max<Int>(static_cast<Int>(ys.size()),m_hMax-(static_cast<Int>(ys.size())-1)*m_s);
    auto newHeights=share(avail,rowHeights);

    std::vector<Int> newY(ys.size());
    Int y=0;
    for (size_t i=0;i<ys.size();++i)
    {
        newHeights[i]=std::max<Int>(1,newHeights[i]);
        newY[i]=y;
        y+=newHeights[i]+m_s;
    }
    for (auto& c : block.cells)
    {
        auto row=static_cast<size_t>(std::find(ys.begin(),ys.end(),c.y)-ys.begin());
        c.y=newY[row];
        c.h=newHeights[row];
    }
    block.rowHeights=newHeights;
    block.height=y-m_s;
}

//--------------------------------------------------------------------------

Block Presets::run() const
{
    Block best;
    bool haveBest=false;

    if (m_n<=4)
    {
        Block t;
        if (templateFor(t) && valid(t))
        {
            best=std::move(t);
            haveBest=true;
        }
    }

    if (!haveBest)
    {
        std::vector<std::vector<int>> comps;
        compositions(comps);
        Int bestPenalty=0;
        for (const auto& comp : comps)
        {
            auto block=buildRows(comp);
            auto p=penalty(block);
            // strict "<": a tie keeps the lexicographically smaller composition
            if (!haveBest || p<bestPenalty)
            {
                bestPenalty=p;
                best=std::move(block);
                haveBest=true;
            }
        }
    }

    if (!haveBest)
    {
        // more images than maxRows x maxPerRow can hold: rows of maxPerRow, squeezed below
        std::vector<int> comp;
        int rest=m_n;
        while (rest>0)
        {
            auto p=std::min(std::max(1,m_cfg.maxPerRow),rest);
            comp.push_back(p);
            rest-=p;
        }
        best=buildRows(comp);
    }

    squeeze(best);
    return best;
}

}

//--------------------------------------------------------------------------

std::vector<QRect> albumLayoutPresets(
        const std::vector<QSize>& pixelSizes,
        const AlbumLayoutOptions& options,
        QSize* totalSize
    )
{
    std::vector<QRect> rects;
    const auto n=static_cast<int>(pixelSizes.size());
    if (n==0)
    {
        if (totalSize!=nullptr)
        {
            *totalSize=QSize(0,0);
        }
        return rects;
    }

    const Int width=std::max(1,options.maxWidth);
    Block block=Presets(pixelSizes,options,width).run();

    // Same rule as albumLayout()'s uniform all-thumbnail shrink: if every image with a known
    // size is smaller than its cell, lay the album out again at a proportionally smaller width
    // budget (integer geometry cannot simply be scaled without disturbing the seams), bounded by
    // the shrink floor. One pass: the smaller budget may pick a different template, which is
    // accepted as is.
    if (options.devicePixelRatio>0)
    {
        double f=0;
        bool anyKnown=false;
        for (const auto& c : block.cells)
        {
            const auto& sz=pixelSizes[static_cast<size_t>(c.id)];
            if (sz.width()<=0 || sz.height()<=0)
            {
                continue;
            }
            auto naturalW=sz.width()/options.devicePixelRatio;
            f=std::max(f,naturalW/static_cast<double>(std::max<Int>(1,c.w)));
            anyKnown=true;
        }
        if (anyKnown && f<1.0)
        {
            const auto floorPx=static_cast<double>((options.shrinkFloor>0) ? options.shrinkFloor : options.minTile);
            double fMin=0;
            for (const auto& c : block.cells)
            {
                fMin=std::max(fMin,floorPx/static_cast<double>(std::max<Int>(1,std::min(c.w,c.h))));
            }
            f=std::min(1.0,std::max(f,fMin));
            if (f<1.0)
            {
                AlbumLayoutOptions shrunk=options;
                shrunk.maxWidth=std::max(1,qRound(width*f));
                shrunk.maxHeight=std::max(1,qRound(options.maxHeight*f));
                block=Presets(pixelSizes,shrunk,shrunk.maxWidth).run();
            }
        }
    }

    rects.assign(static_cast<size_t>(n),QRect());
    int totalW=0;
    int totalH=0;
    for (const auto& c : block.cells)
    {
        QRect r(static_cast<int>(c.x),static_cast<int>(c.y),static_cast<int>(c.w),static_cast<int>(c.h));
        rects[static_cast<size_t>(c.id)]=r;
        totalW=std::max(totalW,r.x()+r.width());
        totalH=std::max(totalH,r.y()+r.height());
    }
    if (totalSize!=nullptr)
    {
        *totalSize=QSize(totalW,totalH);
    }
    return rects;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
