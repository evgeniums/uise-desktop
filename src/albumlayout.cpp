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
#include <cmath>
#include <cstdint>
#include <numeric>

#include <QtGlobal>

#include <uise/desktop/utils/albumlayout.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

double aspectOf(const QSize& sz)
{
    if (sz.width()<=0 || sz.height()<=0)
    {
        return 1.0;
    }
    return static_cast<double>(sz.width())/static_cast<double>(sz.height());
}

/********************** Cost weights **********************/

// All cost terms are dimensionless (differences of natural logarithms), so the weights below are
// comparable with each other: a weight of 1 on a term means "a factor of e in that term costs as
// much as a factor of e in tile-area imbalance". Tuned against demo/chatmessagefiles; the tests
// assert invariants rather than exact rects so these can be adjusted without churning them.

//! Variance of log(tile area / target area) across the album's tiles -- see targetAreas().
constexpr double WeightBalance=0.7;
//! Share of tiles nested two or more orientation changes deep -- a weak preference for plain
//! rows/columns (and one level of hero + stack / grid) over fiddly nested structures.
constexpr double WeightNesting=0.3;
//! log(maxWidth / album width): the album should use the whole width budget.
constexpr double WeightWidthShortfall=3.0;
//! log(box area / album area): among width-filling structures, prefer the one with the larger
//! tiles (a 2-row block over a 1-row strip of tiny tiles).
constexpr double WeightAreaUse=0.6;
//! Mean over known-size images of log(tile width / natural width) when the tile is larger than
//! the image -- a root-level tie-breaker in favour of upscaling less.
constexpr double WeightUpscale=0.5;
//! Sum over tiles of log(minTile / short side) for tiles squeezed under minTile.
constexpr double WeightUnderMin=4.0;

/********************** Search bounds **********************/

//! Candidates for one sub-sequence are deduplicated by aspect ratio in buckets this wide (in
//! log units, ~6%), keeping the best partial cost per bucket, before the per-interval cap.
constexpr double AspectBucketWidth=0.06;
constexpr int MinCandidatesPerInterval=4;
constexpr int MaxCandidatesPerInterval=32;
//! Total (candidate x candidate x orientation) combinations the DP is allowed, which sets the
//! per-interval cap from the image count: 32 for up to 5 images, 16 at 8, 12 at 10, 4 from ~20.
constexpr double CombinationBudget=60000.0;

enum class Orient : uint8_t
{
    Leaf,
    SideBySide, //!< left | right, shared height
    Stacked     //!< top / bottom, shared width
};

/**
 * @brief One candidate structure for a contiguous run of images.
 *
 * Every node of a split tree satisfies W = alpha*H + beta in logical px, with beta carrying the
 * spacing seams exactly: a leaf is (aspect, 0); side by side adds alphas and betas plus one
 * seam; stacked combines reciprocals (see combine()). Because the relation is affine, nesting
 * several same-orientation binary splits reproduces a k-ary row/column exactly, so binary trees
 * lose nothing.
 *
 * The balance statistics (s1, s2) are sums of d_k = log(area_k / target_k) over the subtree's
 * leaves with the subtree normalised to height 1 -- a stacked parent rescales its children, which
 * shifts every d_k in that child by one constant (see combine()), so the sums can be maintained
 * incrementally and the variance is available at every node without visiting its leaves.
 * Spacing is ignored in these statistics only (a <=2% effect on relative areas); the geometry
 * itself is exact.
 */
struct Cand
{
    double alpha=1.0;
    double beta=0.0;
    double logAlpha=0.0;

    double s1=0.0;
    double s2=0.0;
    int m=1;        //!< leaf count

    // Leaves by nesting level relative to this subtree's root, where a level is one change of
    // orientation on the path down: a plain row is all level 0, a hero beside a stacked pair has
    // the pair at level 1. nestP = sum over leaves of max(0, level-1).
    int cnt0=1;
    int cnt1=0;
    int cntDeep=0;
    int nestP=0;

    double partial=0.0;

    Orient orient=Orient::Leaf;
    int split=-1;   //!< last image index of the left/top child
    int left=-1;    //!< candidate index within the left/top child's interval
    int right=-1;   //!< candidate index within the right/bottom child's interval
};

struct DRect
{
    double x=0;
    double y=0;
    double w=0;
    double h=0;
};

