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
 * @brief Parameters bounding an album's overall footprint.
 */
struct UISE_DESKTOP_EXPORT AlbumLayoutOptions
{
    int maxWidth=420;   //!< Bubble content width budget -- rows sum to exactly this unless a tile
                        //!< was shrunk by the natural-size cap below, see albumLayout(). Unlike
                        //!< maxHeight below, this is a HARD ceiling: the caller (ChatMessageImages
                        //!< ::bubbleWidthHint()) clamps the bubble to it, so a tile sticking out
                        //!< past it would simply be cut off -- albumLayout() never lets one.
    int maxHeight=420;  //!< Total height budget. Exceeding it normally triggers a uniform
                        //!< scale-down, but that scale-down is a SOFT target: the minCappedTile
                        //!< floor below overrides it, so the returned album can end up taller than
                        //!< this when honouring the floor requires it. Nothing downstream depends
                        //!< on the album staying under this value -- see albumLayout()'s own doc
                        //!< comment.
    int minTile=60;     //!< Floor on any single tile's width/height -- including a TEMPLATE's own
                        //!< row/column height (e.g. stackByAspect()), not just the natural-size
                        //!< cap below. Keep this modest: raising it distorts legitimate rows (a
                        //!< very wide image wants a short row).
    //! Hard floor on BOTH axes of every tile -- todo-album-layout-small-tile-packing.md's "pack
    //! small tiles" ask, taken Telegram-style: rather than a 2D packing pass, a tile that would
    //! otherwise land under this size (whether because its own image is genuinely small, or
    //! because a dense multi-image template just packed it small) is instead GROWN to it, aspect
    //! preserved, and the image is expected to be scaled up to fill it (see
    //! ChatMessageImageItem::setMaxUpscale()/scaledToFitPadded()'s maxUpscale). Never met by
    //! cropping or distorting -- scaling any rect uniformly by max(floor/w,floor/h) always puts
    //! BOTH resulting dimensions at or above the floor, whatever the rect's own aspect ratio (see
    //! albumLayout()'s own comment for the proof) -- so this floor takes priority over both the
    //! natural-size cap and the maxHeight scale-down above.
    //!
    //! One exception, forced by "never crop, never distort": a tile whose aspect ratio would need
    //! a width above maxWidth to reach this floor is capped at the width budget instead (maxWidth
    //! is hard, see its own comment) -- honouring the floor there would require either cropping or
    //! a tile wider than the whole album. In practice this only affects a tile whose aspect ratio
    //! exceeds maxWidth/minCappedTile, e.g. above ~3.2 at a 320px bubble with the shipped 100px
    //! floor -- panorama-class sources, which the separate send-time extreme-aspect-ratio guard
    //! (whitemclient/files2/imageprocessor.cpp, 10:1) already downgrades to a plain document
    //! before they would ever reach this layout as an image tile.
    //!
    //! Deliberately separate from minTile above: minTile also bounds ordinary template row
    //! heights, and raising IT to a thumbnail-sized floor would clamp and distort rows of
    //! normal-sized images too. 0 means "use minTile" (the pre-existing behaviour).
    int minCappedTile=0;
    int spacing=2;      //!< Gap between adjacent tiles.

