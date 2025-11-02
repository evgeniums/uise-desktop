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

/** @file uise/desktop/src/circlebusy.cpp
*
*  Defines CircleBusy.
*
*/

/****************************************************************************/

#include <QLabel>
#include <QPainter>
#include <QPalette>

#include <uise/desktop/style.hpp>
#include <uise/desktop/circlebusy.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** CircleBusy ***********************************/

CircleBusy::CircleBusy(QWidget *parent,
                       bool centerOnParent,
                       bool disableParentWhenSpinning )
    : QFrame(parent),
      m_centerOnParent(centerOnParent),
      m_disableParentWhenSpinning(disableParentWhenSpinning),
      m_startAngle(90),
      m_circlePercent(DefaultCirclePercent)
{
    m_sample=new QLabel(this);
    m_sample->setObjectName("sample");
    m_sample->setVisible(false);

    m_timeLine.setLoopCount(0);
    setEasingCurve(DefaultEasingCurve);
    connect(
        &m_timeLine,
        &QTimeLine::frameChanged,
        this,
        &CircleBusy::onTimeFrameChanged
    );
    start();
}

//--------------------------------------------------------------------------

void CircleBusy::paintEvent(QPaintEvent *event)
{
    updatePosition();

    QPainter p;
    p.begin(this);

    auto r=rect();

    // draw circle background
    auto w=r.width();
    int borderWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, m_sample);
    auto circleWidth=w-borderWidth*2;
    auto outerWidth=w-circleWidth;
    auto halfOuterWidth=outerWidth/2;
    auto x=r.left()+halfOuterWidth;
    auto y=r.height()-halfOuterWidth-circleWidth;
    QRect circleRect{x,y,circleWidth,circleWidth};
    QColor backgroundColor=m_sample->palette().color(QPalette::Window);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen{backgroundColor,qreal(borderWidth)});
    p.setRenderHints(QPainter::Antialiasing);
    p.drawEllipse(circleRect);

    // draw progress
    p.setBrush(Qt::NoBrush);
    auto progressColor=m_sample->palette().color(QPalette::Text);
    p.setPen(QPen{progressColor,qreal(borderWidth)});
    int spanAngle = static_cast<int>((m_circlePercent/ 100.0) * 360 * 16);
    p.drawArc(circleRect,m_startAngle*16,spanAngle);

    // done
    p.end();
}

//--------------------------------------------------------------------------

void CircleBusy::start()
{
    if(parentWidget() && m_disableParentWhenSpinning)
    {
        parentWidget()->setEnabled(false);
    }
    m_timeLine.setFrameRange(m_startAngle,m_startAngle-270-90);
    m_timeLine.start();
}

//--------------------------------------------------------------------------

void CircleBusy::stop()
{
    if(parentWidget() && m_disableParentWhenSpinning)
    {
        parentWidget()->setEnabled(true);
    }
    m_timeLine.stop();
}

//--------------------------------------------------------------------------

void CircleBusy::onTimeFrameChanged(int frame)
{
    m_startAngle=frame;
    repaint();
}

//--------------------------------------------------------------------------

void CircleBusy::updatePosition()
{
    if (parentWidget() && m_centerOnParent)
    {
        move(parentWidget()->width() / 2 - width() / 2, parentWidget()->height() / 2 - height() / 2);
    }
}

//--------------------------------------------------------------------------

void CircleBusy::setEasingCurve(const QEasingCurve& curve)
{
    m_timeLine.setEasingCurve(curve);
}

//--------------------------------------------------------------------------

QEasingCurve CircleBusy::easingCurve() const
{
    return m_timeLine.easingCurve();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