int candidateLimit(int n)
{
    // number of (i,k,j) split triples the DP visits is C(n+1,3); each costs 2*K*K combinations
    auto triples=static_cast<double>(n)*(n+1.0)*(n-1.0)/6.0;
    if (triples<=0)
    {
        return MaxCandidatesPerInterval;
    }
    auto k=static_cast<int>(std::floor(std::sqrt(CombinationBudget/(2.0*triples))));
    return qBound(MinCandidatesPerInterval,k,MaxCandidatesPerInterval);
}

/**
 * @brief Per-image target area (logical px^2) the balance term measures each tile against.
 *
 * A photograph -- anything whose natural logical area is at least an equal share of the box --
 * targets exactly that equal share, so photographs want equal tiles. A genuinely small image
 * targets its own natural area (floored at a minTile square), i.e. a proportionally smaller
 * tile, which is what steers it into the small slot of a structure instead of a photo-sized one.
 * Unknown sizes behave as photographs.
 */
std::vector<double> targetAreas(const std::vector<QSize>& pixelSizes, const AlbumLayoutOptions& options)
{
    const auto n=static_cast<int>(pixelSizes.size());
    const double refArea=std::max(1.0,static_cast<double>(options.maxWidth)*options.maxHeight/std::max(n,2));
    const double floorArea=static_cast<double>(options.minTile)*options.minTile;

    std::vector<double> targets(static_cast<size_t>(n),refArea);
    if (options.devicePixelRatio<=0)
    {
        return targets;
    }
    for (int i=0;i<n;++i)
    {
        const auto& sz=pixelSizes[static_cast<size_t>(i)];
        if (sz.width()<=0 || sz.height()<=0)
        {
            continue;
        }
        auto natural=(sz.width()/options.devicePixelRatio)*(sz.height()/options.devicePixelRatio);
        targets[static_cast<size_t>(i)]=std::min(refArea,std::max(floorArea,natural));
    }
    return targets;
}

class Layout
{
    public:

        Layout(const std::vector<QSize>& pixelSizes, const AlbumLayoutOptions& options)
            : m_sizes(pixelSizes),
              m_options(options),
              m_n(static_cast<int>(pixelSizes.size())),
              m_s(static_cast<double>(options.spacing)),
              m_table(static_cast<size_t>(m_n)*static_cast<size_t>(m_n))
        {}

        void build();

        //! Root candidate with the lowest total cost once fitted into the box, and its tiles.
        void chooseRoot(int& rootIndex, int& width, int& height, std::vector<DRect>& tiles) const;

        //! Uniform all-thumbnail shrink -- see AlbumLayoutOptions::shrinkFloor.
        void shrink(int rootIndex, int& width, int& height, std::vector<DRect>& tiles) const;

        const std::vector<Cand>& at(int i, int j) const
        {
            return m_table[static_cast<size_t>(i)*static_cast<size_t>(m_n)+static_cast<size_t>(j)];
        }

    private:

        std::vector<Cand>& at(int i, int j)
        {
            return m_table[static_cast<size_t>(i)*static_cast<size_t>(m_n)+static_cast<size_t>(j)];
        }

        Cand combine(const Cand& left, const Cand& right, Orient orient, int split, int leftIndex, int rightIndex) const;

        static void accumulateNesting(Cand& parent, const Cand& child);

        void prune(std::vector<Cand>& all, std::vector<Cand>& out) const;

        void fitRoot(const Cand& root, int& width, int& height) const;

        void place(int i, int j, int index, double x, double y, double w, double h, std::vector<DRect>& out) const;

        double rootCost(const Cand& root, int width, int height, const std::vector<DRect>& tiles) const;

        const std::vector<QSize>& m_sizes;
        const AlbumLayoutOptions& m_options;
        int m_n;
        double m_s;
        int m_limit=MaxCandidatesPerInterval;
        std::vector<double> m_targets;
        std::vector<std::vector<Cand>> m_table;
};

//--------------------------------------------------------------------------

void Layout::accumulateNesting(Cand& parent, const Cand& child)
{
    int c0=child.cnt0;
    int c1=child.cnt1;
    int cd=child.cntDeep;
    int np=child.nestP;
    if (child.orient!=Orient::Leaf && child.orient!=parent.orient)
    {
        // every leaf in this child moves one level deeper
        np+=c1+cd;
        cd+=c1;
        c1=c0;
        c0=0;
    }
    parent.cnt0+=c0;
    parent.cnt1+=c1;
    parent.cntDeep+=cd;
    parent.nestP+=np;
}

//--------------------------------------------------------------------------

