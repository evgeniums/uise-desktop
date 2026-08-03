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
    int maxWidth=420;   //!< Bubble content width budget -- every row/column sums to exactly this.
    int maxHeight=420;  //!< Total height budget; exceeding it triggers a uniform scale-down.
    int minTile=60;     //!< Floor on any single tile's width/height.
    int spacing=2;      //!< Gap between adjacent tiles.
};

/**
 * @brief Compute tile rectangles for a set of images from their pixel sizes, Telegram-style:
 *  a handful of hand-picked templates for 1-4 images (chosen by each image's aspect-ratio
 *  class), falling back to a justified-rows packing for 5 or more.
 * @param pixelSizes Pixel dimensions of each image, in display order. An entry with a
 *  non-positive width or height is treated as square (aspect 1:1) rather than producing a
 *  degenerate rect.
 * @param options Layout bounds.
 * @param totalSize Optional out-param receiving the overall album size (equivalent to the
 *  bounding rect of every returned QRect).
 * @return One rect per input image, same order, all within [0,0,totalSize]. Tiles are meant to
 *  be filled by center-cropping (KeepAspectRatioByExpanding then crop to the rect) -- the
 *  returned geometry is authoritative and a tile's own aspect ratio only ever influenced which
 *  template/proportions were picked, never the final rect shape.
 */
UISE_DESKTOP_EXPORT std::vector<QRect> albumLayout(
    const std::vector<QSize>& pixelSizes,
    const AlbumLayoutOptions& options,
    QSize* totalSize=nullptr
);

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ALBUMLAYOUT_HPP