    //! Width, in the same logical units as maxWidth, that OTHER content in the same bubble has
    //! already claimed -- in practice a caption longer than the album is wide (see
    //! ChatMessageImages::bubbleWidthHint(), which measures its comment before laying the album
    //! out and passes the result here). 0, the default, disables the re-pack this feeds entirely,
    //! leaving the returned geometry bit-identical to what the templates and the per-tile passes
    //! produced on their own.
    //!
    //! Why it exists: which row a tile lands in is decided by the templates / justified-rows
    //! packing from the tiles' PRE-cap sizes -- sizes picked to fill maxWidth. The per-tile
    //! natural-size cap (see albumLayout()'s pass 2) then shrinks small images to their real
    //! on-screen size but never revisits that decision, so an album of thumbnails keeps a row
    //! count computed for tiles several times larger than the ones actually drawn. Five 133px
    //! tiles stay in 2+2+1 rows totalling 274px even when the bubble is 590px wide and three of
    //! them would comfortably share a line.
    //!
    //! That waste is invisible while the album alone decides the bubble's width -- the bubble
    //! just hugs the narrow block -- so re-packing is deliberately NOT done on the strength of
    //! maxWidth alone: doing it there would make a caption-less thumbnail album claim a much
    //! wider bubble for itself (5 tiles at 274px becoming one 544px line) purely because the
    //! budget allowed it. Only space some other section has ALREADY committed to is free to
    //! spend, which is what this reports and the only case the re-pack acts on.
    //!
    //! The re-pack keeps every tile's size and the images' order, and never increases the row
    //! count or the album's height (see albumLayout()'s pass 4); it only merges rows that the
    //! stale pre-cap sizing split, then balances the survivors so the last row is not left
    //! nearly empty.
    int claimedWidth=0;

    //! Converts pixelSizes (device pixels) into the logical units the returned rects are in, so
    //! the natural-size cap (see albumLayout()) can tell how large each image actually is on
    //! screen. Leave at 1.0 if pixelSizes are already logical. Set to 0 to disable the cap AND
    //! the minCappedTile floor entirely (pure aspect-driven templates, the historical behaviour).
    qreal devicePixelRatio=1.0;
};

