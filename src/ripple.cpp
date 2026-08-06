/**
@copyright Evgeny Sidorov 2026

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/ripple.cpp
*
*  Defines RippleOverlay.
*
*/

/****************************************************************************/

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QLineF>
#include <QVariantAnimation>

#include <uise/desktop/style.hpp>
#include <uise/desktop/ripple.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//==========================================================================

class RippleOverlay_p
{
    public:

        bool autoTrigger=RippleOverlay::DefaultAutoTrigger;
        bool rippleEnabled=RippleOverlay::DefaultRippleEnabled;

        QColor color=QColor(0,0,0);
        qreal opacity=RippleOverlay::DefaultOpacity;
        int durationMs=RippleOverlay::DefaultDurationMs;
        int fadeDurationMs=RippleOverlay::DefaultFadeDurationMs;
        QEasingCurve::Type easingType=RippleOverlay::DefaultEasingCurve;
        qreal radiusScaleX=RippleOverlay::DefaultRadiusScaleX;
        qreal radiusScaleY=RippleOverlay::DefaultRadiusScaleY;
        bool holdOnPress=RippleOverlay::DefaultHoldOnPress;
        RippleOverlay::Origin origin=RippleOverlay::DefaultOrigin;
        RippleOverlay::Clip clip=RippleOverlay::DefaultClip;
        int cornerRadius=RippleOverlay::DefaultCornerRadius;
        int insetLeft=RippleOverlay::DefaultInset;
        int insetTop=RippleOverlay::DefaultInset;
        int insetRight=RippleOverlay::DefaultInset;
        int insetBottom=RippleOverlay::DefaultInset;

        QPointF originPoint;
        bool held=false;
        bool pendingFadeAfterGrow=false;
        qreal growValue=0.0;
        qreal fadeValue=1.0;

        QVariantAnimation* growAnim=nullptr;
        QVariantAnimation* fadeAnim=nullptr;
};

//==========================================================================

RippleOverlay::RippleOverlay(QWidget* host)
    : QWidget(host),
      pimpl(std::make_unique<RippleOverlay_p>())
{
    Q_ASSERT(host!=nullptr);

    setObjectName("rippleOverlay");

    // Transparent for both mouse and (lack of) background: this widget only ever paints the
    // ripple shape itself on top of whatever the host already drew -- it never intercepts
    // input (the host, or its own children, must keep receiving mouse events normally) and
    // never erases the host's background before its own paintEvent() runs.
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);

    updateGeometryFromHost();
    host->installEventFilter(this);

    pimpl->growAnim=new QVariantAnimation(this);
    pimpl->growAnim->setStartValue(0.0);
    pimpl->growAnim->setEndValue(1.0);
    connect(
        pimpl->growAnim,
        &QVariantAnimation::valueChanged,
        this,
        [this](const QVariant& value)
        {
            pimpl->growValue=value.toDouble();
            update();
        }
    );
    connect(
        pimpl->growAnim,
        &QVariantAnimation::finished,
        this,
        [this]()
        {
            // Either the host already released while the grow animation was still running
            // (pendingFadeAfterGrow), or holdOnPress is off and a held press never delays the
            // fade in the first place -- either way the grow-to-full-size hold ends here.
            if (pimpl->pendingFadeAfterGrow || !pimpl->holdOnPress)
            {
                pimpl->pendingFadeAfterGrow=false;
                startFade();
            }
        }
    );

    pimpl->fadeAnim=new QVariantAnimation(this);
    pimpl->fadeAnim->setStartValue(1.0);
    pimpl->fadeAnim->setEndValue(0.0);
    connect(
        pimpl->fadeAnim,
        &QVariantAnimation::valueChanged,
        this,
        [this](const QVariant& value)
        {
            pimpl->fadeValue=value.toDouble();
            update();
        }
    );
    connect(
        pimpl->fadeAnim,
        &QVariantAnimation::finished,
        this,
        [this]()
        {
            hide();
        }
    );

    // Nothing to paint until the first ripple -- and, being hidden, this overlay never steals
    // the host's very first QEvent::Show based repolish either.
    hide();
}

//--------------------------------------------------------------------------

RippleOverlay::~RippleOverlay()
{
}

//--------------------------------------------------------------------------

RippleOverlay* RippleOverlay::install(QWidget* host)
{
    if (host==nullptr)
    {
        return nullptr;
    }

    auto overlay=find(host);
    if (overlay!=nullptr)
    {
        return overlay;
    }

    overlay=new RippleOverlay(host);

    // Push QSS-supplied property values (colour, timings, clip, ...) into effect now, same
    // idiom as LoadControl's permanently-hidden #sample reference frame in loadcontrol.cpp:
    // Style::updateWidgetStyle() drives QStyle::polish(), ensurePolished() drives the separate
    // QEvent::Polish path that a hidden widget would otherwise never go through.
    Style::updateWidgetStyle(overlay);
    overlay->ensurePolished();

    return overlay;
}

