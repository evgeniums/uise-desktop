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

#include <QtGlobal>
#include <QLabel>
#include <QPainter>
#include <QEnterEvent>
#include <QPalette>
#include <QVariantAnimation>
#include <QShowEvent>
#include <QHideEvent>

#include <uise/desktop/style.hpp>
#include <uise/desktop/ripple.hpp>
#include <uise/desktop/loadcontrol.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** LoadControl ***********************************/

LoadControl::LoadControl(QWidget* parent)
    : AbstractLoadControl(parent),
      m_hovered(false),
      m_rotationPhase(0.0),
      m_circlePercent(DefaultCirclePercent),
      m_animationDuration(DefaultAnimationDuration)
{
    m_sample=new QFrame(this);
    m_sample->setObjectName("sample");
    m_sample->setVisible(false);
    // m_sample is only ever used as a hidden style reference (its palette -- background/
    // progress colors from loadcontrol.qss's light/dark #sample rules -- is read in
    // paintEvent(), it is never actually shown) -- being permanently hidden, it never goes
    // through Qt's normal show-triggered polish, so its QSS-driven palette would silently stay
    // at its unstyled default without this. Same idiom as ChatMessage::construct() forcing
    // polish on its own hidden avatarFramePlaceholder. Both polish mechanisms are needed, same
    // as DropdownFrame::measureContentSize()'s handling of the same class of issue:
    // Style::updateWidgetStyle() drives QStyle::polish(); ensurePolished() drives the separate
    // QEvent::Polish path that palette/font resolution goes through.
    Style::updateWidgetStyle(m_sample);
    m_sample->ensurePolished();

    // Drives the circulating arc in ProgressMode::Indeterminate/AnimatedProgress. Not started
    // here -- the default mode is Static, so the animation only starts once a non-Static mode is
    // set (see updateProgressMode()) and only while the control is actually visible (see
    // showEvent()/hideEvent()), same reasoning TypingIndicator/Skeleton apply to their loops.
    m_anim=new QVariantAnimation(this);
    m_anim->setStartValue(0.0);
    m_anim->setEndValue(1.0);
    m_anim->setLoopCount(-1);
    m_anim->setDuration(m_animationDuration);
    m_anim->setEasingCurve(DefaultEasingCurve);
    connect(
        m_anim,
        &QVariantAnimation::valueChanged,
        this,
        [this](const QVariant& value)
        {
            m_rotationPhase=value.toDouble();
            update();
        }
    );

    setCursor(Qt::PointingHandCursor);
    setState(state());

    // LoadControl paints its own circle directly in paintEvent() rather than through a child
    // widget, so the whole (square, per loadcontrol.qss's 56x56 pin shared with CircleBusy)
    // frame is the ripple host -- ellipse-clipped and centred, same halo treatment as an
    // icon-only IconTextButton, see ripple.qss. Unlike JumpEdge, LoadControl's own drawn circle
    // is centred within a genuinely square widget, so an ellipse clip on the whole frame draws
    // a true circle, not a distorted oval.
    m_ripple=RippleOverlay::install(this);
}

//--------------------------------------------------------------------------

