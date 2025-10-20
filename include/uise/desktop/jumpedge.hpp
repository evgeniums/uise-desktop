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

/** @file uise/desktop/jumpedge.hpp
*
*  Declares JumpEdge.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_JUMP_EDGE_HPP
#define UISE_DESKTOP_JUMP_EDGE_HPP

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/utils/enums.hpp>

class QLabel;

UISE_DESKTOP_NAMESPACE_BEGIN

class SingleShotTimer;

class UISE_DESKTOP_EXPORT JumpEdge : public QFrame
{
    Q_OBJECT

    public:

        enum class IconDirection
        {
            Up,
            Down,
            Left,
            Right
        };

        constexpr static const qreal CircleWidthRatio=0.8;
        constexpr static const qreal IconRadiusRatio=0.8;
        constexpr static const qreal BadgeSizeRatio=0.8;
        constexpr static const qreal BadgeOverlap=0.2;

        JumpEdge(QWidget* parent);

        void clearBadgeText();

        QString badgeText() const;

        void setDirection(Direction value)
        {
            m_direction=value;
            updateIcon();
        }

        Direction direction() const noexcept
        {
            return m_direction;
        }

        void setOrientation(Qt::Orientation value)
        {
            m_orientation=value;
            updateIcon();
        }

        Qt::Orientation orientation() const noexcept
        {
            return m_orientation;
        }

        IconDirection iconDirection() const noexcept
        {
            return m_iconDirection;
        }

    signals:

        void clicked();

    public slots:

        void setBadgeText(const QString& text);

    protected:

        void paintEvent(QPaintEvent *event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;

    private:

        void updateIcon();

        QLabel* m_badgeText;
        bool m_hovered;
        std::shared_ptr<SvgIcon> m_icon;

        Qt::Orientation m_orientation;
        Direction m_direction;

        QFrame* m_sample;
        IconDirection m_iconDirection;
        SingleShotTimer* m_clickTimer;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_JUMP_EDGE_HPP