//--------------------------------------------------------------------------

RippleOverlay* RippleOverlay::find(QWidget* host)
{
    if (host==nullptr)
    {
        return nullptr;
    }

    return host->findChild<RippleOverlay*>(QString{},Qt::FindDirectChildrenOnly);
}

//--------------------------------------------------------------------------

QWidget* RippleOverlay::host() const noexcept
{
    return qobject_cast<QWidget*>(parent());
}

//--------------------------------------------------------------------------

void RippleOverlay::updateGeometryFromHost()
{
    auto h=host();
    if (h!=nullptr)
    {
        auto w=qMax(0,h->width()-pimpl->insetLeft-pimpl->insetRight);
        auto hgt=qMax(0,h->height()-pimpl->insetTop-pimpl->insetBottom);
        setGeometry(QRect(QPoint(pimpl->insetLeft,pimpl->insetTop),QSize(w,hgt)));
    }
}

//--------------------------------------------------------------------------

void RippleOverlay::setAutoTrigger(bool enable) noexcept
{
    pimpl->autoTrigger=enable;
}

//--------------------------------------------------------------------------

bool RippleOverlay::isAutoTrigger() const noexcept
{
    return pimpl->autoTrigger;
}

//--------------------------------------------------------------------------

void RippleOverlay::setRippleEnabled(bool enable) noexcept
{
    pimpl->rippleEnabled=enable;
    if (!enable)
    {
        cancel();
    }
}

//--------------------------------------------------------------------------

bool RippleOverlay::isRippleEnabled() const noexcept
{
    return pimpl->rippleEnabled;
}

//--------------------------------------------------------------------------

void RippleOverlay::setRippleColor(const QColor& color)
{
    pimpl->color=color;
    update();
}

//--------------------------------------------------------------------------

QColor RippleOverlay::rippleColor() const noexcept
{
    return pimpl->color;
}

//--------------------------------------------------------------------------

void RippleOverlay::setRippleOpacity(qreal opacity) noexcept
{
    pimpl->opacity=opacity;
    update();
}

//--------------------------------------------------------------------------

qreal RippleOverlay::rippleOpacity() const noexcept
{
    return pimpl->opacity;
}

//--------------------------------------------------------------------------

void RippleOverlay::setRippleDurationMs(int ms) noexcept
{
    pimpl->durationMs=ms;
}

//--------------------------------------------------------------------------

int RippleOverlay::rippleDurationMs() const noexcept
{
    return pimpl->durationMs;
}

//--------------------------------------------------------------------------

void RippleOverlay::setRippleFadeDurationMs(int ms) noexcept
{
    pimpl->fadeDurationMs=ms;
}

//--------------------------------------------------------------------------

int RippleOverlay::rippleFadeDurationMs() const noexcept
{
    return pimpl->fadeDurationMs;
}

//--------------------------------------------------------------------------

void RippleOverlay::setRippleEasingCurveType(int type)
{
    pimpl->easingType=static_cast<QEasingCurve::Type>(type);
}

//--------------------------------------------------------------------------

int RippleOverlay::rippleEasingCurveType() const noexcept
{
    return static_cast<int>(pimpl->easingType);
}

//--------------------------------------------------------------------------

void RippleOverlay::setRippleRadiusScaleX(qreal scale) noexcept
{
    pimpl->radiusScaleX=scale;
}

//--------------------------------------------------------------------------

qreal RippleOverlay::rippleRadiusScaleX() const noexcept
{
    return pimpl->radiusScaleX;
}

//--------------------------------------------------------------------------

void RippleOverlay::setRippleRadiusScaleY(qreal scale) noexcept
{
    pimpl->radiusScaleY=scale;
}

//--------------------------------------------------------------------------

qreal RippleOverlay::rippleRadiusScaleY() const noexcept
{
    return pimpl->radiusScaleY;
}

//--------------------------------------------------------------------------

void RippleOverlay::setRippleHoldOnPress(bool enable) noexcept
{
    pimpl->holdOnPress=enable;
}

//--------------------------------------------------------------------------

bool RippleOverlay::isRippleHoldOnPress() const noexcept
{
    return pimpl->holdOnPress;
}

//--------------------------------------------------------------------------

void RippleOverlay::setRippleOrigin(Origin origin) noexcept
{
    pimpl->origin=origin;
}

//--------------------------------------------------------------------------