Cand Layout::combine(const Cand& left, const Cand& right, Orient orient, int split, int leftIndex, int rightIndex) const
{
    Cand c;
    c.orient=orient;
    c.split=split;
    c.left=leftIndex;
    c.right=rightIndex;
    c.m=left.m+right.m;
    c.cnt0=0;
    c.cnt1=0;
    c.cntDeep=0;
    c.nestP=0;

    if (orient==Orient::SideBySide)
    {
        // shared height: widths add, plus one seam
        c.alpha=left.alpha+right.alpha;
        c.beta=left.beta+right.beta+m_s;
        c.logAlpha=std::log(c.alpha);
        // children keep the parent's height, so their per-leaf statistics are unchanged
        c.s1=left.s1+right.s1;
        c.s2=left.s2+right.s2;
    }
    else
    {
        // shared width W: H = (W-bT)/aT + (W-bB)/aB + s, solved for W = alpha*H + beta
        c.alpha=1.0/(1.0/left.alpha+1.0/right.alpha);
        c.beta=c.alpha*(left.beta/left.alpha+right.beta/right.alpha-m_s);
        c.logAlpha=std::log(c.alpha);
        // at parent height 1 a child's height is alpha/alpha_child, which scales each of its leaf
        // areas by that squared -- a constant shift of every d_k in the child
        c.s1=0;
        c.s2=0;
        for (const Cand* child : {&left,&right})
        {
            auto delta=2.0*(c.logAlpha-child->logAlpha);
            c.s1+=child->s1+child->m*delta;
            c.s2+=child->s2+2.0*delta*child->s1+child->m*delta*delta;
        }
    }

    accumulateNesting(c,left);
    accumulateNesting(c,right);

    auto mean=c.s1/c.m;
    auto variance=std::max(0.0,c.s2/c.m-mean*mean);
    c.partial=WeightBalance*variance+WeightNesting*static_cast<double>(c.nestP)/m_n;
    return c;
}

//--------------------------------------------------------------------------

void Layout::prune(std::vector<Cand>& all, std::vector<Cand>& out) const
{
    out.clear();
    if (all.empty())
    {
        return;
    }

    // Keep the best partial cost per aspect bucket -- the parent needs a CHOICE of shapes for
    // this run of images far more than it needs several near-identical shapes -- then the best
    // m_limit overall. Stable sorts and strict comparisons throughout, so the result is a pure
    // function of the input order (determinism is part of albumLayout()'s contract).
    std::vector<int> order(all.size());
    std::iota(order.begin(),order.end(),0);
    auto bucketOf=[&all](int idx)
    {
        return qRound(all[static_cast<size_t>(idx)].logAlpha/AspectBucketWidth);
    };
    std::stable_sort(order.begin(),order.end(),
        [&all,&bucketOf](int lhs, int rhs)
        {
            auto bl=bucketOf(lhs);
            auto br=bucketOf(rhs);
            if (bl!=br)
            {
                return bl<br;
            }
            return all[static_cast<size_t>(lhs)].partial<all[static_cast<size_t>(rhs)].partial;
        }
    );

    std::vector<int> picked;
    picked.reserve(order.size());
    int lastBucket=0;
    bool haveLast=false;
    for (auto idx : order)
    {
        auto b=bucketOf(idx);
        if (!haveLast || b!=lastBucket)
        {
            picked.push_back(idx);
            lastBucket=b;
            haveLast=true;
        }
    }

    std::stable_sort(picked.begin(),picked.end(),
        [&all](int lhs, int rhs)
        {
            return all[static_cast<size_t>(lhs)].partial<all[static_cast<size_t>(rhs)].partial;
        }
    );
    if (static_cast<int>(picked.size())>m_limit)
    {
        picked.resize(static_cast<size_t>(m_limit));
    }

    out.reserve(picked.size());
    for (auto idx : picked)
    {
        out.push_back(all[static_cast<size_t>(idx)]);
    }
}

//--------------------------------------------------------------------------

void Layout::build()
{
    m_limit=candidateLimit(m_n);
    m_targets=targetAreas(m_sizes,m_options);

    for (int i=0;i<m_n;++i)
    {
        Cand leaf;
        leaf.alpha=aspectOf(m_sizes[static_cast<size_t>(i)]);
        leaf.beta=0.0;
        leaf.logAlpha=std::log(leaf.alpha);
        // area of a leaf at height 1 is its aspect
        auto d=std::log(leaf.alpha/m_targets[static_cast<size_t>(i)]);
        leaf.s1=d;
        leaf.s2=d*d;
        at(i,i).push_back(leaf);
    }

    std::vector<Cand> all;
    for (int len=2;len<=m_n;++len)
    {
        for (int i=0;i+len-1<m_n;++i)
        {
            auto j=i+len-1;
            all.clear();
            for (int k=i;k<j;++k)
            {
                const auto& lefts=at(i,k);
                const auto& rights=at(k+1,j);
                all.reserve(all.size()+2*lefts.size()*rights.size());
                for (size_t li=0;li<lefts.size();++li)
                {
                    for (size_t ri=0;ri<rights.size();++ri)
                    {
                        all.push_back(combine(lefts[li],rights[ri],Orient::SideBySide,k,static_cast<int>(li),static_cast<int>(ri)));
                        all.push_back(combine(lefts[li],rights[ri],Orient::Stacked,k,static_cast<int>(li),static_cast<int>(ri)));
                    }
                }
            }
            prune(all,at(i,j));
        }
    }
}

