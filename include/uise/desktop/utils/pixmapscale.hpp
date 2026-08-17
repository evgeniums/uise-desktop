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
*  Declares scaledAndCropped(), scaledToFit() and scaledToFitPadded() - the
*  aspect-ratio policies used to fit a source pixmap into a target box, shared
*  by every preview/thumbnail widget instead of each call site scaling ad hoc.
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
 * @param contentSize Full pixel size of the ORIGINAL image `src` represents, when `src` is only a
 *  reduced-resolution rendition of it (e.g. a chat image tile fed the 1080px `chat` rung of a
 *  4000px photo). Same pixel units as src.size(). Invalid/omitted (the default) means "src IS the
 *  original", i.e. the historical behaviour, unchanged for every existing caller.
 *
 *  This exists because the never-upscale rule below is about the ORIGINAL's resolution, not about
 *  whichever rendition happened to be handed over: a tile given a 1080px rung of a 4000px photo,
 *  inside a box needing 1200 physical px, must fill the box (the detail genuinely exists in the
 *  image; only the delivered rendition is smaller) rather than sit at 1080 centred on a padded
 *  canvas. Passing the original's own size keeps the rule honest in the case it was actually
 *  written for -- a genuinely small image (a 400x300 photo in a 1200px box) still refuses to be
 *  enlarged, because there contentSize equals src.size().
 * @param maxUpscale How far the ORIGINAL may be enlarged beyond its own size, as a multiplier
 *  (e.g. 2.0 allows up to double). 1.0 (the default) is the historical never-upscale rule,
 *  unchanged for every existing caller. Used by chat album tiles (see
 *  ChatMessageImageItem::setMaxUpscale()) so a genuinely small original still fills its tile,
 *  bounded, instead of sitting at native size on a padded canvas -- see
 *  ChatMessageImageItem::updatePreview(). Deliberately a paint-time-only allowance, applied to a
 *  tile's own already-decided rect -- see albumLayout()'s own doc comment for why the analogous
 *  layout-time idea (shrinking the whole album to bound one tile's upscale) was tried and
 *  reverted.
 * @return A pixmap that fits inside boxSize with the source's own aspect ratio preserved (one
 *  dimension may be smaller than the box; never enlarged beyond contentSize*maxUpscale, where
 *  contentSize defaults to the source's own size). Used for previews whose true aspect ratio
 *  should be visible (e.g. the file-upload dialog's big image preview).
 */
inline QPixmap scaledToFit(const QPixmap& src, const QSize& boxSize, const QSize& contentSize=QSize(), qreal maxUpscale=1.0)
{
    // Never upscale beyond the ORIGINAL image's own size, times maxUpscale (this function's own
    // documented contract above) -- clamp the scale target to boxSize on each axis. When src
    // already fits and is itself the original, target equals src.size() and scaled() below is a
    // same-size no-op; deliberately still routed through scaled() rather than returning src
    // directly, so every result -- shrunk, enlarged or unchanged -- is a freshly produced QPixmap
    // via the identical path a RoundedImage brush-texture paint (roundedimage.cpp) has always
    // received here, rather than the caller's original QPixmap object (which callers may go on to
    // mutate/reuse, e.g. FileUploadItem::image() callers).
    const QSize nat=(contentSize.isValid() && !contentSize.isEmpty()) ? contentSize : src.size();
    const QSize limit=(maxUpscale>1.0)
        ? QSize(qRound(nat.width()*maxUpscale),qRound(nat.height()*maxUpscale))
        : nat;
    QSize target(
        qMin(limit.width(),boxSize.width()),
        qMin(limit.height(),boxSize.height())
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
 * @param contentSize Full pixel size of the ORIGINAL image `src` represents when `src` is only a
 *  reduced-resolution rendition of it -- forwarded verbatim to scaledToFit(), see its own doc
 *  comment for the full rationale. Invalid/omitted keeps the historical behaviour.
 * @param maxUpscale Forwarded verbatim to scaledToFit() -- see its own doc comment. 1.0 (the
 *  default) is the historical never-upscale rule, unchanged for every existing caller.
 * @return A pixmap of exactly targetSize, transparent outside the centred, aspect-preserved
 *  source content, never enlarged beyond contentSize*maxUpscale -- i.e. letterboxed/pillarboxed
 *  rather than cropped. Used for chat message image tiles (ChatMessageImageItem) showing REAL
 *  content, where displaying the full image at its own aspect ratio was an explicit, confirmed
 *  requirement (unlike scaledAndCropped()'s square/fixed-chip use cases, which are unaffected).
 *  Those same tiles' low-resolution placeholder uses scaledAndCropped() instead, same as every
 *  other thumbnail chip -- see ChatMessageImageItem::updatePreview()'s own doc comment for why
 *  REAL content is padded but the placeholder is cropped, and for why maxUpscale is non-default
 *  there (album tiles bounded-upscale a genuinely small original rather than pad it).
 */
inline QPixmap scaledToFitPadded(const QPixmap& src, const QSize& targetSize, const QSize& contentSize=QSize(), qreal maxUpscale=1.0)
{
    QPixmap canvas(targetSize);
    canvas.fill(Qt::transparent);
    if (src.isNull() || targetSize.width()<=0 || targetSize.height()<=0)
    {
        return canvas;
    }

    auto fitted=scaledToFit(src,targetSize,contentSize,maxUpscale);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QRect target(QPoint(0,0),fitted.size());
    target.moveCenter(QRect(QPoint(0,0),targetSize).center());
    painter.drawPixmap(target,fitted);

    return canvas;
}

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_PIXMAPSCALE_HPP
