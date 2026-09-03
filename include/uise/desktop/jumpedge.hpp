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

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class RippleOverlay;

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

        /** @brief The click-ripple overlay installed on this widget's main circle area (not
         *  the optional badge strip above it), see RippleOverlay. */
        RippleOverlay* rippleOverlay() const noexcept
        {
            return m_ripple;
        }

        /**
         * @brief Force the visual hovered state without a real QEnterEvent.
         * @param hovered New forced-hover state.
         *
         * ORed with the button's own real hover state (see enterEvent()/leaveEvent()) when
         * paintEvent() decides whether to paint the hovered icon -- for a caller (e.g.
         * ImageViewerWidget's edge navigation zones) that wants this button to look hovered
         * while the pointer is actually in some larger area that only proxies for hovering it.
         */
        void setForceHovered(bool hovered);

    signals:

        void clicked();

    public slots:

        void setBadgeText(const QString& text);

    protected:

        void paintEvent(QPaintEvent *event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;

    private:

        void updateIcon();
        void renderBadgeText(QPainter& p);
        void updateRippleGeometry();

        QLabel* m_badgeText;
        bool m_hovered;
        bool m_forceHovered=false;
        std::shared_ptr<SvgIcon> m_icon;

        Qt::Orientation m_orientation;
        Direction m_direction;

        QFrame* m_sample;
        IconDirection m_iconDirection;
        bool m_pressed=false;

        QWidget* m_rippleArea=nullptr;
        RippleOverlay* m_ripple=nullptr;
};

}

#endif // UISE_DESKTOP_JUMP_EDGE_HPP