//--------------------------------------------------------------------------

void Layout::fitRoot(const Cand& root, int& width, int& height) const
{
    // as wide as the box allows, unless the height budget binds first
    double hReal=static_cast<double>(m_options.maxHeight);
    if (root.alpha>0)
    {
        hReal=std::min(hReal,(static_cast<double>(m_options.maxWidth)-root.beta)/root.alpha);
    }
    hReal=std::max(hReal,1.0);
    auto wReal=root.alpha*hReal+root.beta;
    width=std::max(1,qRound(wReal));
    height=std::max(1,qRound(hReal));
}

//--------------------------------------------------------------------------

void Layout::place(int i, int j, int index, double x, double y, double w, double h, std::vector<DRect>& out) const
{
    const auto& c=at(i,j)[static_cast<size_t>(index)];
    if (c.orient==Orient::Leaf)
    {
        out[static_cast<size_t>(i)]=DRect{x,y,w,h};
        return;
    }

    const auto k=c.split;
    const auto& left=at(i,k)[static_cast<size_t>(c.left)];
    const auto& right=at(k+1,j)[static_cast<size_t>(c.right)];

    // The node's own (w,h) came from an integer-rounded parent, so alpha*h+beta misses w by up to
    // half a pixel; distributing the available extent in proportion to what each child asks for
    // spreads that error instead of dumping it onto one child.
    if (c.orient==Orient::SideBySide)
    {
        auto wl=std::max(1e-6,left.alpha*h+left.beta);
        auto wr=std::max(1e-6,right.alpha*h+right.beta);
        auto avail=std::max(1e-6,w-m_s);
        auto wLeft=avail*wl/(wl+wr);
        auto wRight=avail-wLeft;
        place(i,k,c.left,x,y,wLeft,h,out);
        place(k+1,j,c.right,x+wLeft+m_s,y,wRight,h,out);
    }
    else
    {
        auto ht=std::max(1e-6,(w-left.beta)/left.alpha);
        auto hb=std::max(1e-6,(w-right.beta)/right.alpha);
        auto avail=std::max(1e-6,h-m_s);
        auto hTop=avail*ht/(ht+hb);
        auto hBottom=avail-hTop;
        place(i,k,c.left,x,y,w,hTop,out);
        place(k+1,j,c.right,x,y+hTop+m_s,w,hBottom,out);
    }
}

//--------------------------------------------------------------------------

double Layout::rootCost(const Cand& root, int width, int height, const std::vector<DRect>& tiles) const
{
    auto cost=root.partial;

    const auto maxW=static_cast<double>(m_options.maxWidth);
    const auto maxH=static_cast<double>(m_options.maxHeight);
    cost+=WeightWidthShortfall*std::max(0.0,std::log(maxW/width));
    cost+=WeightAreaUse*std::max(0.0,std::log((maxW*maxH)/(static_cast<double>(width)*height)));

    if (m_options.devicePixelRatio>0)
    {
        double upscale=0;
        int known=0;
        for (int i=0;i<m_n;++i)
        {
            const auto& sz=m_sizes[static_cast<size_t>(i)];
            if (sz.width()<=0 || sz.height()<=0)
            {
                continue;
            }
            auto naturalW=sz.width()/m_options.devicePixelRatio;
            upscale+=std::max(0.0,std::log(tiles[static_cast<size_t>(i)].w/naturalW));
            ++known;
        }
        if (known>0)
        {
            cost+=WeightUpscale*upscale/m_n;
        }
    }

    double underMin=0;
    const auto minTile=static_cast<double>(m_options.minTile);
    for (const auto& t : tiles)
    {
        auto shortSide=std::max(1e-6,std::min(t.w,t.h));
        if (shortSide<minTile)
        {
            underMin+=std::log(minTile/shortSide);
        }
    }
    cost+=WeightUnderMin*underMin;

    return cost;
}

