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

/** @file uise/desktop/utils/albumlayout.hpp
*
*  Declares albumLayout().
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ALBUMLAYOUT_HPP
#define UISE_DESKTOP_ALBUMLAYOUT_HPP

#include <vector>

#include <QRect>
#include <QSize>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Which of the two album layout algorithms albumLayout() runs.
 */
enum class AlbumLayoutMode
{
    //! Exact-aspect, gapless guillotine layout that spends the whole width budget and never
    //! crops -- the default. See albumLayout().
    Wide,
    //! Telegram-style preset templates for 1-4 images and a small enumerated search over row
    //! compositions for more, with aspect ratios clamped to a comfortable range and the
    //! difference absorbed by centre-cropping the image inside its cell. See
    //! albumLayoutPresets().
    PresetTemplates
};

/**
 * @brief Tunables of the PresetTemplates mode (see albumLayoutPresets()). Ratios and
 *  thresholds are in THOUSANDTHS (1000 = 1.0), the mode's arithmetic being integer-only;
 *  cell minimums are thousandths of the width budget.
 *
 * Invariants: minRatio < narrowThreshold < wideThreshold < maxRatio (so clamping a ratio never
 * changes an image's class), and weightOver > weightMinCell > weightAspect+weightEven+weightCrop
 * (so the hard constraints are never outbid by the aesthetic ones).
 */
struct UISE_DESKTOP_EXPORT AlbumPresetConfig
{
    int minRatio=600;          //!< below this a cell is not made narrower, the image is cropped instead
    int maxRatio=1700;         //!< above this a cell is not made wider, the image is cropped instead
    int narrowThreshold=800;   //!< class Narrow when the image's ratio is at most this
    int wideThreshold=1200;    //!< class Wide when the image's ratio is at least this
    int similarThreshold=200;  //!< two ratios count as similar when within this of each other
    int stackAverage=1400;     //!< two similar Wide images stack when their average ratio reaches this
    int minCellWidth=150;      //!< minimum cell width, thousandths of the width budget
    int minCellHeight=100;     //!< minimum cell height, thousandths of the width budget
    int maxRows=4;             //!< composition search: at most this many rows
    int maxPerRow=4;           //!< composition search: at most this many cells per row
    int targetAspect=1400;     //!< block aspect (W/H) the search steers towards
    int weightAspect=1000;
    int weightEven=600;
    int weightCrop=400;
    int weightMinCell=4000;
    int weightOver=8000;
};

/**
 * @brief Parameters bounding an album's overall footprint.
 */
struct UISE_DESKTOP_EXPORT AlbumLayoutOptions
{
    AlbumLayoutMode mode=AlbumLayoutMode::Wide; //!< which algorithm runs, see AlbumLayoutMode
    AlbumPresetConfig presets;                   //!< PresetTemplates tunables, ignored by Wide

    int maxWidth=420;   //!< Width budget, HARD: the returned album is never wider than this. The
                        //!< caller (ChatMessageImages::bubbleWidthHint()) clamps the bubble to it,
                        //!< so a tile sticking out past it would simply be cut off. The layout
                        //!< prefers to use all of it (see albumLayout()).
    int maxHeight=420;  //!< Height budget, also HARD: an album that would be taller is scaled
                        //!< down uniformly, which makes it narrower than maxWidth instead.
    int minTile=60;     //!< Soft floor on a tile's short side. Defended by the layout's cost
                        //!< function (a structure that squeezes any tile under it is heavily
                        //!< penalised), not enforced -- a budget too small for n tiles of this
                        //!< size necessarily breaks it. Also the default for shrinkFloor below.
    //! Lower bound, on a tile's short side, for the uniform all-thumbnail shrink (see
    //! albumLayout()): an album in which EVERY image is smaller than its tile is scaled down as
    //! a whole until its largest image is shown at natural size -- but never so far that any
    //! tile's short side drops below this. 0 means "use minTile". ChatMessageImages feeds its
    //! QSS-settable minTileSize here, which doubles as the placeholder tile extent, so a genuinely
    //! small image and an unresolved placeholder read at the same scale.
    int shrinkFloor=0;
    int spacing=2;      //!< Gap between adjacent tiles -- every seam in the returned geometry is
                        //!< exactly this wide, and there are no other gaps.

