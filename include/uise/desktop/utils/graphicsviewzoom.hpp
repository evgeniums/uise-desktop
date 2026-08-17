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

/** @file uise/desktop/utils/graphicsviewzoom.hpp
*
*  Declares GraphicsViewZoom.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_GRAPHICSVIEWZOOM_HPP
#define UISE_DESKTOP_GRAPHICSVIEWZOOM_HPP

#include <functional>

#include <QObject>
#include <QPoint>

#include <uise/desktop/uisedesktop.hpp>

class QGraphicsView;
class QGraphicsItem;

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Trackpad/mouse zoom and drag-to-pan for a QGraphicsView, shared by ImageViewer and
 * SimpleImageEditor.
 *
 * Works as an event filter installed on view->viewport() rather than a QGraphicsView subclass, so
 * it attaches equally to a plain QGraphicsView (ImageViewer) and to an existing QGraphicsView
 * subclass (SimpleImageEditor's FreeHandDrawView) without touching either hierarchy.
 *
 * Handles macOS trackpad pinch (QNativeGestureEvent, Qt::ZoomNativeGesture) and Ctrl/Cmd+wheel zoom
 * -- complementary paths, not redundant: a macOS pinch does not synthesize Ctrl+wheel, and Windows
 * precision touchpads only ever send Ctrl+wheel for a pinch. An unmodified wheel event is left
 * untouched so QGraphicsView keeps scrolling/panning with it exactly as before this helper existed.
 *
 * Deliberately stateless with respect to scale: currentScale()/baselineScale() are always derived
 * from the view's live transform (never cached), so there is nothing for the host's own rotate()/
 * flipHorizontal()/fitInView() calls to invalidate. currentScale() uses sqrt(m11^2+m12^2), the same
 * idiom CropRectItem already relies on (see imagecropper.cpp), which stays correct even when the
 * transform also carries a flip (negative m11) or a 90-degree rotation (m11==0).
 *
 * Mouse button/move events are observed for panning but are NEVER consumed (eventFilter() never
 * returns true for them) -- a host relying on its own click/drag detection (e.g. ImageViewerWidget's
 * viewerClicked()) keeps working unchanged; only zoom-modifier wheel events and native pinch/smart-
 * zoom gestures are actually consumed.
 */
class UISE_DESKTOP_EXPORT GraphicsViewZoom : public QObject
{
    Q_OBJECT

    public:

        //! Multiplier applied by zoomIn()/zoomOut() and by one wheel "notch"/50px of pixelDelta.
        constexpr static const qreal DefaultStepFactor=1.25;

        //! Minimum zoom, relative to baselineScale() -- 1.0 means "never smaller than fit".
        constexpr static const qreal DefaultMinZoomFactor=1.0;

        //! Maximum zoom, relative to baselineScale().
        constexpr static const qreal DefaultMaxZoomFactor=8.0;

        //! Pixel-delta trackpad scroll distance treated as equivalent to one wheel notch.
        constexpr static const qreal DefaultPixelsPerNotch=50.0;

        //! Installs itself as an event filter on view->viewport(). view must outlive this object.
        explicit GraphicsViewZoom(QGraphicsView* view, QObject* parent=nullptr);

        QGraphicsView* view() const noexcept;

        //! Item baselineScale()/fitToItem() measure against. Pass nullptr when nothing is loaded
        //! (e.g. on unload/reset) -- baselineScale() and isZoomed() degrade gracefully to 1.0/false.
        void setFitItem(QGraphicsItem* item);
        QGraphicsItem* fitItem() const noexcept;

        //! When true (default) baselineScale() never exceeds 1.0 -- mirrors the "fit only if the
        //! image is larger than the viewport, otherwise native size" rule both ImageViewer and
        //! SimpleImageEditor already apply on initial load.
        void setFitOnlyIfLarger(bool value);
        bool isFitOnlyIfLarger() const noexcept;

        //! Re-fit the view to fitItem() now: QGraphicsView::fitInView() if it is larger than the
        //! viewport, else normalizes the transform's scale magnitude to 1.0 while PRESERVING
        //! whatever rotation/flip the transform already carries (unlike a bare resetTransform(),
        //! which would also discard rotation/flip).
        void fitToItem();

        //! sqrt(m11^2+m12^2) of the view's current transform -- the scale magnitude, invariant to
        //! rotation and flip. Never 0 (falls back to 1.0 for a degenerate transform).
        qreal currentScale() const;

