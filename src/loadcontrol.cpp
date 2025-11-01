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

/** @file uise/desktop/src/loadcontrol.cpp
*
*  Defines LoadControl.
*
*/

/****************************************************************************/

#include <QLabel>
#include <QPainter>
#include <QEnterEvent>
#include <QPalette>

#include <uise/desktop/style.hpp>
#include <uise/desktop/loadcontrol.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** LoadControl ***********************************/

LoadControl::LoadControl(QWidget* parent)
    : QFrame(parent),
      m_state(State::CanDownload),
      m_hovered(false)
{
    m_sample=new QFrame(this);
    m_sample->setObjectName("sample");
    m_sample->setVisible(false);

    setCursor(Qt::PointingHandCursor);
    setState(m_state);
}

//--------------------------------------------------------------------------

void LoadControl::paintEvent(QPaintEvent *event)
{
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
    auto backgroundColor=m_sample->palette().color(QPalette::Window);
    p.setBrush(backgroundColor);    
    p.setPen(QPen{backgroundColor,qreal(borderWidth)});
    p.setRenderHints(QPainter::Antialiasing);
    p.drawEllipse(circleRect);

    // draw progress
    p.setBrush(Qt::NoBrush);
    auto progressColor=m_sample->palette().color(QPalette::Text);    
    p.setPen(QPen{progressColor,qreal(borderWidth)});
    int startAngle = 90 * 16;
    int spanAngle = -static_cast<int>((m_progress/ 100.0) * 360 * 16);
    p.drawArc(circleRect,startAngle,spanAngle);

    // draw icon
    if (m_icon)
    {
        p.setPen(Qt::NoPen);

        // encode configurable parameters in QSS using properties:
        //  *min-width is for base padding of icon
        auto m=m_sample->contentsMargins();
        auto basePadding=m_sample->minimumWidth()-m.left()-m.right();

        auto iconOuterWidth=2*basePadding;
        auto iconWidth=w-iconOuterWidth;

        auto leftPadding=basePadding;
        auto bottomPadding=basePadding;

        QRect iconRect{r.left()+leftPadding,r.bottom()-iconWidth-bottomPadding,iconWidth,iconWidth};

        auto mode=IconMode::Normal;
        if (m_hovered)
        {
            mode=IconMode::Hovered;
        }
        m_icon->paint(&p,iconRect,mode,QIcon::Off,false);
    }

    // done
    p.end();
}

//--------------------------------------------------------------------------

void LoadControl::updateIcon(const QString name)
{
    if (name.isEmpty())
    {
        m_icon.reset();
        update();
        return;
    }

    m_icon=Style::instance().svgIconLocator().icon(QString("LoadControl::%1").arg(name),this);
    update();
}

//--------------------------------------------------------------------------

void LoadControl::enterEvent(QEnterEvent* event)
{
    m_hovered=true;
    QFrame::enterEvent(event);
    update();
}

//--------------------------------------------------------------------------

void LoadControl::leaveEvent(QEvent* event)
{
    m_hovered=false;
    QFrame::leaveEvent(event);
    update();
}

//--------------------------------------------------------------------------

void LoadControl::mousePressEvent(QMouseEvent* event)
{
    QFrame::mousePressEvent(event);
    if (event->button()==Qt::LeftButton)
    {
        emit clicked();
    }
}

//--------------------------------------------------------------------------

void LoadControl::setProgress(qreal value)
{
    m_progress=value;
    update();
}

//--------------------------------------------------------------------------

void LoadControl::setState(State state)
{
    m_state=state;

    QString iconName;
    switch (state)
    {
        case State::None:
        {
        }
        break;

        case State::CanDownload:
        {
            iconName="download";
        }
        break;

        case State::CanUpload:
        {
            iconName="upload";
        }
        break;

        case State::Paused:
        {
            iconName="pause";
        }
        break;

        case State::Waiting:
        {
            iconName="wait";
        }
        break;

        case State::Running:
        {
            iconName="stop";
        }
        break;
    }

    updateIcon(iconName);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