    //! Converts pixelSizes (device pixels) into the logical units the returned rects are in. Used
    //! for two things only: steering genuinely small images into the smaller slots of a
    //! structure (see albumLayout()'s cost function), and the uniform all-thumbnail shrink (see
    //! shrinkFloor). It never changes a tile's shape. Leave at 1.0 if pixelSizes are already
    //! logical; 0 disables both uses (pure aspect-driven layout).
    qreal devicePixelRatio=1.0;
};

/**
 * @brief Compute tile rectangles for a set of images from their pixel sizes. Dispatches on
 *  options.mode: AlbumLayoutMode::PresetTemplates runs albumLayoutPresets(); everything below
 *  describes the default, AlbumLayoutMode::Wide -- an exact-aspect, gapless "guillotine" grid.
 * @param pixelSizes Pixel dimensions of each image, in display order, in DEVICE pixels (see
 *  options.devicePixelRatio). An entry with a non-positive width or height is treated as square
 *  (aspect 1:1), takes no part in the small-image steering or the uniform shrink (there is no
 *  known resolution to compare against), and otherwise lays out like any other tile.
 * @param options Layout bounds.
 * @param totalSize Optional out-param receiving the overall album size -- the bounding rect of
 *  every returned QRect, which the tiles fill completely apart from the spacing seams. Never
 *  wider than options.maxWidth nor taller than options.maxHeight. It can be smaller than both
 *  when the chosen structure is height-bound, or when the uniform all-thumbnail shrink applied
 *  -- callers should size themselves to this rather than assuming either budget (see
 *  ChatMessageImages::bubbleWidthHint(), which lets the bubble hug the album).
 * @return One rect per input image, same order. Guarantees:
 *
 *  - **Exact aspect.** Every tile has its own image's aspect ratio to within one logical pixel
 *    per dimension (integer rounding of its edges), so the image fills its tile with no
 *    letterboxing; the painter covers that one-pixel residual (see
 *    ChatMessageImageItem::updatePreview()). No aspect is ever clamped or approximated.
 *  - **Gapless.** Every edge of every tile either lies on the album's border or faces a
 *    neighbouring tile across a seam of exactly options.spacing. Both budgets are hard.
 *  - **Message order.** Tiles are placed by an in-order traversal of a binary tree of
 *    side-by-side / stacked splits over the images in input order: for any i<j, tile j lies
 *    entirely to the right of tile i or entirely below it. The first image therefore always
 *    occupies the top-left slot, the last the bottom-right one, and a caption saying "the first
 *    photo" keeps meaning the first tile. Which STRUCTURE is picked depends on the whole set
 *    (see below), so reordering the same images may change the shape -- but never the order.
 *  - **Deterministic.** The same input always yields the same geometry.
 *
 *  How the structure is chosen: every binary tree of horizontal/vertical splits over the
 *  sequence has an exactly computable aspect ratio (side by side: aspects add; stacked: their
 *  reciprocals add -- spacing is carried along exactly as an affine term), so a dynamic
 *  programme over contiguous sub-sequences enumerates candidate structures, keeping for each
 *  sub-sequence a bounded set that is diverse in aspect ratio and best in a scale-invariant
 *  partial cost. The root candidates are then fitted into the maxWidth x maxHeight box (as wide
 *  as the box allows, height permitting) and the one with the lowest total cost wins. The cost
 *  prefers, in decreasing weight: no tile squeezed under minTile; the album using the full width
 *  budget and as much of the box's area as possible (so a 1-row strip of tiny tiles loses to a
 *  2-row block of large ones); balanced tile areas -- where "balanced" means every image gets an
 *  area proportional to its own natural logical area, capped at an equal share of the box, so
 *  photographs end up equal-sized while a genuinely small image (a thumbnail among photos) is
 *  steered into a correspondingly smaller slot rather than being blown up to photo size; and,
 *  as a weak tie-breaker, simple row/column structures over deeply nested ones.
 *
 *  One inherent consequence: two images of the same aspect ratio always receive equal tiles
 *  (any split of the two is symmetric), so small-image steering only has room to act from three
 *  images up.
 *
 *  Small images are never given a tile smaller than the structure dictates -- that is what
 *  keeps the album gapless -- so the painter upscales them to fill it. The one exception is an
 *  album in which EVERY image with a known size would be upscaled: that album is scaled down as
 *  a whole, uniformly (so it stays gapless), until its largest image is shown at natural size,
 *  bounded below by options.shrinkFloor. A single normal photograph in the set prevents any
 *  shrink, so one thumbnail can never drag its neighbours down.
 */
