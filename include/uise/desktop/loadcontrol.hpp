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

/** @file uise/desktop/loadcontrol.hpp
*
*  Declares LoadControl.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_LOAD_CONTROL_HPP
#define UISE_DESKTOP_LOAD_CONTROL_HPP

#include <QFrame>
#include <QEasingCurve>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/utils/enums.hpp>
#include <uise/desktop/abstractloadcontrol.hpp>

class QVariantAnimation;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class RippleOverlay;

class UISE_DESKTOP_EXPORT LoadControl : public AbstractLoadControl
{
    Q_OBJECT

    Q_PROPERTY(qreal circlePercent READ circlePercent WRITE setCirclePercent)
    Q_PROPERTY(int animationDuration READ animationDuration WRITE setAnimationDuration)
    Q_PROPERTY(int easingCurveType READ easingCurveType WRITE setEasingCurveType)

    public:

        //! Diameter of the drawn circle, as a ratio of the control's own (shorter) side.
        constexpr static const qreal CircleWidthRatio=0.6;
        //! Diameter of the icon, as a ratio of the circle (CircleWidthRatio), not of the
        //! control itself -- the icon shrinks/grows along with the circle.
        constexpr static const qreal IconRadiusRatio=0.65;

        //! Default arc length, as a percentage of the full circle, used in ProgressMode::Indeterminate.
        constexpr static const qreal DefaultCirclePercent=15.0;
        //! Default duration of a full revolution of the circulating arc, in milliseconds.
        constexpr static const int DefaultAnimationDuration=1000;
        //! Default easing curve of the circulating arc -- matches CircleBusy's default breathing
        //! accelerate/decelerate motion rather than a mechanical constant-speed spin.
        constexpr static const QEasingCurve::Type DefaultEasingCurve=QEasingCurve::InOutSine;

        LoadControl(QWidget* parent=nullptr);

        void setCirclePercent(qreal circlePercent)
        {
            m_circlePercent=circlePercent;
            update();
        }

        qreal circlePercent() const noexcept
        {
            return m_circlePercent;
        }

        void setAnimationDuration(int ms);

        int animationDuration() const noexcept
        {
            return m_animationDuration;
        }

        void setEasingCurve(const QEasingCurve& curve);

        QEasingCurve easingCurve() const;

        int easingCurveType() const noexcept
        {
            return static_cast<int>(easingCurve().type());
        }

        void setEasingCurveType(int type)
        {
            setEasingCurve(static_cast<QEasingCurve::Type>(type));
        }

        /** @brief The click-ripple overlay installed on this widget, see RippleOverlay. */
        RippleOverlay* rippleOverlay() const noexcept
        {
            return m_ripple;
        }

        /**
         * @brief Enable or disable click interaction, default enabled.
         *
         * Disabling suppresses clicked(), the ripple, the hover color swap and the
         * pointing-hand cursor together -- a control that still highlighted and rippled but
         * did nothing on click would misrepresent itself as actionable. Intended for states
         * where the control is purely an indicator, e.g. a permanently failed transfer whose
         * only recovery path is a menu entry rather than the control itself.
         */
        void setClickable(bool enable);

        bool isClickable() const noexcept
        {
            return m_clickable;
        }

    protected:

        void paintEvent(QPaintEvent *event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;

        void updateState() override;
        void updateProgress() override;
        void updateProgressMode() override;

    private:

        void updateIcon(const QString name={}, const QString& context={});
        void updateAnimation();

        std::shared_ptr<SvgIcon> m_icon;

        QFrame* m_sample;

        bool m_hovered;

        QVariantAnimation* m_anim;
        qreal m_rotationPhase;
        qreal m_circlePercent;
        int m_animationDuration;
        bool m_pressed=false;
        bool m_clickable=true;

        RippleOverlay* m_ripple=nullptr;
};

}

#endif // UISE_DESKTOP_LOAD_CONTROL_HPP
