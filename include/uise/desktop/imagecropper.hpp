/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/imagecropper.hpp
*
*  Declares ImageCropper.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_IMAGE_CROPPER_HPP
#define UISE_DESKTOP_IMAGE_CROPPER_HPP

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneMouseEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

// --- CropRectItem Definition and Implementation ---
class UISE_DESKTOP_EXPORT CropRectItem : public QGraphicsRectItem
{
    public:
        enum HandleType {
            NoHandle,
            TopLeft,
            TopRight,
            BottomLeft,
            BottomRight,
            TopEdge,
            BottomEdge,
            LeftEdge,
            RightEdge,
            Move
        };

        QRectF m_cropperRect;

        void setKeepAspectRatio(bool enable)
        {
            m_keepAspectRatio=enable;
        }

        bool isKeepAspectRatio() const noexcept
        {
            return m_keepAspectRatio;
        }

        void setSquare(bool enable)
        {
            m_square=enable;
        }

        bool isSquare() const noexcept
        {
            return m_square;
        }

        void setEllipse(bool enable)
        {
            m_ellipse=enable;
        }

        bool isEllipse() const noexcept
        {
            return m_ellipse;
        }

        void setMinimumImageSize(const QSize& size)
        {
            m_minWidth=size.width();
            m_minHeight=size.height();
        }

        QSize minimumImageSize() const
        {
            return QSize{int(m_minWidth),int(m_minHeight)};
        }

        // Constructor: Takes the initial crop area, the image item to crop, and the parent item.
        CropRectItem(const QRectF& imageRect, const QRectF& initialRect, QGraphicsPixmapItem* imageItem, QGraphicsItem *parent = nullptr)
            : QGraphicsRectItem(imageRect, parent),
              m_activeHandle(NoHandle),
              m_imageItem(imageItem),
              m_square(false),
              m_ellipse(false),
              m_keepAspectRatio(true)
        {
            m_cropperRect=initialRect;

            if (m_square)
            {
                m_xyAspectRatio=1.0;
            }
            else if (imageRect.height()!=0)
            {
                m_xyAspectRatio=imageRect.width()/imageRect.height();
            }

            // Enable hover events to change cursor shape
            setAcceptHoverEvents(true);

            // Set flags for movement, but we'll handle actual movement and resizing manually
            // because we want custom handles.
            setFlags(ItemIsMovable | ItemSendsGeometryChanges);
        }

        // Returns the current crop rectangle in coordinates local to the QGraphicsPixmapItem
        QRect getCropAreaInImageCoordinates() const {
            if (!m_imageItem) return QRect();

            // 1. Get the current rect of the cropper in the scene's coordinates
            QRectF sceneRect = mapToScene(rect()).boundingRect();

            // 2. Map this scene rectangle to the local coordinates of the image item
            // This is crucial if the image item is scaled or transformed.
            QRectF imageLocalRect = m_imageItem->mapFromScene(sceneRect).boundingRect();

            // 3. Convert to QRect (integer coordinates) for QPixmap::copy
            return imageLocalRect.toRect();
        }

    protected:
        // --- Drawing the Cropper and Handles ---

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
        {
            Q_UNUSED(option);
            Q_UNUSED(widget);

            QColor backgroundColor(0, 0, 0, 128);
            auto sz=boundingRect().size().toSize();
            QPixmap px(sz);
            QPainter p;
            px.fill(Qt::transparent);
            p.begin(&px);
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
            painter->drawPixmap(boundingRect().topLeft(), px);

            const auto& r=m_cropperRect;

            // 2. Draw the crop border
            QPen borderPen(Qt::white);
            borderPen.setWidth(2);
            borderPen.setStyle(Qt::DashLine);
            painter->setPen(borderPen);
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(r);

            // 3. Draw the corner and center handles
            painter->setBrush(Qt::white);
            painter->setPen(QPen(Qt::black, 1));
            const qreal handleSize = 10.0;
            const qreal halfHandle = handleSize / 2.0;

            // Draw Corner Handles
            painter->drawRect(r.topLeft().x() - halfHandle, r.topLeft().y() - halfHandle, handleSize, handleSize);
            painter->drawRect(r.topRight().x() - halfHandle, r.topRight().y() - halfHandle, handleSize, handleSize);
            painter->drawRect(r.bottomLeft().x() - halfHandle, r.bottomLeft().y() - halfHandle, handleSize, handleSize);
            painter->drawRect(r.bottomRight().x() - halfHandle, r.bottomRight().y() - halfHandle, handleSize, handleSize);
        }

        // --- Interaction Logic ---

        // Map a position to a HandleType for cursor changing and state tracking
        HandleType getHandleType(const QPointF& pos) const
        {
            const qreal handleTolerance = 15.0; // Area around handles to detect click
            QRectF r = m_cropperRect;

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

        void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override
        {
            HandleType handle = getHandleType(event->pos());
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

        void mousePressEvent(QGraphicsSceneMouseEvent *event) override
        {
            if (event->button() == Qt::LeftButton) {
                m_activeHandle = getHandleType(event->pos());
                m_lastPos = event->pos();
                if (m_activeHandle == Move) {
                    // If moving, we let QGraphicsRectItem handle the initial press for movement
                    QGraphicsRectItem::mousePressEvent(event);
                }
            }
        }

        void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override
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
            auto updateRect = [&](qreal newX, qreal newY, qreal newW, qreal newH) {

                // Apply new position and size, ensuring non-negative width/height
                if (newW > m_minWidth && newH > m_minHeight
                    && newX>=0
                    && newY>=0
                    && (newX+newW)<=imageBounds.right()
                    && (newY+newH)<=imageBounds.bottom()
                    )
                {
                    m_cropperRect=QRectF(newX, newY, newW, newH);
                }
            };

            switch (m_activeHandle) {
            case Move:
                // Let base class handle movement, but check against image bounds
                // QGraphicsRectItem::mouseMoveEvent(event);
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

            // After modification, update m_lastPos for the next move event,
            // but only if it was a resize operation. For move, m_lastPos is handled by base class
            // or we simply update it here after the move is done.
            m_lastPos = event->pos();
            update(); // Force repaint
        }

        void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
        {
            if (m_activeHandle != NoHandle) {
                m_activeHandle = NoHandle;
                // If it was a Move operation, let base class handle the release.
                if (getHandleType(event->pos()) == Move) {
                    QGraphicsRectItem::mouseReleaseEvent(event);
                }
            }
        }

    private:

        HandleType m_activeHandle;
        QPointF m_lastPos;
        QGraphicsPixmapItem* m_imageItem;

        bool keepAspectRatio() const
        {
            return m_square||m_keepAspectRatio;
        }

        bool m_square;
        bool m_ellipse;
        bool m_keepAspectRatio;

        double m_xyAspectRatio=1.0;
        qreal m_minWidth=10;
        qreal m_minHeight=10;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_IMAGE_CROPPER_HPP