void LoadControl::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p;
    p.begin(this);

    auto r=rect();

    // draw circle background
    //
    // The circle (and, below, the icon within it) are sized as a ratio of the control's own
    // rect, not the rect itself minus a near-zero style-metric inset -- CircleWidthRatio/
    // IconRadiusRatio exist for exactly this (previously declared but never actually read
    // here, which is why the circle used to fill ~100% of the control regardless of their
    // value). borderWidth is unrelated to sizing -- it is only the pen width used to stroke
    // the circle/arc outlines below.
    int borderWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, m_sample);
    auto circleWidth=qRound(qMin(r.width(),r.height())*CircleWidthRatio);
    auto x=r.left()+(r.width()-circleWidth)/2;
    auto y=r.top()+(r.height()-circleWidth)/2;
    QRect circleRect{x,y,circleWidth,circleWidth};
    auto backgroundColor=m_sample->palette().color(QPalette::Window);
    p.setBrush(backgroundColor);
    p.setPen(QPen{backgroundColor,qreal(borderWidth)});
    p.setRenderHints(QPainter::Antialiasing);
    p.drawEllipse(circleRect);

    // draw progress
    //
    // startAngle/spanAngle default to the Static look (fixed 12-o'clock start, span grows with
    // progress()); Indeterminate/AnimatedProgress override one or both from m_rotationPhase, which
    // is driven by m_anim while a non-Static mode is active (see updateProgressMode()).
    p.setBrush(Qt::NoBrush);
    auto progressColor=m_sample->palette().color(QPalette::Text);
    p.setPen(QPen{progressColor,qreal(borderWidth)});
    int startAngle = 90 * 16;
    int spanAngle = -static_cast<int>((progress()/ 100.0) * 360 * 16);
    switch (progressMode())
    {
        case ProgressMode::Static:
        {
        }
        break;

        case ProgressMode::Indeterminate:
        {
            startAngle=qRound((90.0-m_rotationPhase*360.0)*16);
            spanAngle=-static_cast<int>((m_circlePercent/100.0)*360*16);
        }
        break;

        case ProgressMode::AnimatedProgress:
        {
            startAngle=qRound((90.0-m_rotationPhase*360.0)*16);
        }
        break;
    }
    p.drawArc(circleRect,startAngle,spanAngle);

    // draw icon, centered within the circle (not the control's full rect) -- sized relative
    // to circleWidth so it shrinks/grows along with the circle rather than staying anchored
    // to the control's outer bounds
    if (m_icon)
    {
        p.setPen(Qt::NoPen);

        auto iconWidth=qRound(circleWidth*IconRadiusRatio);
        QRect iconRect{
            circleRect.left()+(circleWidth-iconWidth)/2,
            circleRect.top()+(circleWidth-iconWidth)/2,
            iconWidth,
            iconWidth
        };

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

void LoadControl::updateProgress()
{
    update();
}

//--------------------------------------------------------------------------

void LoadControl::updateProgressMode()
{
    m_rotationPhase=0.0;
    updateAnimation();
    update();
}

//--------------------------------------------------------------------------

void LoadControl::updateAnimation()
{
    // The animation only ever runs for a non-Static mode, and only while the control is on
    // screen -- LoadControl is created per chat file/image item inside a scrolled list (see
    // ChatMessageFileItem/ChatMessageImageItem), so leaving timers running on off-screen items
    // would be a real, avoidable CPU cost.
    auto shouldRun=progressMode()!=ProgressMode::Static && isVisible();
    if (shouldRun)
    {
        if (m_anim->state()!=QAbstractAnimation::Running)
        {
            m_anim->start();
        }
    }
    else
    {
        m_anim->stop();
    }
}

//--------------------------------------------------------------------------

void LoadControl::setAnimationDuration(int ms)
{
    m_animationDuration=ms;
    auto wasRunning=(m_anim->state()==QAbstractAnimation::Running);
    if (wasRunning)
    {
        m_anim->stop();
    }
    m_anim->setDuration(m_animationDuration);
    if (wasRunning)
    {
        m_anim->start();
    }
}

//--------------------------------------------------------------------------

void LoadControl::setEasingCurve(const QEasingCurve& curve)
{
    m_anim->setEasingCurve(curve);
}

//--------------------------------------------------------------------------

QEasingCurve LoadControl::easingCurve() const
{
    return m_anim->easingCurve();
}

//--------------------------------------------------------------------------

void LoadControl::showEvent(QShowEvent* event)
{
    QFrame::showEvent(event);
    updateAnimation();
}

//--------------------------------------------------------------------------

void LoadControl::hideEvent(QHideEvent* event)
{
    QFrame::hideEvent(event);
    updateAnimation();
}

//--------------------------------------------------------------------------

void LoadControl::updateState()
{
    QString iconName;
    switch (state())
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