RippleOverlay::Origin RippleOverlay::rippleOrigin() const noexcept
{
    return pimpl->origin;
}

//--------------------------------------------------------------------------

void RippleOverlay::setRippleOriginName(const QString& name)
{
    setRippleOrigin(name.compare(QStringLiteral("center"),Qt::CaseInsensitive)==0 ? Origin::Center : Origin::Cursor);
}

//--------------------------------------------------------------------------

QString RippleOverlay::rippleOriginName() const
{
    return pimpl->origin==Origin::Center ? QStringLiteral("center") : QStringLiteral("cursor");
}

//--------------------------------------------------------------------------

void RippleOverlay::setRippleClip(Clip clip) noexcept
{
    pimpl->clip=clip;
}

//--------------------------------------------------------------------------

RippleOverlay::Clip RippleOverlay::rippleClip() const noexcept
{
    return pimpl->clip;
}

//--------------------------------------------------------------------------

void RippleOverlay::setRippleClipName(const QString& name)
{
    if (name.compare(QStringLiteral("ellipse"),Qt::CaseInsensitive)==0)
    {
        setRippleClip(Clip::Ellipse);
    }
    else if (name.compare(QStringLiteral("rounded"),Qt::CaseInsensitive)==0)
    {
        setRippleClip(Clip::Rounded);
    }
    else
    {
        setRippleClip(Clip::Rect);
    }
}

//--------------------------------------------------------------------------

QString RippleOverlay::rippleClipName() const
{
    switch (pimpl->clip)
    {
        case Clip::Ellipse: return QStringLiteral("ellipse");
        case Clip::Rounded: return QStringLiteral("rounded");
        case Clip::Rect: break;
    }
    return QStringLiteral("rect");
}

//--------------------------------------------------------------------------

void RippleOverlay::setRippleCornerRadius(int radius) noexcept
{
    pimpl->cornerRadius=radius;
}

//--------------------------------------------------------------------------

int RippleOverlay::rippleCornerRadius() const noexcept
{
    return pimpl->cornerRadius;
}

//--------------------------------------------------------------------------

void RippleOverlay::setRippleInsetLeft(int px) noexcept
{
    pimpl->insetLeft=px;
    updateGeometryFromHost();
}

//--------------------------------------------------------------------------

int RippleOverlay::rippleInsetLeft() const noexcept
{
    return pimpl->insetLeft;
}

//--------------------------------------------------------------------------

void RippleOverlay::setRippleInsetTop(int px) noexcept
{
    pimpl->insetTop=px;
    updateGeometryFromHost();
}

//--------------------------------------------------------------------------

int RippleOverlay::rippleInsetTop() const noexcept
{
    return pimpl->insetTop;
}

//--------------------------------------------------------------------------

void RippleOverlay::setRippleInsetRight(int px) noexcept
{
    pimpl->insetRight=px;
    updateGeometryFromHost();
}

//--------------------------------------------------------------------------

int RippleOverlay::rippleInsetRight() const noexcept
{
    return pimpl->insetRight;
}

//--------------------------------------------------------------------------

void RippleOverlay::setRippleInsetBottom(int px) noexcept
{
    pimpl->insetBottom=px;
    updateGeometryFromHost();
}

//--------------------------------------------------------------------------

int RippleOverlay::rippleInsetBottom() const noexcept
{
    return pimpl->insetBottom;
}

//--------------------------------------------------------------------------

void RippleOverlay::start(const QPoint& pos)
{
    if (!pimpl->rippleEnabled)
    {
        return;
    }

    // Only one ripple is ever shown at a time -- a new press simply restarts from scratch.
    pimpl->growAnim->stop();
    pimpl->fadeAnim->stop();
    pimpl->pendingFadeAfterGrow=false;

    pimpl->originPoint=pos;
    pimpl->held=true;
    pimpl->growValue=0.0;
    pimpl->fadeValue=1.0;

    pimpl->growAnim->setDuration(pimpl->durationMs);
    pimpl->growAnim->setEasingCurve(pimpl->easingType);

    show();
    raise();
    pimpl->growAnim->start();
    update();
}

//--------------------------------------------------------------------------

void RippleOverlay::release()
{
    pimpl->held=false;

    if (!pimpl->holdOnPress)
    {
        // The fade is (or will be) started from the grow-finished handler regardless of
        // held -- nothing to do here.
        return;
    }

    if (pimpl->growAnim->state()==QAbstractAnimation::Running)
    {
        // Let the grow animation finish its course -- its finished handler will start the
        // fade -- rather than cutting the grow short mid-flight.
        pimpl->pendingFadeAfterGrow=true;
        return;
    }

    startFade();
}

//--------------------------------------------------------------------------