        //! The scale at which fitItem() exactly fits the viewport (capped at 1.0 when
        //! isFitOnlyIfLarger()), computed live from the CURRENT transform every call -- so it is
        //! automatically correct after a viewport resize, after rotation, or after fitItem()'s own
        //! bounding rect changes (e.g. a higher-resolution pixmap arriving asynchronously). 1.0 if
        //! fitItem() is null or the viewport/item bounds are degenerate.
        qreal baselineScale() const;

        //! currentScale()/baselineScale(); 1.0 at the fit/baseline level.
        qreal zoomFactor() const;

        //! zoomFactor() meaningfully above 1.0.
        bool isZoomed() const;

        //! Whether either scrollbar currently has a non-empty range -- i.e. there is anything to pan.
        bool isPannable() const;

        //! Whether a pan drag is in progress right now.
        bool isPanning() const noexcept;

        //! Zoom to an absolute scale (not a zoomFactor -- a raw view transform magnitude), clamped,
        //! anchored so the scene point under anchorViewportPos stays under it after the zoom.
        void zoomTo(qreal absoluteScale, const QPoint& anchorViewportPos);

        //! Multiply the current scale by factor (clamped), anchored at anchorViewportPos.
        void zoomBy(qreal factor, const QPoint& anchorViewportPos);

        //! Zoom to baselineScale()*factor (clamped), anchored at anchorViewportPos.
        void setZoomFactor(qreal factor, const QPoint& anchorViewportPos);

        //! Run fn (e.g. a host's scene->setSceneRect() call) while keeping the scene point
        //! currently at the viewport centre pinned there afterwards -- guards against a scene rect
        //! change (a higher-resolution pixmap arriving asynchronously while zoomed in) jumping the
        //! visible area.
        void keepViewportCenter(const std::function<void()>& fn);

        void setStepFactor(qreal value) noexcept;
        qreal stepFactor() const noexcept;

        void setMinZoomFactor(qreal value) noexcept;
        qreal minZoomFactor() const noexcept;

        void setMaxZoomFactor(qreal value) noexcept;
        qreal maxZoomFactor() const noexcept;

        void setPixelsPerNotch(qreal value) noexcept;
        qreal pixelsPerNotch() const noexcept;

        //! Master switch for drag-to-pan; wheel/gesture zoom is unaffected. Default true.
        void setPanEnabled(bool value);
        bool isPanEnabled() const noexcept;

        //! Which buttons start a pan drag. Default Qt::LeftButton.
        void setPanButtons(Qt::MouseButtons buttons) noexcept;
        Qt::MouseButtons panButtons() const noexcept;

        //! Veto hook consulted on every candidate pan press, in viewport coordinates -- return false
        //! to let the press fall through to the view/scene untouched (e.g. the editor vetoes while
        //! freehand draw is enabled or the press lands on a crop handle). No filter (default) means
        //! every press that isPannable() may start a pan.
        void setPanFilter(std::function<bool(const QPoint&)> filter);

        //! When false (default true), this never touches viewport()->setCursor()/unsetCursor() --
        //! required by a host (ImageViewer) whose own hover logic already owns the viewport cursor,
        //! to avoid the two fighting over it on every mouse move.
        void setCursorManaged(bool value) noexcept;
        bool isCursorManaged() const noexcept;

    public slots:

        //! Step in/out by stepFactor(), anchored at the viewport centre. Same step the wheel path
        //! uses, so toolbar buttons and wheel/gesture zoom feel identical.
        void zoomIn();
        void zoomOut();

        //! Back to baselineScale() -- equivalent to fitToItem() plus a zoomChanged() notification.
        void resetZoom();

    signals:

        void zoomChanged(qreal zoomFactor);

        //! Fires on every pan step (not just at drag end).
        void panned();

    protected:

        bool eventFilter(QObject* watched, QEvent* event) override;

    private:

        qreal clampScale(qreal scale) const;
        void beginPan(const QPoint& pos);
        void endPan();

        QGraphicsView* m_view;
        QGraphicsItem* m_fitItem=nullptr;
        bool m_fitOnlyIfLarger=true;

        qreal m_stepFactor=DefaultStepFactor;
        qreal m_minZoomFactor=DefaultMinZoomFactor;
        qreal m_maxZoomFactor=DefaultMaxZoomFactor;
        qreal m_pixelsPerNotch=DefaultPixelsPerNotch;

        bool m_panEnabled=true;
        Qt::MouseButtons m_panButtons=Qt::LeftButton;
        std::function<bool(const QPoint&)> m_panFilter;
        bool m_cursorManaged=true;

        bool m_panning=false;
        QPoint m_lastPanPos;

        bool m_gestureActive=false;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_GRAPHICSVIEWZOOM_HPP
