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

/** @file uise/desktop/freehanddrawview.hpp
*
*  Declares FreeHandDrawView.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FREE_HAND_DRAW_VIEW_HPP
#define UISE_DESKTOP_FREE_HAND_DRAW_VIEW_HPP

#include <QStack>
#include <QGraphicsView>
#include <QPainterPath>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT FreeHandDrawView : public QGraphicsView
{
    Q_OBJECT

    public:

        explicit FreeHandDrawView(QWidget *parent = nullptr);

        bool isFreeHandDrawEnabled() const noexcept
        {
            return m_freeHandDrawEnabled;
        }

        QColor penColor() const noexcept
        {
            return m_penColor;
        }

        int penWidth() const noexcept
        {
            return m_penWidth;
        }

        void setItemGroup(QGraphicsItemGroup* group)
        {
            m_group=group;
        }

        QGraphicsItemGroup* group() const
        {
            return m_group;
        }

    public slots:

        void setFreeHandDrawEnabled(bool value);
        void undoHandDraw();
        void redoHandDraw();
        void acceptHandDraw();
        void cancelHandDraw();

        void setPenWidth(int width)
        {
            m_penWidth=width;
        }

        void setPenColor(const QColor& color)
        {
            m_penColor=color;
        }

    protected:

        void mousePressEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;

    private:

        QGraphicsPathItem *m_currentPathItem;
        QPainterPath m_currentPath;

        bool m_freeHandDrawEnabled;
        QColor m_penColor;
        int m_penWidth;

        QStack<QGraphicsPathItem*> m_undoStack;
        QStack<QGraphicsPathItem*> m_redoStack;

        QGraphicsItemGroup* m_group;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_FREE_HAND_DRAW_VIEW_HPP
