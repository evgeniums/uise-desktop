/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/imagecropper.cpp
*
*  Defines CropRectItem.
*
*/

/****************************************************************************/

#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QCursor>
#include <QGraphicsView>

#include <uise/desktop/imagecropper.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

// Sub-pixel differences are noise, not a real frame move -- gates frameChanged() so a live window
// resize (many resizeEvent()s per second, each calling adjustCropRect()) doesn't re-trigger
// GraphicsViewZoom::reapplyLimits() (and therefore a real scale() call) on every tick when the
// viewport-clamped frame lands back in the same place anyway.
bool rectsNearlyEqual(const QRectF& a, const QRectF& b)
{
    constexpr qreal Eps=0.5;
    return qAbs(a.left()-b.left())<Eps && qAbs(a.top()-b.top())<Eps
           && qAbs(a.right()-b.right())<Eps && qAbs(a.bottom()-b.bottom())<Eps;
}

}

/****************************** CropRectItem *****************************/

//--------------------------------------------------------------------------

CropRectItem::CropRectItem(QGraphicsView* view, QGraphicsPixmapItem* imageItem, QGraphicsItem *parent)
    : QObject(),
      QGraphicsRectItem(imageItem->boundingRect(),parent),
      m_activeHandle(NoHandle),
      m_imageItem(imageItem),
      m_square(false),
      m_ellipse(false),
      m_keepAspectRatio(true),
      m_view(view)
{    
}

//--------------------------------------------------------------------------

void CropRectItem::init()
{
    m_cropperRect=m_imageItem->boundingRect();
    updateAspectRatio();
    adjustCropRect();

    // Enable hover events to change cursor shape
    setAcceptHoverEvents(true);

    // Set flags for movement, but we'll handle actual movement and resizing manually
    // because we want custom handles.
    setFlags(ItemIsMovable | ItemSendsGeometryChanges);

    setZValue(1);
    update();
}

//--------------------------------------------------------------------------

QRectF CropRectItem::getCropAreaCoordinates() const
{
    return mapToScene(m_cropperRect).boundingRect();
}

//--------------------------------------------------------------------------

void CropRectItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    if (m_imageItem==nullptr)
    {
        return;
    }

    QColor backgroundColor(0, 0, 0, 128);
    auto sz=boundingRect().size().toSize();
    QPixmap px(sz);
    QPainter p;
    px.fill(Qt::transparent);
    p.begin(&px);
    p.setRenderHints(QPainter::Antialiasing);
    p.fillRect(rect(), backgroundColor);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::transparent);
    if (m_ellipse)
    {
        p.drawEllipse(m_cropperRect);
    }
    else
    {
        p.drawRect(m_cropperRect);
    }
    p.end();
    painter->setRenderHints(QPainter::SmoothPixmapTransform);
    painter->drawPixmap(0,0,sz.width(),sz.height(),px);

    const auto& r=m_cropperRect;

    auto transform = m_view->transform();
    auto scale_x = qSqrt(transform.m11() * transform.m11() + transform.m12() * transform.m12());

    // calculate width of handle
    qreal handleSize = BaseHandleWidth;
    QRect portRect = m_view->viewport()->rect();
    if (m_fixedOnScreen || sz.width()>portRect.width() || sz.height()>portRect.height())
    {
        if (scale_x>0)
        {
            handleSize=handleSize/scale_x;
        }
    }

    // draw the crop border
    QColor borderColor{BorderColor};
    borderColor.setAlpha(BorderColorAlpha);
    QPen borderPen(borderColor);
    borderPen.setWidth(qRound(handleSize));
    borderPen.setStyle(Qt::DashLine);
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(r);

    // draw the corner handles
    auto cornerHandleSize=handleSize*20;
    auto cornerWidth=handleSize*4;
    if (r.width()<64)
    {
        cornerHandleSize=handleSize*4;
        cornerWidth=2;
    }
    QColor handleColor{HandleColor};
    QPen pen{handleColor};
    pen.setWidth(cornerWidth);
    painter->setPen(pen);

    // draw corner handles
    painter->drawLine(r.topLeft().x(),r.topLeft().y(),r.topLeft().x()+cornerHandleSize,r.topLeft().y());
    painter->drawLine(r.topRight().x()-cornerHandleSize,r.topLeft().y(),r.topRight().x(),r.topRight().y());
    painter->drawLine(r.bottomLeft().x(),r.bottomLeft().y(),r.bottomLeft().x()+cornerHandleSize,r.bottomLeft().y());
    painter->drawLine(r.bottomRight().x()-cornerHandleSize,r.bottomLeft().y(),r.bottomRight().x(),r.bottomRight().y());

    painter->drawLine(r.topLeft().x(),r.topLeft().y(),r.topLeft().x(),r.topLeft().y()+cornerHandleSize);
    painter->drawLine(r.topRight().x(),r.topLeft().y(),r.topRight().x(),r.topRight().y()+cornerHandleSize);
    painter->drawLine(r.bottomLeft().x(),r.bottomLeft().y()-cornerHandleSize,r.bottomLeft().x(),r.bottomLeft().y());
    painter->drawLine(r.bottomRight().x(),r.bottomLeft().y()-cornerHandleSize,r.bottomRight().x(),r.bottomRight().y());
}

