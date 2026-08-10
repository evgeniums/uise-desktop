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

/** @file uise/desktop/utils/pixmapscale.hpp
*
*  Declares scaledAndCropped() and scaledToFit() - the two aspect-ratio
*  policies used to fit a source pixmap into a target box, shared by every
*  preview/thumbnail widget instead of each call site scaling ad hoc.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_PIXMAPSCALE_HPP
#define UISE_DESKTOP_PIXMAPSCALE_HPP

#include <QPixmap>
#include <QSize>
#include <QRect>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Scale a pixmap to COVER a box and centre-crop the overflow, so the result is exactly
 *  targetSize regardless of the source's aspect ratio.
 * @param src Source pixmap.
 * @param targetSize Target size, in the SAME pixel units as src - callers painting into a
 *  RoundedImage must pass its widget size already multiplied by the screen's devicePixelRatio
 *  (matching RoundedImage::setImageSize()'s own m_size=size*pixelRatio convention,
 *  roundedimage.cpp) rather than tagging the result with QPixmap::setDevicePixelRatio(): the
 *  brush-texture paint path RoundedImage::paintEvent() uses (setBrush(px);drawRoundedRect(...))
 *  tiles at the pixmap's raw pixel size and does not itself scale for a dpr tag, so an untagged,
 *  already-physical-sized pixmap is what avoids the blur a naive logical-size scale produces on
 *  HiDPI/Retina.
 * @return A pixmap of exactly targetSize, covering the box with no letterboxing. Used for
 *  square/fixed-size chips (e.g. the file-upload row thumbnail) where the aspect ratio must not
 *  vary.
 */
inline QPixmap scaledAndCropped(const QPixmap& src, const QSize& targetSize)
{
    auto scaled=src.scaled(targetSize,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);
    QRect cropRect(
        (scaled.width()-targetSize.width())/2,
        (scaled.height()-targetSize.height())/2,
        targetSize.width(),
        targetSize.height()
    );
    return scaled.copy(cropRect);
}

/**
 * @brief Scale a pixmap to FIT INSIDE a bounding box, preserving its aspect ratio - the result
 *  may be narrower than boxSize on one axis, never cropped and never exceeding the box.
 * @param src Source pixmap.
 * @param boxSize Bounding box - same pixel-unit convention as scaledAndCropped()'s targetSize
 *  (already dpr-scaled by the caller when painted via a RoundedImage's brush texture path).
 * @return A pixmap that fits inside boxSize with the source's own aspect ratio preserved (one
 *  dimension may be smaller than the box; never upscaled beyond the source's own size). Used for
 *  previews whose true aspect ratio should be visible (e.g. the file-upload dialog's big image
 *  preview).
 */
inline QPixmap scaledToFit(const QPixmap& src, const QSize& boxSize)
{
    return src.scaled(boxSize,Qt::KeepAspectRatio,Qt::SmoothTransformation);
}

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_PIXMAPSCALE_HPP
