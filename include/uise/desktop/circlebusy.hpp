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

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT CircleBusy : public QFrame
{
    Q_OBJECT

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

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CIRCLE_BUSY_HPP