//--------------------------------------------------------------------------

CropRectItem::HandleType CropRectItem::getHandleType(QPointF pos, bool forCursor) const
{
    QRectF r = m_cropperRect;
    auto t=m_view->transform();
    if (forCursor && (t.isRotating()||t.isScaling()))
    {
        t=t.inverted();
        pos=t.map(pos);
        r=t.mapRect(r).toRect();
    }

    qreal handleTolerance = BaseHandleTolerance; // Area around handles to detect click
    auto transform = m_view->transform();
    auto scale_x = qSqrt(transform.m11() * transform.m11() + transform.m12() * transform.m12());
    QRect portRect = m_view->viewport()->rect();
    if (scale_x>0 && (m_fixedOnScreen || r.width()>portRect.width() || r.height()>portRect.height()))
    {
        handleTolerance=handleTolerance/scale_x;
    }

    // Corners
    if (QRectF(r.topLeft() - QPointF(handleTolerance/2, handleTolerance/2), QSizeF(handleTolerance, handleTolerance)).contains(pos)) return TopLeft;
    if (QRectF(r.topRight() - QPointF(handleTolerance/2, handleTolerance/2), QSizeF(handleTolerance, handleTolerance)).contains(pos)) return TopRight;
    if (QRectF(r.bottomLeft() - QPointF(handleTolerance/2, handleTolerance/2), QSizeF(handleTolerance, handleTolerance)).contains(pos)) return BottomLeft;
    if (QRectF(r.bottomRight() - QPointF(handleTolerance/2, handleTolerance/2), QSizeF(handleTolerance, handleTolerance)).contains(pos)) return BottomRight;

    // Edges (simplified detection)
    if (pos.y() > r.top() - 5 && pos.y() < r.top() + 5 && pos.x() > r.left() + handleTolerance && pos.x() < r.right() - handleTolerance) return TopEdge;
    if (pos.y() < r.bottom() + 5 && pos.y() > r.bottom() - 5 && pos.x() > r.left() + handleTolerance && pos.x() < r.right() - handleTolerance) return BottomEdge;
    if (pos.x() > r.left() - 5 && pos.x() < r.left() + 5 && pos.y() > r.top() + handleTolerance && pos.y() < r.bottom() - handleTolerance) return LeftEdge;
    if (pos.x() < r.right() + 5 && pos.x() > r.right() - 5 && pos.y() > r.top() + handleTolerance && pos.y() < r.bottom() - handleTolerance) return RightEdge;

    // Move
    if (r.contains(pos)) return Move;

    return NoHandle;
}

//--------------------------------------------------------------------------

void CropRectItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    HandleType handle = getHandleType(event->pos(),true);
    switch (handle) {
    case TopLeft:
    case BottomRight:
        setCursor(Qt::SizeFDiagCursor); break;
    case TopRight:
    case BottomLeft:
        setCursor(Qt::SizeBDiagCursor); break;
    case TopEdge:
    case BottomEdge:
        setCursor(Qt::SizeVerCursor); break;
    case LeftEdge:
    case RightEdge:
        setCursor(Qt::SizeHorCursor); break;
    case Move:
        setCursor(Qt::SizeAllCursor); break;
    default:
        setCursor(Qt::ArrowCursor); break;
    }
    QGraphicsRectItem::hoverMoveEvent(event);
}

//--------------------------------------------------------------------------

void CropRectItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_activeHandle = getHandleType(event->pos());
        m_lastPos = event->pos();
        if (m_activeHandle == Move)
        {
            // If moving, we let QGraphicsRectItem handle the initial press for movement
            QGraphicsRectItem::mousePressEvent(event);
            return;
        }
        if (m_activeHandle != NoHandle)
        {
            return;
        }
    }

    // Release the grab for a press outside the crop rect (the dimmed margin) or with a button
    // other than left -- without this, ItemIsMovable (see init()) plus the accepted-by-default
    // QGraphicsSceneMouseEvent makes this item the scene's mouse grabber for ANY press anywhere
    // on the image, not just one that actually lands on the rect/a handle. The stray grab was
    // previously harmless in practice (mouseMoveEvent()/mouseReleaseEvent() both no-op while
    // m_activeHandle==NoHandle), but there is no reason to hold it, and a host adding its own
    // pan/gesture handling on the view (see GraphicsViewZoom) is exactly the kind of caller that
    // should not have to rely on that no-op guard to stay correct.
    event->ignore();
}

//--------------------------------------------------------------------------

void CropRectItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activeHandle == NoHandle) return;

    QRectF currentRect = m_cropperRect;
    QPointF delta = event->pos() - m_lastPos;

    qreal x = currentRect.x();
    qreal y = currentRect.y();
    qreal w = currentRect.width();
    qreal h = currentRect.height();

    // Keep track of the original bounds of the image for clamping
    QRectF imageBounds = boundingRect();
    auto updateRect = [&](qreal newX, qreal newY, qreal newW, qreal newH)
    {
        // Apply new position and size, ensuring non-negative width/height
        if (newW > m_minWidth && newH > m_minHeight
            && newX>=0
            && newY>=0
            && (newX+newW)<=imageBounds.right()
            && (newY+newH)<=imageBounds.bottom()
            )
        {
            m_cropperRect=QRectF(newX, newY, newW, newH);
            if (m_fixedOnScreen)
            {
                syncViewportFrameFromCropperRect();
            }
        }
    };

    switch (m_activeHandle) {
    case Move:
        // move image
        updateRect(x + delta.x(), y + delta.y(), w, h);
        break;

        // --- Resizing Handles ---
    case TopLeft:
        if (keepAspectRatio())
        {
            delta.setY(delta.x()/m_xyAspectRatio);
        }
        updateRect(x + delta.x(), y + delta.y(), w - delta.x(), h - delta.y());
        break;
    case TopRight:
        if (keepAspectRatio())
        {
            w=(h - delta.y())*m_xyAspectRatio;
            updateRect(x, y + delta.y(), w, h - delta.y());
        }
        else
        {
            updateRect(x, y + delta.y(), w + delta.x(), h - delta.y());
        }
        break;
    case BottomLeft:
        if (keepAspectRatio())
        {
            h=(w - delta.x())/m_xyAspectRatio;
            updateRect(x + delta.x(), y, w - delta.x(), h);
        }
        else
        {
            updateRect(x + delta.x(), y, w - delta.x(), h + delta.y());
        }
        break;
    case BottomRight:
        if (keepAspectRatio())
        {
            delta.setX(delta.y()*m_xyAspectRatio);
        }
        updateRect(x, y, w + delta.x(), h + delta.y());
        break;
    case TopEdge:
        if (keepAspectRatio())
        {
            delta.setX(delta.y()*m_xyAspectRatio);
            updateRect(x + delta.x(), y + delta.y(), w - delta.x(), h - delta.y());
        }
        else
        {
            updateRect(x, y + delta.y(), w, h - delta.y());
        }
        break;
    case BottomEdge:
        if (keepAspectRatio())
        {
            delta.setX(delta.y()*m_xyAspectRatio);
            updateRect(x, y, w + delta.x(), h + delta.y());
        }
        else
        {
            updateRect(x, y, w, h + delta.y());
        }
        break;
    case LeftEdge:
        if (keepAspectRatio())
        {
            delta.setY(delta.x()/m_xyAspectRatio);
            updateRect(x + delta.x(), y + delta.y(), w - delta.x(), h - delta.y());
        }
        else
        {
            updateRect(x + delta.x(), y, w - delta.x(), h);
        }
        break;
    case RightEdge:
        if (keepAspectRatio())
        {
            delta.setY(delta.x()/m_xyAspectRatio);
            updateRect(x, y, w + delta.x(), h + delta.y());
        }
        else
        {
            updateRect(x, y, w + delta.x(), h);
        }
        break;
    default:
        break;
    }

    m_lastPos = event->pos();
    update(); // Force repaint
}

//--------------------------------------------------------------------------

void CropRectItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activeHandle != NoHandle)
    {
        m_activeHandle = NoHandle;
        // If it was a Move operation, let base class handle the release.
        if (getHandleType(event->pos()) == Move)
        {
            QGraphicsRectItem::mouseReleaseEvent(event);
        }
    }
}

//--------------------------------------------------------------------------

void CropRectItem::adjustCropRect()
{
    if (m_imageItem==nullptr)
    {
        return;
    }

    if (m_fixedOnScreen)
    {
        adjustViewportFrame();
        return;
    }

    QRectF imageBoundsScene = m_imageItem->mapToScene(m_imageItem->boundingRect()).boundingRect();
    QRectF imageBounds=mapRectFromScene(imageBoundsScene);

    if (m_limitToVisibleArea)
    {
        QRect portRect = m_view->viewport()->rect();
        QRectF sceneRect = m_view->mapToScene(portRect).boundingRect();
        QRectF viewBounds = mapRectFromScene(sceneRect);

        auto left=std::max(imageBounds.x(),viewBounds.x());
        auto top=std::max(imageBounds.y(),viewBounds.y());
        auto right=std::min(imageBounds.right(),viewBounds.right());
        auto bottom=std::min(imageBounds.bottom(),viewBounds.bottom());
        m_cropperRect=QRectF{QPointF{left,top},QPointF{right,bottom}};
    }
    else
    {
        // The host view is zoomed in (see setLimitToVisibleArea()'s doc) -- intersecting with
        // only the currently visible viewport would collapse the crop rect down to whatever
        // sliver of the image happens to be on screen right now, so use the full image bounds
        // instead.
        m_cropperRect=imageBounds;
    }

    if (m_square || m_keepAspectRatio)
    {
        auto newRatio=m_cropperRect.width()/m_cropperRect.height();
        if (!qFuzzyCompare(m_xyAspectRatio,newRatio))
        {
            qreal newX;
            qreal newY;
            qreal newW;
            qreal newH;

            if (newRatio>m_xyAspectRatio)
            {
                // decrease width
                newW=m_cropperRect.height()*m_xyAspectRatio;
                auto deltaX=(qRound(m_cropperRect.width())-newW)/2;
                newX=m_cropperRect.x()+deltaX;
                newY=m_cropperRect.y();
                newH=m_cropperRect.height();
            }
            else
            {
                // decrease height
                newH=m_cropperRect.width()/m_xyAspectRatio;
                auto deltaY=(qRound(m_cropperRect.height())-newH)/2;
                newY=m_cropperRect.y()+deltaY;
                newX=m_cropperRect.x();
                newW=m_cropperRect.width();
            }

             m_cropperRect=QRectF{newX,newY,newW,newH};
        }
    }
}

//--------------------------------------------------------------------------

void CropRectItem::syncToView()
{
    if (!m_fixedOnScreen || m_view==nullptr || m_viewportFrame.isEmpty())
    {
        return;
    }

    // Pure QTransform composition, deliberately NOT QGraphicsView::mapToScene(QRect)/
    // QGraphicsItem::mapFromScene(QRectF) -- see syncViewportFrameFromCropperRect()'s own comment
    // for why the QPolygon(F)-returning overloads round to integer VIEWPORT pixels and must not be
    // used on a value (m_viewportFrame) that flows back out into scene coordinates here.
    auto sceneRect=m_view->viewportTransform().inverted().mapRect(m_viewportFrame);
    m_cropperRect=mapRectFromScene(sceneRect);
    update();
}

//--------------------------------------------------------------------------