void RippleOverlay::cancel()
{
    pimpl->growAnim->stop();
    pimpl->fadeAnim->stop();
    pimpl->held=false;
    pimpl->pendingFadeAfterGrow=false;
    pimpl->growValue=0.0;
    pimpl->fadeValue=1.0;
    hide();
}

//--------------------------------------------------------------------------

void RippleOverlay::startFade()
{
    if (pimpl->fadeAnim->state()==QAbstractAnimation::Running)
    {
        return;
    }

    pimpl->fadeAnim->stop();
    pimpl->fadeAnim->setDuration(pimpl->fadeDurationMs);
    pimpl->fadeAnim->setStartValue(pimpl->fadeValue);
    pimpl->fadeAnim->setEndValue(0.0);
    pimpl->fadeAnim->start();
}

//--------------------------------------------------------------------------

bool RippleOverlay::eventFilter(QObject* watched, QEvent* event)
{
    if (watched!=host())
    {
        return QWidget::eventFilter(watched,event);
    }

    switch (event->type())
    {
        case (QEvent::Resize):
        {
            updateGeometryFromHost();
        }
        break;

        case (QEvent::ChildAdded):
        case (QEvent::Show):
        {
            // Keep the overlay last in the host's child list -- and therefore on top -- even
            // if the host adds children (or is re-shown) after install() was called.
            raise();
        }
        break;

        case (QEvent::MouseButtonPress):
        {
            if (pimpl->autoTrigger)
            {
                auto mouseEvent=static_cast<QMouseEvent*>(event);
                if (mouseEvent->button()==Qt::LeftButton)
                {
                    // mouseEvent->pos() is in the HOST's own local coordinates; this overlay's
                    // own (0,0) sits inset pixels away from the host's, whenever a non-zero
                    // rippleInset is set (see updateGeometryFromHost()) -- translate so a
                    // cursor-origin ripple still starts exactly under the press.
                    start(mouseEvent->pos()-QPoint(pimpl->insetLeft,pimpl->insetTop));
                }
            }
        }
        break;

        case (QEvent::MouseButtonRelease):
        {
            if (pimpl->autoTrigger)
            {
                auto mouseEvent=static_cast<QMouseEvent*>(event);
                if (mouseEvent->button()==Qt::LeftButton)
                {
                    release();
                }
            }
        }
        break;

        case (QEvent::Leave):
        {
            if (pimpl->autoTrigger)
            {
                release();
            }
        }
        break;

        case (QEvent::EnabledChange):
        case (QEvent::Hide):
        {
            cancel();
        }
        break;

        default:
        break;
    }

    // Never consume the event -- the overlay only observes the host, it must not interfere
    // with the host's own (or its children's) normal event handling.
    return false;
}

//--------------------------------------------------------------------------

void RippleOverlay::paintEvent(QPaintEvent* /*event*/)
{
    if (!pimpl->rippleEnabled || pimpl->growValue<=0.0)
    {
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF r=rect();

    QPainterPath clipPath;
    switch (pimpl->clip)
    {
        case (Clip::Rounded):
        {
            clipPath.addRoundedRect(r,pimpl->cornerRadius,pimpl->cornerRadius);
        }
        break;

        case (Clip::Ellipse):
        {
            clipPath.addEllipse(r);
        }
        break;

        case (Clip::Rect):
        {
            clipPath.addRect(r);
        }
        break;
    }
    p.setClipPath(clipPath);

    QPointF origin=(pimpl->origin==Origin::Center) ? r.center() : pimpl->originPoint;

    // Radius needed to just cover the farthest corner from the origin, so a ripple that grows
    // to full size always ends up covering the whole overlay regardless of where it started.
    qreal maxRadius=0.0;
    const QPointF corners[4]={r.topLeft(),r.topRight(),r.bottomLeft(),r.bottomRight()};
    for (const auto& corner : corners)
    {
        maxRadius=qMax(maxRadius,QLineF(origin,corner).length());
    }

    // Independent per-axis scale on the SAME base radius (rather than, say, independently
    // measuring the horizontal/vertical reach to the nearest edge) -- so equal X/Y scales
    // always draw a true circle, and an unequal pair (see IconTextButton's ripple.qss rule for
    // a host with visible text) draws a flattened ellipse without changing where "fully grown"
    // is reached on whichever axis is scaled down.
    auto radiusX=maxRadius*pimpl->radiusScaleX*pimpl->growValue;
    auto radiusY=maxRadius*pimpl->radiusScaleY*pimpl->growValue;

    auto color=pimpl->color;
    color.setAlphaF(qBound(0.0,color.alphaF()*pimpl->opacity*pimpl->fadeValue,1.0));

    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(origin,radiusX,radiusY);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