/**
 * @brief Compute tile rectangles for a set of images from their pixel sizes, Telegram-style:
 *  a handful of hand-picked templates for 1-4 images, falling back to a justified-rows packing
 *  for 5 or more.
 * @param pixelSizes Pixel dimensions of each image, in display order, in DEVICE pixels (see
 *  options.devicePixelRatio). An entry with a non-positive width or height is treated as square
 *  (aspect 1:1) rather than producing a degenerate rect, and is exempt from the natural-size cap
 *  below (there is no known resolution to cap it against) but NOT from the minCappedTile floor --
 *  growing a rect to a floor needs no knowledge of the source, and a placeholder tile is exactly
 *  the one showing a load control, so it must not be tiny either.
 * @param options Layout bounds.
 * @param totalSize Optional out-param receiving the overall album size (equivalent to the
 *  bounding rect of every returned QRect). May be NARROWER than options.maxWidth when the cap
 *  below shrank a row, and may be TALLER than options.maxHeight when the minCappedTile floor held
 *  a tile up against the height scale-down -- callers should size themselves to this rather than
 *  assuming either budget (see ChatMessageImages::bubbleWidthHint(), which lets the bubble hug the
 *  album's actual width).
 * @return One rect per input image, same order, all within [0,0,totalSize]. Images are also
 *  DISPLAYED in that order -- left to right, top to bottom, including the "hero" (full-width or
 *  big-left) slot some templates have, which always goes to the first image. Geometry is decided
 *  in two passes:
 *
 *  1. A template picked from the images' ASPECT ratios lays out full-width rows/columns. For 3-4
 *     images the template is chosen from the MULTISET of aspect-ratio classes across ALL images
 *     (see classify() in the .cpp), never from a single image's class, so reordering the same set
 *     of images picks the same template -- which is what keeps an unrepresentative first image
 *     from selecting a shape that suits none of the set, without having to reorder anything.
 *     Templates deliberately prefer spending the horizontal budget (a hero image plus a packed
 *     row) over stacking every image into a narrow tall column.
 *  2. A per-tile sizing pass bounded from ABOVE by the image's own resolution in logical units
 *     (pixelSizes divided by options.devicePixelRatio -- the natural-size cap) and from BELOW by
 *     options.minCappedTile on BOTH axes (or options.minTile if that is 0 -- the floor). The two
 *     bounds combine so the floor always wins when they disagree: a blurry, usable tile beats an
 *     accurately-sized, unusable one. Scaling a rect uniformly by max(floor/w,floor/h) always
 *     lands both resulting dimensions at or above the floor, whatever the rect's own aspect ratio
 *     -- proof: if w<=h then floor/w>=floor/h, so the factor is floor/w, giving new width exactly
 *     floor and new height floor*(h/w)>=floor since h>=w; symmetric otherwise. The floor is
 *     therefore always reachable by a pure aspect-preserving scale and NEVER needs a crop (see
 *     ChatMessageImageItem::updatePreview()'s own never-crop rule for real content) -- with one
 *     exception, see minCappedTile's own comment: a tile whose aspect ratio would need a width
 *     above maxWidth to reach the floor is capped at the width budget instead, since maxWidth is a
 *     hard ceiling and the floor is not. An adjusted tile is rescaled in place, preserving the
 *     shape its template chose, and its row is then re-flowed left-to-right, WRAPPING onto a
 *     further line if growth pushed it past maxWidth. Rows containing an adjusted tile therefore
 *     do NOT sum to maxWidth, and the album as a whole can be narrower than the width budget (a
 *     capped/floored row) or taller than the height budget (the floor overriding pass 3 below).
 *  3. If the album from pass 2 is taller than options.maxHeight, every rect is scaled down
 *     uniformly to fit -- this step is floor-BLIND (a tile pass 2 floored can be pushed back under
 *     the floor by it) -- followed by a floor-only regrow pass, structurally identical to pass 2's
 *     lower bound, that restores the floor for anything pass 3 pushed below it. Deliberately two
 *     passes rather than a single floor-aware rescale factor: clamping the rescale itself would
 *     let one at-floor tile veto the whole shrink, ballooning the album far past maxHeight;
 *     shrinking everything first and then re-growing only what fell through the floor lets the
 *     OTHER (non-floored) tiles absorb the height instead, keeping the album close to its budget.
 *  4. Finally, if options.claimedWidth says another section of the bubble has already claimed
 *     more width than the album ended up using, the rows are re-packed into it -- see
 *     claimedWidth's own comment for why this is gated on already-claimed space rather than on
 *     the width budget. Tile SIZES and image order are untouched; only which row each tile sits
 *     in changes. Rows are first merged greedily (first-fit, which minimises the row count for a
 *     fixed order), then the narrowest width achieving that same row count is found by bisection
 *     and used for the actual packing, so the tiles spread evenly (five tiles in a 590px bubble
 *     become 3+2, not 4+1). Because greedy first-fit is row-count-optimal, this pass can never
 *     produce MORE rows than pass 3 left, and since each line's height is a maximum over its own
 *     members, merging lines can never make the album taller -- so it cannot undo pass 3's
 *     maxHeight work or push a tile back under the floor.
 *
 *  The natural-size cap (pass 2's upper bound) is what keeps a 100px thumbnail from being handed
 *  the same tile as a 2048px photo just because they share an aspect ratio -- their tiles now
 *  differ in size the same way the images do. It is deliberately PER TILE: an earlier version
 *  instead shrank the whole album uniformly whenever any one tile was oversized, which dragged
 *  every other tile down with it whenever one small image shared an album with normal-sized
 *  photos. The minCappedTile floor (pass 2's lower bound, reinforced in pass 3) is what then keeps
 *  the natural-size cap itself from producing a genuinely tiny, hard-to-use tile.
 *
 *  Tiles are meant to be filled by fitting the source INSIDE the rect, preserving its own aspect
 *  ratio and never upscaling past its native resolution -- see utils/pixmapscale.hpp's
 *  scaledToFitPadded() and its maxUpscale parameter for the bounded exception used by chat album
 *  tiles, which after the passes above should only ever be needed for an image smaller than
 *  minCappedTile. The returned geometry is still a budget rather than an exact content shape: a
 *  tile's own aspect ratio only influences which template/proportions get picked (and, for some
 *  multi-image templates, is only approximated -- see stackByAspect()'s own doc comment), so
 *  painted content may still be letterboxed/pillarboxed within its rect. This function itself only
 *  computes geometry; it does not paint or crop anything.
 */
UISE_DESKTOP_EXPORT std::vector<QRect> albumLayout(
    const std::vector<QSize>& pixelSizes,
    const AlbumLayoutOptions& options,
    QSize* totalSize=nullptr
);

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ALBUMLAYOUT_HPP