void CropRectItem::adjustViewportFrame()
{
    if (m_view==nullptr || m_view->viewport()==nullptr || m_imageItem==nullptr)
    {
        return;
    }

    QRectF available;
    QPointF center;
    if (m_viewportFrame.isEmpty())
    {
        // First-time seed: the full viewport, centred. Deliberately NOT intersected with the
        // image's own (possibly tiny, pre-upscale) on-screen bounds the way the legacy body's
        // "limit to visible area" is -- the frame defines the output window independently of
        // whatever pixel size the source image happens to be; GraphicsViewZoom::setCoverRect()
        // (see SimpleImageEditor::updateZoomLimitsForCropper()) is what scales a smaller image up
        // to fill it, and seeding from the image's pre-upscale bounds would produce a frame no
        // bigger than the un-zoomed image, defeating that.
        available=QRectF(m_view->viewport()->rect());
        center=available.center();
    }
    else
    {
        // Already active -- e.g. a crop-shape or aspect-ratio change while the user has the editor
        // open. Keep the frame's own current centre and footprint as the budget instead of
        // recentring on the image, so an in-progress crop selection is not discarded.
        available=m_viewportFrame;
        center=m_viewportFrame.center();
    }

    qreal w=available.width();
    qreal h=available.height();
    if (keepAspectRatio() && m_xyAspectRatio>0 && h>0)
    {
        auto ratio=w/h;
        if (!qFuzzyCompare(ratio,m_xyAspectRatio))
        {
            if (ratio>m_xyAspectRatio)
            {
                w=h*m_xyAspectRatio;
            }
            else
            {
                h=w/m_xyAspectRatio;
            }
        }
    }

    QRectF frame{0,0,w,h};
    frame.moveCenter(center);
    frame=clampToViewport(frame);

    auto changed=!rectsNearlyEqual(frame,m_viewportFrame);
    m_viewportFrame=frame;

    syncToView();
    if (changed)
    {
        emit frameChanged();
    }
}

//--------------------------------------------------------------------------

void CropRectItem::syncViewportFrameFromCropperRect()
{
    if (m_view==nullptr || m_view->viewport()==nullptr)
    {
        return;
    }

    // Pure QTransform composition (mapRectToScene() + viewportTransform(), both exact QRectF <->
    // QRectF maps), deliberately NOT QGraphicsView::mapFromScene(QPolygonF) -- that overload returns
    // a QPolygon (INTEGER), rounding each of the 4 mapped corners to the nearest device pixel
    // independently. The bounding rect of four independently-rounded corners can land up to 1px
    // larger, in EITHER dimension, than the exact rect. This function reruns on every mouse-move
    // during a drag (see updateRect() above), each time feeding its own just-rounded-up output back
    // in as next call's input -- that compounding rounding-up bias is exactly the "frame grows while
    // you drag it" bug this replaces, not anything in the drag math itself.
    QRectF viewportRect=m_view->viewportTransform().mapRect(mapRectToScene(m_cropperRect));
    viewportRect=clampToViewport(viewportRect);

    auto changed=!rectsNearlyEqual(viewportRect,m_viewportFrame);
    m_viewportFrame=viewportRect;

    // Re-derive m_cropperRect from the (possibly clamped) frame rather than leaving the caller's
    // candidate in place, so the two never drift apart.
    m_cropperRect=mapRectFromScene(m_view->viewportTransform().inverted().mapRect(m_viewportFrame));

    if (changed)
    {
        emit frameChanged();
    }
}

//--------------------------------------------------------------------------

QRectF CropRectItem::clampToViewport(QRectF rect) const
{
    if (m_view==nullptr || m_view->viewport()==nullptr)
    {
        return rect;
    }

    QRectF bounds{QPointF{0,0},QSizeF{m_view->viewport()->rect().size()}};

    if (rect.width()>bounds.width() || rect.height()>bounds.height())
    {
        if (keepAspectRatio() && rect.width()>0 && rect.height()>0)
        {
            // Shrink both dimensions by the SAME factor -- clamping width/height independently
            // (the plain per-axis path below) silently turns a square/aspect-locked frame into a
            // rectangle matching whatever aspect ratio the viewport happens to shrink to, e.g. after
            // the window is resized non-uniformly. setWidth()/setHeight() below each move only the
            // right/bottom edge, so moveCenter() afterwards re-centres the shrunk rect on its own
            // previous centre rather than leaving it pinned to the old top-left.
            auto shrink=std::min(bounds.width()/rect.width(),bounds.height()/rect.height());
            auto center=rect.center();
            rect.setWidth(rect.width()*shrink);
            rect.setHeight(rect.height()*shrink);
            rect.moveCenter(center);
        }
        else
        {
            if (rect.width()>bounds.width())
            {
                rect.setWidth(bounds.width());
            }
            if (rect.height()>bounds.height())
            {
                rect.setHeight(bounds.height());
            }
        }
    }

    if (rect.left()<bounds.left())
    {
        rect.moveLeft(bounds.left());
    }
    if (rect.top()<bounds.top())
    {
        rect.moveTop(bounds.top());
    }
    if (rect.right()>bounds.right())
    {
        rect.moveRight(bounds.right());
    }
    if (rect.bottom()>bounds.bottom())
    {
        rect.moveBottom(bounds.bottom());
    }
    return rect;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
