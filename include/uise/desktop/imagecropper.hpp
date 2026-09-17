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

#include <QObject>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneMouseEvent>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//! QObject is deliberately combined with QGraphicsRectItem via multiple inheritance (rather than
//! QGraphicsObject, which has no QGraphicsRectItem-equivalent) so this item can emit frameChanged()
//! -- QObject must stay the first base for moc/qobject_cast to work.
class UISE_DESKTOP_EXPORT CropRectItem : public QObject, public QGraphicsRectItem
{
    Q_OBJECT

    public:

        constexpr static const qreal BaseHandleWidth=1.0;
        constexpr static const qreal BaseHandleTolerance=15.0;
        constexpr static const QRgb HandleColor=QRgb{0xF0F0F0};
        constexpr static const QRgb BorderColor=QRgb{0xF0F0F0};
        constexpr static const uint32_t BorderColorAlpha=30;

        enum HandleType
        {
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
            updateAspectRatio();
            adjustCropRect();
            update();
        }

        bool isKeepAspectRatio() const noexcept
        {
            return m_keepAspectRatio;
        }

        void setSquare(bool enable)
        {
            m_square=enable;
            updateAspectRatio();
            adjustCropRect();
            update();
        }

        bool isSquare() const noexcept
        {
            return m_square;
        }

        void setEllipse(bool enable)
        {
            m_ellipse=enable;
            update();
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

        CropRectItem(QGraphicsView* view, QGraphicsPixmapItem* imageItem, QGraphicsItem *parent = nullptr);

        void init();

        QRectF getCropAreaCoordinates() const;

        void setView(QGraphicsView* view)
        {
            m_view=view;
        }

        QGraphicsView* view() const
        {
            return m_view;
        }

        void adjustCropRect();

        //! When true (default) adjustCropRect() intersects the crop rect with the currently
        //! visible viewport area, so a huge unfitted image doesn't start with a crop rect running
        //! off-screen. Must be turned off while the host view is zoomed in, or a rebuild of the
        //! crop rect (rotate/flip/aspect-ratio change) collapses it down to whatever sliver of the
        //! image happens to be on screen at that moment.
        void setLimitToVisibleArea(bool value) noexcept
        {
            m_limitToVisibleArea=value;
        }

        bool isLimitToVisibleArea() const noexcept
        {
            return m_limitToVisibleArea;
        }

        //! Public wrapper of the private hit-test below, in scene coordinates -- lets a host (e.g.
        //! a pan-filter callback) find out whether a point would land on a resize/move handle
        //! without duplicating the hit-testing logic itself.
        HandleType handleAt(const QPointF& scenePos) const
        {
            return getHandleType(mapFromScene(scenePos));
        }

        //! When true (default), the crop frame is pinned to the viewport -- zooming/panning the
        //! view moves and scales the image behind a stationary frame (Instagram-style). When
        //! false, the frame is glued to the image and zooms/pans together with it (legacy
        //! behaviour). Must be set before init() -- see AbstractImageEditor::CropFrameMode.
        void setFixedOnScreen(bool value) noexcept
        {
            m_fixedOnScreen=value;
        }

        bool isFixedOnScreen() const noexcept
        {
            return m_fixedOnScreen;
        }

        //! The pinned frame's own rectangle, in viewport pixels. Empty until the first
        //! adjustCropRect() (fixed-on-screen mode only) -- see frameChanged().
        QRectF viewportFrame() const noexcept
        {
            return m_viewportFrame;
        }

        //! Re-derives the crop rect's scene-space projection from the still-pinned viewport frame
        //! after a pure view change (zoom/pan/resize) -- the frame's own viewport rectangle is
        //! unchanged, so this does not emit frameChanged(). No-op unless isFixedOnScreen(). Called
        //! by the host on every scroll/zoom/resize (see FreeHandDrawView::scrollContentsBy()).
        void syncToView();

    protected:

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;        

        void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;

        void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

        void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;

        void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    signals:

        //! Emitted when the viewport-pinned frame's own rectangle changes (fixed-on-screen mode
        //! only) -- e.g. after a user resize/move drag, or after adjustCropRect() reseeds/resizes
        //! it for a crop-shape or aspect-ratio change. NOT emitted for a pure zoom/pan -- the
        //! frame's viewport rectangle is by definition unchanged then, see syncToView().
        void frameChanged();

    private:

        HandleType m_activeHandle;
        QPointF m_lastPos;
        QGraphicsPixmapItem* m_imageItem;

        HandleType getHandleType(QPointF pos, bool forCursor=false) const;

        //! Fixed-on-screen counterpart of adjustCropRect()'s legacy body -- seeds/resizes
        //! m_viewportFrame (centred in the full viewport on first seed; otherwise in place around
        //! its own current centre/footprint) and re-derives m_cropperRect from it.
        void adjustViewportFrame();

        //! Fixed-on-screen counterpart of the legacy drag path -- re-derives m_viewportFrame from a
        //! just-dragged m_cropperRect, clamped to the viewport, then re-derives m_cropperRect back
        //! from the clamped frame so the two never drift apart.
        void syncViewportFrameFromCropperRect();

        //! Shrinks/moves rect to fit fully inside the current viewport, preserving its size unless
        //! it is already larger than the viewport in a given dimension. Used both after a drag and
        //! after a viewport resize, which can leave a previously in-bounds frame hanging outside.
        QRectF clampToViewport(QRectF rect) const;

        void updateAspectRatio()
        {
            auto cropperRect=m_imageItem->boundingRect();
            if (m_square)
            {
                m_xyAspectRatio=1.0;
            }
            else if (cropperRect.height()!=0)
            {
                m_xyAspectRatio=cropperRect.width()/cropperRect.height();
            }
        }

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

        QGraphicsView* m_view=nullptr;

        bool m_limitToVisibleArea=true;

        bool m_fixedOnScreen=true;
        QRectF m_viewportFrame;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_IMAGE_CROPPER_HPP
