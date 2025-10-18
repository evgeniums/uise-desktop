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

#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneMouseEvent>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

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

        enum { Type = UserType + 1 };

        int type() const override
        {
            return Type;
        }

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

        CropRectItem(const QRectF& imageRect, const QRectF& initialRect, QGraphicsPixmapItem* imageItem, QGraphicsItem *parent = nullptr);

        QRectF getCropAreaCoordinates() const;

    protected:

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

        HandleType getHandleType(const QPointF& pos) const;

        void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;

        void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

        void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;

        void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

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

        QRectF m_cropperRect;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_IMAGE_CROPPER_HPP
