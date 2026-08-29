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

/** @file uise/desktop/src/graphicsviewzoom.cpp
*
*  Defines GraphicsViewZoom.
*
*/

/****************************************************************************/

#include <cmath>
#include <algorithm>

#include <QGraphicsView>
#include <QGraphicsItem>
#include <QScrollBar>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QNativeGestureEvent>

#include <uise/desktop/utils/graphicsviewzoom.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

// zoomFactor() values within this of 1.0 count as "not zoomed" -- guards isZoomed()/isPannable()
// consumers (e.g. ImageViewerWidget::isInPrevNavigationZone()) against floating-point noise left
// over from a zoomTo()/zoomBy() call that landed exactly on the clamp.
constexpr qreal ZoomEpsilon=1e-3;

// Qt maps the macOS Command key onto Qt::ControlModifier by default, so a single ControlModifier
// check covers Ctrl on Windows/Linux and Cmd on macOS. MetaModifier is accepted too so this keeps
// working in an application that sets Qt::AA_MacDontSwapCtrlAndMeta (which routes Cmd to
// MetaModifier instead). Mirrors calendar.cpp's own file-local hasControlModifier() -- kept as a
// separate copy rather than shared between the two translation units.
bool hasZoomModifier(Qt::KeyboardModifiers mods) noexcept
{
    return mods.testFlag(Qt::ControlModifier) || mods.testFlag(Qt::MetaModifier);
}

}

//--------------------------------------------------------------------------

GraphicsViewZoom::GraphicsViewZoom(
        QGraphicsView* view,
        QObject* parent
    ) : QObject(parent),
        m_view(view)
{
    if (m_view!=nullptr && m_view->viewport()!=nullptr)
    {
        m_view->viewport()->installEventFilter(this);
    }
}

//--------------------------------------------------------------------------