//--------------------------------------------------------------------------

void Layout::chooseRoot(int& rootIndex, int& width, int& height, std::vector<DRect>& tiles) const
{
    const auto& roots=at(0,m_n-1);
    rootIndex=0;
    double best=0;
    std::vector<DRect> candidateTiles(static_cast<size_t>(m_n));
    for (size_t r=0;r<roots.size();++r)
    {
        int w=0;
        int h=0;
        fitRoot(roots[r],w,h);
        place(0,m_n-1,static_cast<int>(r),0.0,0.0,w,h,candidateTiles);
        auto cost=rootCost(roots[r],w,h,candidateTiles);
        if (r==0 || cost<best)
        {
            best=cost;
            rootIndex=static_cast<int>(r);
            width=w;
            height=h;
            tiles=candidateTiles;
        }
    }
}

//--------------------------------------------------------------------------

void Layout::shrink(int rootIndex, int& width, int& height, std::vector<DRect>& tiles) const
{
    if (m_options.devicePixelRatio<=0)
    {
        return;
    }

    // The least-upscaled known image decides: if even IT is shown larger than natural size, the
    // whole album is thumbnails and may come down to its natural scale. One image at or above
    // natural size (a normal photo) makes f>=1 and keeps everything as laid out.
    double f=0;
    bool anyKnown=false;
    for (int i=0;i<m_n;++i)
    {
        const auto& sz=m_sizes[static_cast<size_t>(i)];
        if (sz.width()<=0 || sz.height()<=0)
        {
            continue;
        }
        auto naturalW=sz.width()/m_options.devicePixelRatio;
        f=std::max(f,naturalW/std::max(1e-6,tiles[static_cast<size_t>(i)].w));
        anyKnown=true;
    }
    if (!anyKnown || f>=1.0)
    {
        return;
    }

    // ...but never below the floor on any tile's short side
    const auto floorPx=static_cast<double>((m_options.shrinkFloor>0) ? m_options.shrinkFloor : m_options.minTile);
    double fMin=0;
    for (const auto& t : tiles)
    {
        fMin=std::max(fMin,floorPx/std::max(1e-6,std::min(t.w,t.h)));
    }
    f=std::min(1.0,std::max(f,fMin));
    if (f>=1.0)
    {
        return;
    }

    const auto& root=at(0,m_n-1)[static_cast<size_t>(rootIndex)];
    height=std::max(1,qRound(f*height));
    width=std::max(1,qRound(root.alpha*height+root.beta));
    place(0,m_n-1,rootIndex,0.0,0.0,width,height,tiles);
}

}

//--------------------------------------------------------------------------

std::vector<QRect> albumLayout(
        const std::vector<QSize>& pixelSizes,
        const AlbumLayoutOptions& optionsIn,
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

    if (optionsIn.mode==AlbumLayoutMode::PresetTemplates)
    {
        return albumLayoutPresets(pixelSizes,optionsIn,totalSize);
    }

    AlbumLayoutOptions options=optionsIn;
    options.maxWidth=std::max(1,options.maxWidth);
    options.maxHeight=std::max(1,options.maxHeight);
    options.minTile=std::max(1,options.minTile);
    options.spacing=std::max(0,options.spacing);

    Layout layout(pixelSizes,options);
    layout.build();

    int rootIndex=0;
    int width=0;
    int height=0;
    std::vector<DRect> tiles(static_cast<size_t>(n));
    layout.chooseRoot(rootIndex,width,height,tiles);
    layout.shrink(rootIndex,width,height,tiles);

    // Round EDGES, not sizes: a seam separates x1 of one tile from x1+spacing of the next with an
    // integer spacing, and qRound(v+s)==qRound(v)+s, so every seam stays exactly `spacing` wide
    // and the album's own border stays exact. Each tile's dimensions end up within one pixel of
    // its exact aspect, which the painter covers rather than pads.
    rects.reserve(static_cast<size_t>(n));
    int totalW=0;
    int totalH=0;
    for (const auto& t : tiles)
    {
        auto x0=qRound(t.x);
        auto y0=qRound(t.y);
        auto x1=std::max(x0+1,qRound(t.x+t.w));
        auto y1=std::max(y0+1,qRound(t.y+t.h));
        rects.emplace_back(x0,y0,x1-x0,y1-y0);
        totalW=std::max(totalW,x1);
        totalH=std::max(totalH,y1);
    }

    if (totalSize!=nullptr)
    {
        *totalSize=QSize(totalW,totalH);
    }
    return rects;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
