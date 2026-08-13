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
*  Declares scaledAndCropped(), scaledToFit(), scaledToFitPadded() and
*  stretchedToFill() - the aspect-ratio policies used to fit a source pixmap
*  into a target box, shared by every preview/thumbnail widget instead of each
*  call site scaling ad hoc.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_PIXMAPSCALE_HPP
#define UISE_DESKTOP_PIXMAPSCALE_HPP

#include <QPixmap>
#include <QSize>
#include <QRect>
#include <QPainter>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Scale a pixmap to COVER a box and centre-crop the overflow, so the result is exactly
 *  targetSize regardless of the source's aspect ratio.
 * @param src Source pixmap.
 * @param targetSize Target size, in the SAME pixel units as src - callers painting into a
 *  RoundedImage must pass its widget size already multiplied by the screen's devicePixelRatio
 *  (matching RoundedImage::setImageSize()'s own m_size=size*pixelRatio convention,
 *  roundedimage.cpp) AND tag the returned pixmap with QPixmap::setDevicePixelRatio(dpr) before
 *  handing it to setPixmap(): the brush-texture paint path RoundedImage::paintEvent() uses
 *  (setBrush(px);drawRoundedRect(0,0,size().width(),size().height(),...)) draws in LOGICAL
 *  widget coordinates and DOES honor a pixmap's devicePixelRatio tag when texture-mapping it into
 *  that rect -- an untagged physical-sized pixmap is read as 1 raw pixel = 1 logical unit and
 *  ends up dpr times too large for the rect (only its top-left 1/dpr fraction paints, upscaled
 *  and blurred). Both halves are required: scale to physical size for real HiDPI detail, then tag
 *  so the paint path renders it at the correct logical footprint. See
 *  ChatMessageImageItem::updatePreview() / FileUploadListItem::updatePreviews() for the pattern.
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
    // Never upscale beyond the source's own size (this function's own documented contract
    // above) -- clamp the scale target to boxSize on each axis. When src already fits, target
    // equals src.size() and scaled() below is a same-size no-op; deliberately still routed
    // through scaled() rather than returning src directly, so every result -- shrunk or not --
    // is a freshly produced QPixmap via the identical path a RoundedImage brush-texture paint
    // (roundedimage.cpp) has always received here, rather than the caller's original QPixmap
    // object (which callers may go on to mutate/reuse, e.g. FileUploadItem::image() callers).
    QSize target(
        qMin(src.width(),boxSize.width()),
        qMin(src.height(),boxSize.height())
    );
    return src.scaled(target,Qt::KeepAspectRatio,Qt::SmoothTransformation);
}

/**
 * @brief Scale a pixmap to fit inside targetSize (via scaledToFit() -- never upscaled, aspect
 *  preserved) and compose it, centred, onto a fully targetSize canvas -- for painting into a
 *  RoundedImage-based widget whose brush-texture paint path (roundedimage.cpp's paintEvent())
 *  requires an EXACTLY targetSize pixmap to render correctly at all (a smaller pixmap handed to
 *  a QBrush texture fill TILES rather than centres, so scaledToFit()'s own result cannot be
 *  handed to setPixmap() directly for this paint path).
 * @param src Source pixmap.
 * @param targetSize The exact size of the returned canvas -- same pixel-unit convention as
 *  scaledAndCropped()'s targetSize (physical pixels; tag the RETURNED pixmap with
 *  QPixmap::setDevicePixelRatio() before setPixmap(), same rule as scaledAndCropped()'s own doc
 *  comment -- do NOT tag src or the intermediate scaledToFit() result).
 * @return A pixmap of exactly targetSize, transparent outside the centred, aspect-preserved,
 *  never-upscaled source content -- i.e. letterboxed/pillarboxed rather than cropped. Used for
 *  chat message image tiles (ChatMessageImageItem) showing REAL content, where displaying the
 *  full image at its own aspect ratio and never enlarging it past its own resolution was an
 *  explicit, confirmed requirement (unlike scaledAndCropped()'s square/fixed-chip use cases,
 *  which are unaffected). For the low-resolution placeholder those same tiles show while real
 *  content resolves, see stretchedToFill() instead.
 */
inline QPixmap scaledToFitPadded(const QPixmap& src, const QSize& targetSize)
{
    QPixmap canvas(targetSize);
    canvas.fill(Qt::transparent);
    if (src.isNull() || targetSize.width()<=0 || targetSize.height()<=0)
    {
        return canvas;
    }

    auto fitted=scaledToFit(src,targetSize);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QRect target(QPoint(0,0),fitted.size());
    target.moveCenter(QRect(QPoint(0,0),targetSize).center());
    painter.drawPixmap(target,fitted);

    return canvas;
}

/**
 * @brief Stretch a pixmap to fill targetSize exactly, DISTORTING it -- aspect ratio is
 *  deliberately not preserved, and the source is enlarged as far as needed.
 * @param src Source pixmap.
 * @param targetSize Exact size of the result -- same pixel-unit convention as
 *  scaledAndCropped()'s targetSize (physical pixels; tag the RETURNED pixmap with
 *  QPixmap::setDevicePixelRatio() before setPixmap()).
 * @return A pixmap of exactly targetSize with no padding and no cropping, at the cost of the
 *  source's proportions.
 *
 * Sole intended use is a low-resolution PLACEHOLDER standing in for content that has not arrived
 * yet -- specifically a chat message tile's embedded ~128px thumbnail, which files2 generates
 * with ScaleMode::FillCrop, i.e. a SQUARE centre-crop of the original. Fitting that square into a
 * tile shaped for the original's real aspect ratio leaves large empty bars, so the placeholder
 * reads as a small square adrift in a blank tile; stretching it instead fills the tile with a
 * blurry, distorted, but recognisable impression of the image, which is the confirmed requirement
 * here ("scaled up to fill the whole tile, not only the square, i.e. losing aspect ratio").
 *
 * Do NOT use this for real content: distortion is only acceptable because the result is
 * transient and already visibly low quality.
 */
inline QPixmap stretchedToFill(const QPixmap& src, const QSize& targetSize)
{
    if (src.isNull() || targetSize.width()<=0 || targetSize.height()<=0)
    {
        QPixmap canvas(targetSize.isValid() ? targetSize : QSize(1,1));
        canvas.fill(Qt::transparent);
        return canvas;
    }
    return src.scaled(targetSize,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
}

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_PIXMAPSCALE_HPP