UISE_DESKTOP_EXPORT std::vector<QRect> albumLayout(
    const std::vector<QSize>& pixelSizes,
    const AlbumLayoutOptions& options,
    QSize* totalSize=nullptr
);

/**
 * @brief The AlbumLayoutMode::PresetTemplates algorithm, callable directly (albumLayout()
 *  routes here when options.mode says so): Telegram-style preset templates for 1-4 images,
 *  an enumerated search over row compositions for more, centre-cropping instead of exact
 *  aspects.
 * @param pixelSizes As for albumLayout(). A non-positive size is treated as square.
 * @param options Layout bounds; options.presets holds this mode's tunables.
 * @param totalSize Optional out-param receiving the block's size -- exactly options.maxWidth
 *  wide for every block of two or more images (a single image is fitted by its own ratio), never
 *  taller than options.maxHeight.
 * @return One rect per input image, same order. Guarantees:
 *
 *  - **Gapless and within budget.** Cells in a row share a height and sum exactly to the width
 *    budget; rows are separated by exactly options.spacing; a block taller than maxHeight has
 *    its row heights squeezed proportionally to fit (widths unchanged, crop grows).
 *  - **Cropped, not padded.** Each image's ratio is clamped to [minRatio, maxRatio] before any
 *    geometry is computed and the cell is sized from the clamped ratio, so the image is expected
 *    to be painted aspect-FILL (cover the cell, centre-crop the excess -- see
 *    ChatMessageImageItem::setCoverContent()). The crop is what lets a template keep its shape
 *    whatever the exact ratios are.
 *  - **Message order**, same rule as albumLayout(): for i<j, tile j is entirely right of or
 *    entirely below tile i.
 *  - **Deterministic**, integer-only arithmetic: lengths are distributed with a remainder-takes-
 *    the-rest share, so a row never loses a pixel to rounding, and ties in the composition
 *    search keep the lexicographically smaller composition.
 *
 *  Templates (each classifies images as Narrow / Square / Wide by narrowThreshold /
 *  wideThreshold): one image fits its ratio into the box; two similar Wide images stack, two
 *  similar same-class images split the width in half, otherwise a proportional row; three
 *  Narrow images make a row, a Wide first image goes on top of a row of two, otherwise a
 *  full-height column on the left beside a stack of two; four images: a Wide first image on top
 *  of a row of three, a Narrow first image as a column beside a stack of three, otherwise a 2x2
 *  grid with a shared vertical seam. A template whose cells would fall under the minimum cell
 *  size is discarded in favour of the composition search, which scores every way of splitting
 *  the images into at most maxRows rows of at most maxPerRow cells by block aspect, row-height
 *  evenness, mean crop loss, cell minimums and height overflow.
 *
 *  Like albumLayout(), an album in which every known image would be upscaled is laid out again
 *  at a proportionally smaller width budget, bounded by options.shrinkFloor, so a lone thumbnail
 *  is not blown up to the bubble width.
 */
UISE_DESKTOP_EXPORT std::vector<QRect> albumLayoutPresets(
    const std::vector<QSize>& pixelSizes,
    const AlbumLayoutOptions& options,
    QSize* totalSize=nullptr
);

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ALBUMLAYOUT_HPP
