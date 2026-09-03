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

/** @file uise/desktop/circlebusy.hpp
*
*  Declares CircleBusy.
*
*/

/****************************************************************************/

#include <QTimeLine>

#ifndef UISE_DESKTOP_CIRCLE_BUSY_HPP
#define UISE_DESKTOP_CIRCLE_BUSY_HPP

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

class QLabel;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class UISE_DESKTOP_EXPORT CircleBusy : public QFrame
{
    Q_OBJECT

    Q_PROPERTY(qreal circlePercent READ circlePercent WRITE setCirclePercent)
    Q_PROPERTY(int easingCurveType READ easingCurveType WRITE setEasingCurveType)

    public:

        constexpr static const qreal DefaultCirclePercent=15.0;
        constexpr static const QEasingCurve::Type DefaultEasingCurve=QEasingCurve::InOutSine;

        CircleBusy(QWidget *parent = nullptr,
                    bool centerOnParent = true,
                    bool disableParentWhenSpinning = true);

        void setCirclePercent(qreal circlePercent)
        {
            m_circlePercent=circlePercent;
            update();
        }

        qreal circlePercent() const noexcept
        {
            return m_circlePercent;
        }

        void setCenterOnParent(bool enable)
        {
            m_centerOnParent=enable;
            updatePosition();
        }

        bool isCenterOnParent() const noexcept
        {
            return m_centerOnParent;
        }

        void setDisableParentWhenSpinning(bool enable)
        {
            m_disableParentWhenSpinning=enable;
        }

        bool isDisableParentWhenSpinning() const noexcept
        {
            return m_disableParentWhenSpinning;
        }

        void setEasingCurve(const QEasingCurve& curve);

        QEasingCurve easingCurve() const;

        int easingCurveType() const noexcept
        {
            return static_cast<int>(m_timeLine.easingCurve().type());
        }

        void setEasingCurveType(int type)
        {
            setEasingCurve(static_cast<QEasingCurve::Type>(type));
        }

        bool isRunning() const noexcept;

    public slots:

        void start();

        void stop();

    protected:

        void paintEvent(QPaintEvent *event) override;

    private slots:

        void onTimeFrameChanged(int frame);

    private:

        void updatePosition();
        bool m_centerOnParent;
        bool m_disableParentWhenSpinning;

        int m_startAngle;
        qreal m_circlePercent;
        QTimeLine m_timeLine;
        QLabel* m_sample;

        QEasingCurve::Type m_easingCurve;
};

}

#endif // UISE_DESKTOP_CIRCLE_BUSY_HPP