QGraphicsView* GraphicsViewZoom::view() const noexcept
{
    return m_view;
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::setFitItem(QGraphicsItem* item)
{
    if (item==m_fitItem)
    {
        return;
    }
    m_fitItem=item;

    // A genuinely different item (navigating to another image, or unload/reset) has no relation
    // to whatever zoom the PREVIOUS item was at -- unlike a host re-calling setFitItem() with the
    // SAME item on every producer-driven pixmap update (a version-ladder upgrade), which must
    // leave a deliberate user zoom alone.
    m_userZoomed=false;
}

//--------------------------------------------------------------------------

QGraphicsItem* GraphicsViewZoom::fitItem() const noexcept
{
    return m_fitItem;
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::setFitOnlyIfLarger(bool value)
{
    m_fitOnlyIfLarger=value;
}

//--------------------------------------------------------------------------

bool GraphicsViewZoom::isFitOnlyIfLarger() const noexcept
{
    return m_fitOnlyIfLarger;
}

//--------------------------------------------------------------------------

qreal GraphicsViewZoom::currentScale() const
{
    if (m_view==nullptr)
    {
        return 1.0;
    }
    const auto& t=m_view->transform();
    auto s=std::sqrt(t.m11()*t.m11()+t.m12()*t.m12());
    if (qFuzzyIsNull(s))
    {
        return 1.0;
    }
    return s;
}

//--------------------------------------------------------------------------

bool GraphicsViewZoom::naturalViewSize(QSizeF& out) const
{
    if (m_view==nullptr || m_fitItem==nullptr || m_view->viewport()==nullptr)
    {
        return false;
    }

    auto s=currentScale();

    // Mapping the item's own (rotation/flip-inclusive, but never zoom-inclusive) scene bounding
    // rect through the CURRENT transform and dividing back out by the current scale s yields a
    // result independent of s by construction -- this is what lets both baselineScale() and the
    // minDisplayPixels() floor serve ImageViewer (rotation lives in the view transform, item itself
    // unrotated) and SimpleImageEditor (rotation lives in the item group's own transform, view
    // transform carries only zoom) alike: sceneBoundingRect() already reflects whichever of the two
    // applies, and mapRect() naturally swaps width/height for a 90-degree rotation either way.
    auto mapped=m_view->transform().mapRect(m_fitItem->sceneBoundingRect());
    if (mapped.isEmpty() || qFuzzyIsNull(s))
    {
        return false;
    }

    out=QSizeF(mapped.width()/s,mapped.height()/s);
    return true;
}

//--------------------------------------------------------------------------

qreal GraphicsViewZoom::baselineScale() const
{
    if (m_view==nullptr || m_fitItem==nullptr || m_view->viewport()==nullptr)
    {
        return 1.0;
    }

    auto viewportRect=m_view->viewport()->rect();
    if (viewportRect.isEmpty())
    {
        return 1.0;
    }

    QSizeF natural;
    if (!naturalViewSize(natural))
    {
        // naturalViewSize() only fails here because the mapped bounding rect is empty (m_view/
        // m_fitItem/viewport null was already ruled out above) -- same fallback the pre-extraction
        // code used for that case.
        return currentScale();
    }

    auto fit=std::min(
        static_cast<qreal>(viewportRect.width())/natural.width(),
        static_cast<qreal>(viewportRect.height())/natural.height()
    );
    if (m_fitOnlyIfLarger)
    {
        fit=std::min(fit,1.0);
    }
    if (qFuzzyIsNull(fit) || fit<0)
    {
        return 1.0;
    }
    return fit;
}

//--------------------------------------------------------------------------

qreal GraphicsViewZoom::zoomFactor() const
{
    auto base=baselineScale();
    if (qFuzzyIsNull(base))
    {
        return 1.0;
    }
    return currentScale()/base;
}

//--------------------------------------------------------------------------

bool GraphicsViewZoom::isZoomed() const
{
    return zoomFactor()>1.0+ZoomEpsilon;
}

//--------------------------------------------------------------------------

bool GraphicsViewZoom::isUserZoomed() const noexcept
{
    return m_userZoomed;
}

//--------------------------------------------------------------------------

bool GraphicsViewZoom::isPannable() const
{
    if (m_view==nullptr)
    {
        return false;
    }
    auto* h=m_view->horizontalScrollBar();
    auto* v=m_view->verticalScrollBar();
    return (h!=nullptr && h->maximum()>h->minimum())
           || (v!=nullptr && v->maximum()>v->minimum());
}

//--------------------------------------------------------------------------

bool GraphicsViewZoom::isPanning() const noexcept
{
    return m_panning;
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::fitToItem()
{
    if (m_view==nullptr || m_fitItem==nullptr || m_view->viewport()==nullptr)
    {
        return;
    }

    auto viewportRect=m_view->viewport()->rect();
    if (viewportRect.isEmpty())
    {
        return;
    }

    // Actually re-fitting -- whatever deliberate zoom the user had is gone now.
    m_userZoomed=false;

    // Normalize the transform's scale magnitude to 1.0 first, mirroring the first step of
    // QGraphicsView::fitInView() itself -- this PRESERVES whatever rotation/flip is already in the
    // transform (a bare resetTransform() would silently discard it too, which is the latent bug
    // this fixes: resizing the window after rotating a small/already-fitting image used to
    // un-rotate it).
    auto s=currentScale();
    if (!qFuzzyIsNull(s) && !qFuzzyCompare(s,1.0))
    {
        m_view->scale(1.0/s,1.0/s);
    }

    auto natural=m_view->transform().mapRect(m_fitItem->sceneBoundingRect());
    bool larger=natural.width()>viewportRect.width() || natural.height()>viewportRect.height();
    if (larger)
    {
        m_view->fitInView(m_fitItem,Qt::KeepAspectRatio);
    }
    // else: already normalized to unit magnitude above, rotation/flip preserved -- nothing more.
}

//--------------------------------------------------------------------------

qreal GraphicsViewZoom::clampScale(qreal scale) const
{
    auto base=baselineScale();
    auto minS=base*m_minZoomFactor;

    if (m_minDisplayPixels>0.0)
    {
        QSizeF natural;
        if (naturalViewSize(natural))
        {
            auto naturalMin=std::min(natural.width(),natural.height());
            if (naturalMin>0.0)
            {
                // Lowers minS, never raises it -- a degenerate naturalViewSize() (handled above by
                // simply not entering this branch) or a naturalMin of 0 leaves today's
                // minZoomFactor()-relative floor untouched. Capped at base: the pixel floor may only
                // ever pull the reachable minimum below fit, never push it above fit -- an image
                // that already displays smaller than minDisplayPixels() at fit must stay at fit,
                // never be forced to zoom IN to reach the floor.
                minS=std::min(minS,std::min(m_minDisplayPixels/naturalMin,base));
            }
        }
    }

    auto maxS=base*m_maxZoomFactor;
    if (minS>maxS)
    {
        std::swap(minS,maxS);
    }
    return qBound(minS,scale,maxS);
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::zoomTo(qreal absoluteScale, const QPoint& anchorViewportPos)
{
    if (m_view==nullptr)
    {
        return;
    }

    auto current=currentScale();
    if (qFuzzyIsNull(current))
    {
        return;
    }

    auto base=baselineScale();
    auto clamped=clampScale(absoluteScale);
    auto factor=clamped/current;
    if (qFuzzyCompare(factor,1.0))
    {
        return;
    }

    // Manual scrollbar math with the anchor scoped to NoAnchor for just this call, rather than
    // setTransformationAnchor(AnchorUnderMouse) -- that relies on QGraphicsView's own last-mouse-
    // move scene point, which is stale for a pinch/wheel event delivered with a stationary cursor.
    // Scoping NoAnchor here leaves rotate()/flipHorizontal()'s existing AnchorViewCenter behaviour,
    // set by the host elsewhere, completely untouched.
    const auto oldAnchor=m_view->transformationAnchor();
    m_view->setTransformationAnchor(QGraphicsView::NoAnchor);
    const auto sceneAnchor=m_view->mapToScene(anchorViewportPos);
    m_view->scale(factor,factor);
    const auto delta=m_view->mapFromScene(sceneAnchor)-anchorViewportPos;
    if (m_view->horizontalScrollBar()!=nullptr)
    {
        m_view->horizontalScrollBar()->setValue(m_view->horizontalScrollBar()->value()+delta.x());
    }
    if (m_view->verticalScrollBar()!=nullptr)
    {
        m_view->verticalScrollBar()->setValue(m_view->verticalScrollBar()->value()+delta.y());
    }
    m_view->setTransformationAnchor(oldAnchor);

    // Reflects intent, not just position: a deliberate zoom OUT below the baseline (reachable once
    // minDisplayPixels() is set) counts as user-zoomed too, same as a zoom in -- isZoomed() alone
    // would miss it, which is exactly the regression that would let ImageViewer::fitImage() snap a
    // deliberate zoom-out back to fit on the next viewport resize or async pixmap upgrade. Compared
    // against the baseline already computed above (not re-derived from the transform after scale())
    // to avoid a float round-trip that could flake right at the clamp.
    m_userZoomed=!qFuzzyIsNull(base) && qAbs(clamped-base)>base*ZoomEpsilon;

    emit zoomChanged(zoomFactor());
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::zoomBy(qreal factor, const QPoint& anchorViewportPos)
{
    if (!std::isfinite(factor) || factor<=0)
    {
        return;
    }
    zoomTo(currentScale()*factor,anchorViewportPos);
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::setZoomFactor(qreal factor, const QPoint& anchorViewportPos)
{
    zoomTo(baselineScale()*factor,anchorViewportPos);
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::keepViewportCenter(const std::function<void()>& fn)
{
    if (!fn)
    {
        return;
    }
    if (m_view==nullptr || m_view->viewport()==nullptr)
    {
        fn();
        return;
    }

    auto center=m_view->viewport()->rect().center();
    auto sceneCenter=m_view->mapToScene(center);

    fn();

    auto delta=m_view->mapFromScene(sceneCenter)-center;
    if (delta.isNull())
    {
        return;
    }
    if (m_view->horizontalScrollBar()!=nullptr)
    {
        m_view->horizontalScrollBar()->setValue(m_view->horizontalScrollBar()->value()+delta.x());
    }
    if (m_view->verticalScrollBar()!=nullptr)
    {
        m_view->verticalScrollBar()->setValue(m_view->verticalScrollBar()->value()+delta.y());
    }
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::setStepFactor(qreal value) noexcept
{
    m_stepFactor=value;
}

qreal GraphicsViewZoom::stepFactor() const noexcept
{
    return m_stepFactor;
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::setMinZoomFactor(qreal value) noexcept
{
    m_minZoomFactor=value;
}

qreal GraphicsViewZoom::minZoomFactor() const noexcept
{
    return m_minZoomFactor;
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::setMaxZoomFactor(qreal value) noexcept
{
    m_maxZoomFactor=value;
}

qreal GraphicsViewZoom::maxZoomFactor() const noexcept
{
    return m_maxZoomFactor;
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::setPixelsPerNotch(qreal value) noexcept
{
    m_pixelsPerNotch=value;
}

qreal GraphicsViewZoom::pixelsPerNotch() const noexcept
{
    return m_pixelsPerNotch;
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::setMinDisplayPixels(qreal value) noexcept
{
    m_minDisplayPixels=value;
}

qreal GraphicsViewZoom::minDisplayPixels() const noexcept
{
    return m_minDisplayPixels;
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::setPanEnabled(bool value)
{
    m_panEnabled=value;
    if (!value && m_panning)
    {
        endPan();
    }
}

bool GraphicsViewZoom::isPanEnabled() const noexcept
{
    return m_panEnabled;
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::setPanButtons(Qt::MouseButtons buttons) noexcept
{
    m_panButtons=buttons;
}

Qt::MouseButtons GraphicsViewZoom::panButtons() const noexcept
{
    return m_panButtons;
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::setPanFilter(std::function<bool(const QPoint&)> filter)
{
    m_panFilter=std::move(filter);
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::setCursorManaged(bool value) noexcept
{
    m_cursorManaged=value;
}

bool GraphicsViewZoom::isCursorManaged() const noexcept
{
    return m_cursorManaged;
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::zoomIn()
{
    if (m_view==nullptr || m_view->viewport()==nullptr)
    {
        return;
    }
    zoomBy(m_stepFactor,m_view->viewport()->rect().center());
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::zoomOut()
{
    if (m_view==nullptr || m_view->viewport()==nullptr)
    {
        return;
    }
    zoomBy(1.0/m_stepFactor,m_view->viewport()->rect().center());
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::resetZoom()
{
    fitToItem();
    emit zoomChanged(zoomFactor());
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::beginPan(const QPoint& pos)
{
    m_panning=true;
    m_lastPanPos=pos;
    if (m_cursorManaged && m_view!=nullptr && m_view->viewport()!=nullptr)
    {
        m_view->viewport()->setCursor(Qt::ClosedHandCursor);
    }
}

//--------------------------------------------------------------------------

void GraphicsViewZoom::endPan()
{
    m_panning=false;
    if (m_cursorManaged && m_view!=nullptr && m_view->viewport()!=nullptr)
    {
        m_view->viewport()->unsetCursor();
    }
}

//--------------------------------------------------------------------------

bool GraphicsViewZoom::eventFilter(QObject* watched, QEvent* event)
{
    if (m_view==nullptr || m_view->viewport()==nullptr || watched!=m_view->viewport())
    {
        return QObject::eventFilter(watched,event);
    }

    switch (event->type())
    {
        case QEvent::NativeGesture:
        {
            auto* g=static_cast<QNativeGestureEvent*>(event);
            if (g->gestureType()==Qt::ZoomNativeGesture)
            {
                // value() is macOS's incremental NSEvent.magnification delta -- clamp per-event
                // to defend against an unexpectedly large or non-finite value rather than trust it
                // blindly (see the platform-matrix confidence notes in the design doc/plan).
                auto factor=1.0+g->value();
                if (std::isfinite(factor))
                {
                    factor=qBound(0.5,factor,2.0);
                    zoomBy(factor,g->position().toPoint());
                }
                event->accept();
                return true;
            }
            if (g->gestureType()==Qt::BeginNativeGesture)
            {
                m_gestureActive=true;
                event->accept();
                return true;
            }
            if (g->gestureType()==Qt::EndNativeGesture)
            {
                m_gestureActive=false;
                event->accept();
                return true;
            }
            break;
        }

        case QEvent::Wheel:
        {
            auto* w=static_cast<QWheelEvent*>(event);
            if (!hasZoomModifier(w->modifiers()))
            {
                // Not our event -- QGraphicsView keeps scrolling/panning with it exactly as
                // before, which is also how a trackpad user gets two-finger-scroll panning for
                // free without any pan-specific code here.
                break;
            }
            auto pixelDelta=w->pixelDelta();
            auto angleDelta=w->angleDelta();
            qreal factor;
            if (pixelDelta.y()!=0)
            {
                factor=std::pow(m_stepFactor,pixelDelta.y()/m_pixelsPerNotch);
            }
            else if (angleDelta.y()!=0)
            {
                factor=std::pow(m_stepFactor,angleDelta.y()/120.0);
            }
            else
            {
                break;
            }
            zoomBy(factor,w->position().toPoint());
            event->accept();
            return true;
        }

        case QEvent::MouseButtonPress:
        {
            // Never consumed -- see the class doc. Only bookkeeping for a possible pan.
            if (m_panEnabled && !m_panning)
            {
                auto* m=static_cast<QMouseEvent*>(event);
                if ((m_panButtons & m->button()) && isPannable())
                {
                    auto pos=m->pos();
                    if (!m_panFilter || m_panFilter(pos))
                    {
                        beginPan(pos);
                    }
                }
            }
            break;
        }

        case QEvent::MouseMove:
        {
            // Never consumed -- see the class doc.
            if (m_panning)
            {
                auto* m=static_cast<QMouseEvent*>(event);
                if (!(m->buttons() & m_panButtons))
                {
                    // The button-release event itself can be lost if it happens outside the
                    // viewport mid-drag -- a move that no longer reports the pan button down is
                    // treated the same as an explicit release.
                    endPan();
                }
                else
                {
                    auto pos=m->pos();
                    auto delta=pos-m_lastPanPos;
                    m_lastPanPos=pos;
                    if (m_view->horizontalScrollBar()!=nullptr)
                    {
                        m_view->horizontalScrollBar()->setValue(m_view->horizontalScrollBar()->value()-delta.x());
                    }
                    if (m_view->verticalScrollBar()!=nullptr)
                    {
                        m_view->verticalScrollBar()->setValue(m_view->verticalScrollBar()->value()-delta.y());
                    }
                    emit panned();
                }
            }
            break;
        }

        case QEvent::MouseButtonRelease:
        {
            // Never consumed -- see the class doc.
            if (m_panning)
            {
                auto* m=static_cast<QMouseEvent*>(event);
                if (m_panButtons & m->button())
                {
                    endPan();
                }
            }
            break;
        }

        default:
            break;
    }

    return QObject::eventFilter(watched,event);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
