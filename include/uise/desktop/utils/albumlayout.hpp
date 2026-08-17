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
                        //!< was shrunk by the natural-size cap below, see albumLayout().
    int maxHeight=420;  //!< Total height budget; exceeding it triggers a uniform scale-down.
    int minTile=60;     //!< Floor on any single tile's width/height.
    int spacing=2;      //!< Gap between adjacent tiles.

    //! Converts pixelSizes (device pixels) into the logical units the returned rects are in, so
    //! the natural-size cap (see albumLayout()) can tell how large each image actually is on
    //! screen. Leave at 1.0 if pixelSizes are already logical. Set to 0 to disable the cap
    //! entirely (pure aspect-driven templates, the historical behaviour).
    qreal devicePixelRatio=1.0;
};

/**
 * @brief Compute tile rectangles for a set of images from their pixel sizes, Telegram-style:
 *  a handful of hand-picked templates for 1-4 images, falling back to a justified-rows packing
 *  for 5 or more.
 * @param pixelSizes Pixel dimensions of each image, in display order, in DEVICE pixels (see
 *  options.devicePixelRatio). An entry with a non-positive width or height is treated as square
 *  (aspect 1:1) rather than producing a degenerate rect, and is exempt from the natural-size cap
 *  below -- there is no known resolution to cap it against.
 * @param options Layout bounds.
 * @param totalSize Optional out-param receiving the overall album size (equivalent to the
 *  bounding rect of every returned QRect). May be NARROWER than options.maxWidth when the cap
 *  below shrank a row -- callers should size themselves to this rather than assuming maxWidth
 *  (see ChatMessageImages::bubbleWidthHint(), which lets the bubble hug the album).
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
 *  2. A per-tile NATURAL-SIZE CAP: no tile may exceed the image's own size in logical units
 *     (pixelSizes divided by options.devicePixelRatio), floored at options.minTile so a very
 *     small image still gets a usable tile. A capped tile is shrunk in place, preserving the
 *     shape its template chose, and its row is then re-flowed left-to-right so no gap is left
 *     where it shrank. Rows containing a capped tile therefore do NOT sum to maxWidth, and the
 *     album as a whole can be narrower than the budget.
 *
 *  The cap is what keeps a 100px thumbnail from being handed the same tile as a 2048px photo just
 *  because they share an aspect ratio -- their tiles now differ in size the same way the images
 *  do. It is deliberately PER TILE: an earlier version instead shrank the whole album uniformly
 *  whenever any one tile was oversized, which dragged every other tile down with it whenever one
 *  small image shared an album with normal-sized photos.
 *
 *  Tiles are meant to be filled by fitting the source INSIDE the rect, preserving its own aspect
 *  ratio and never upscaling past its native resolution -- see utils/pixmapscale.hpp's
 *  scaledToFitPadded() and its maxUpscale parameter for the bounded exception used by chat album
 *  tiles, which after the cap above should only ever be needed for an image smaller than minTile.
 *  The returned geometry is still a budget rather than an exact content shape: a tile's own
 *  aspect ratio only influences which template/proportions get picked (and, for some multi-image
 *  templates, is only approximated -- see stackByAspect()'s own doc comment), so painted content
 *  may still be letterboxed/pillarboxed within its rect. This function itself only computes
 *  geometry; it does not paint or crop anything.
 */
UISE_DESKTOP_EXPORT std::vector<QRect> albumLayout(
    const std::vector<QSize>& pixelSizes,
    const AlbumLayoutOptions& options,
    QSize* totalSize=nullptr
);

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ALBUMLAYOUT_HPP
