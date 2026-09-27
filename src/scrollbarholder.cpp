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

/** @file uise/desktop/scrollbarholder.cpp
*
*  Defines ScrollBarHolder.
*
*/

/****************************************************************************/

#include <QEvent>
#include <QPointer>
#include <QGraphicsOpacityEffect>
#include <QVariantAnimation>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/scrollbarholder.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

class ScrollBarHolder_p
{
    public:

        enum class State
        {
            Hidden,
            FadingIn,
            Shown,
            FadingOut
        };

        Qt::Orientation orientation=Qt::Vertical;
        QScrollBar* bar=nullptr;

        bool needed=false;
        bool holdPlace=false;
        bool autoHide=false;

        QPointer<QWidget> hoverTarget;
        bool hovered=false;

        State state=State::Hidden;
        QGraphicsOpacityEffect* effect=nullptr;
        QVariantAnimation* animation=nullptr;
        SingleShotTimer* hideTimer=nullptr;

        int fadeInMs=ScrollBarHolder::DefaultFadeInMs;
        int fadeOutMs=ScrollBarHolder::DefaultFadeOutMs;
        int hideDelayMs=ScrollBarHolder::DefaultHideDelayMs;

        //! Kept installed for as long as autoHide is enabled -- see the class doc comment in
        //! scrollbarholder.hpp for why that is safe here (unlike a per-item effect inside the
        //! scrolled content).
        void ensureEffect()
        {
            if (effect==nullptr)
            {
                effect=new QGraphicsOpacityEffect(bar);
                effect->setOpacity(state==State::Hidden ? 0.0 : 1.0);
                bar->setGraphicsEffect(effect);
            }
        }

        void removeEffect()
        {
            if (effect!=nullptr)
            {
                // QWidget::setGraphicsEffect() deletes the previously installed effect.
                bar->setGraphicsEffect(nullptr);
                effect=nullptr;
            }
        }

        void applyOpacity(qreal value)
        {
            if (effect!=nullptr)
            {
                effect->setOpacity(value);
            }
        }

        void stopAnimation()
        {
            if (animation->state()==QAbstractAnimation::Running)
            {
                animation->stop();
            }
        }

        //! Set state and opacity immediately, with no animation.
        void snapTo(State s)
        {
            stopAnimation();
            state=s;
            if (autoHide)
            {
                ensureEffect();
                applyOpacity(s==State::Hidden ? 0.0 : 1.0);
            }
        }

        //! Animate the handle to fully opaque (show=true) or fully transparent (show=false).
        //! No-op unless auto-hide is enabled and the bar is currently needed.
        void fadeTo(bool show, int durationMs)
        {
            if (!autoHide || !needed)
            {
                return;
            }
            if (show && (state==State::Shown || state==State::FadingIn))
            {
                return;
            }
            if (!show && (state==State::Hidden || state==State::FadingOut))
            {
                return;
            }

            ensureEffect();
            stopAnimation();

            auto from=effect->opacity();
            auto to=show ? 1.0 : 0.0;
            state=show ? State::FadingIn : State::FadingOut;

            animation->setDuration(durationMs);
            animation->setStartValue(from);
            animation->setEndValue(to);
            animation->start();
        }
};

//--------------------------------------------------------------------------

ScrollBarHolder::ScrollBarHolder(Qt::Orientation orientation, QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<ScrollBarHolder_p>())
{
    pimpl->orientation=orientation;

    // Orthogonal outer layout, matching the original VerticalScrollBar (a horizontal layout
    // around a single vertical bar) -- doesn't matter functionally with just one child widget,
    // kept for consistency.
    auto l=Layout::box(this,Layout::orthOrientation(orientation));

    pimpl->bar=new QScrollBar(this);
    pimpl->bar->setOrientation(orientation);
    l->addWidget(pimpl->bar);

    if (orientation==Qt::Vertical)
    {
        setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);
    }
    else
    {
        setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
    }

    pimpl->animation=new QVariantAnimation(this);
    connect(
        pimpl->animation,
        &QVariantAnimation::valueChanged,
        this,
        [this](const QVariant& value)
        {
            pimpl->applyOpacity(value.toReal());
        }
    );
    connect(
        pimpl->animation,
        &QVariantAnimation::finished,
        this,
        [this]()
        {
            pimpl->state=(pimpl->animation->endValue().toReal()>=1.0)
                ? ScrollBarHolder_p::State::Shown
                : ScrollBarHolder_p::State::Hidden;
        }
    );

    pimpl->hideTimer=new SingleShotTimer(this);

    // If the drag ends outside the view, fade back out immediately rather than waiting for the
    // hide delay -- the mouse has already left, notifyUserScrolled() just never got a chance to
    // arm the timer because Leave was deferred while the slider was down (see eventFilter()).
    connect(
        pimpl->bar,
        &QScrollBar::sliderReleased,
        this,
        [this]()
        {
            if (!pimpl->hovered)
            {
                pimpl->hideTimer->cancel();
                pimpl->fadeTo(false,pimpl->fadeOutMs);
            }
        }
    );
}

//--------------------------------------------------------------------------

ScrollBarHolder::~ScrollBarHolder()
{}

//--------------------------------------------------------------------------

QSize ScrollBarHolder::minimumSizeHint() const
{
    return pimpl->bar->minimumSizeHint();
}

//--------------------------------------------------------------------------

QSize ScrollBarHolder::sizeHint() const
{
    return pimpl->bar->sizeHint();
}

//--------------------------------------------------------------------------

QScrollBar* ScrollBarHolder::bar() const
{
    return pimpl->bar;
}

//--------------------------------------------------------------------------

void ScrollBarHolder::setHoldPlace(bool enable)
{
    pimpl->holdPlace=enable;
    if (!pimpl->needed)
    {
        QFrame::setVisible(pimpl->holdPlace);
    }
}

//--------------------------------------------------------------------------

bool ScrollBarHolder::isHoldPlace() const
{
    return pimpl->holdPlace;
}

//--------------------------------------------------------------------------

void ScrollBarHolder::setVisible(bool enable)
{
    pimpl->needed=enable;
    if (!enable)
    {
        QFrame::setVisible(pimpl->holdPlace);
    }
    else
    {
        QFrame::setVisible(true);
    }
    pimpl->bar->setVisible(enable);

    if (!enable)
    {
        pimpl->hideTimer->cancel();
        pimpl->snapTo(ScrollBarHolder_p::State::Hidden);
    }
    else
    {
        // Newly needed (content just started overflowing, or the policy just turned it on) --
        // snap straight to the right state for the current hover, no fade-in flash.
        pimpl->snapTo(pimpl->hovered ? ScrollBarHolder_p::State::Shown : ScrollBarHolder_p::State::Hidden);
    }
}

//--------------------------------------------------------------------------

bool ScrollBarHolder::isVisible() const
{
    return pimpl->needed;
}

//--------------------------------------------------------------------------

void ScrollBarHolder::setAutoHide(bool enable)
{
    if (pimpl->autoHide==enable)
    {
        return;
    }
    pimpl->autoHide=enable;
    pimpl->hideTimer->cancel();
    pimpl->stopAnimation();

    if (enable)
    {
        pimpl->ensureEffect();
        pimpl->state=pimpl->hovered ? ScrollBarHolder_p::State::Shown : ScrollBarHolder_p::State::Hidden;
        pimpl->applyOpacity(pimpl->hovered ? 1.0 : 0.0);
    }
    else
    {
        pimpl->state=ScrollBarHolder_p::State::Shown;
        pimpl->removeEffect();
    }
}

//--------------------------------------------------------------------------

bool ScrollBarHolder::isAutoHide() const
{
    return pimpl->autoHide;
}

//--------------------------------------------------------------------------

void ScrollBarHolder::setHoverTarget(QWidget* target)
{
    if (pimpl->hoverTarget)
    {
        pimpl->hoverTarget->removeEventFilter(this);
    }

    pimpl->hoverTarget=target;
    pimpl->hovered=(target!=nullptr) && target->underMouse();

    if (pimpl->hoverTarget)
    {
        pimpl->hoverTarget->installEventFilter(this);
    }

    if (pimpl->autoHide && pimpl->needed)
    {
        pimpl->snapTo(pimpl->hovered ? ScrollBarHolder_p::State::Shown : ScrollBarHolder_p::State::Hidden);
    }
}

//--------------------------------------------------------------------------

QWidget* ScrollBarHolder::hoverTarget() const
{
    return pimpl->hoverTarget;
}

//--------------------------------------------------------------------------

void ScrollBarHolder::notifyUserScrolled()
{
    if (!pimpl->autoHide || !pimpl->needed)
    {
        return;
    }

    pimpl->hideTimer->cancel();
    pimpl->fadeTo(true,pimpl->fadeInMs);

    if (!pimpl->hovered)
    {
        pimpl->hideTimer->shot(
            static_cast<size_t>(pimpl->hideDelayMs),
            [this]()
            {
                if (!pimpl->hovered)
                {
                    pimpl->fadeTo(false,pimpl->fadeOutMs);
                }
            },
            true
        );
    }
}

//--------------------------------------------------------------------------

void ScrollBarHolder::setFadeInDurationMs(int value)
{
    pimpl->fadeInMs=value;
}

//--------------------------------------------------------------------------

int ScrollBarHolder::fadeInDurationMs() const
{
    return pimpl->fadeInMs;
}

//--------------------------------------------------------------------------

void ScrollBarHolder::setFadeOutDurationMs(int value)
{
    pimpl->fadeOutMs=value;
}

//--------------------------------------------------------------------------

int ScrollBarHolder::fadeOutDurationMs() const
{
    return pimpl->fadeOutMs;
}

//--------------------------------------------------------------------------

void ScrollBarHolder::setHideDelayMs(int value)
{
    pimpl->hideDelayMs=value;
}

//--------------------------------------------------------------------------

int ScrollBarHolder::hideDelayMs() const
{
    return pimpl->hideDelayMs;
}

//--------------------------------------------------------------------------

bool ScrollBarHolder::eventFilter(QObject* watched, QEvent* event)
{
    if (watched==pimpl->hoverTarget)
    {
        switch (event->type())
        {
            case QEvent::Enter:
                pimpl->hovered=true;
                pimpl->hideTimer->cancel();
                pimpl->fadeTo(true,pimpl->fadeInMs);
            break;

            case QEvent::Leave:
                pimpl->hovered=false;
                if (!pimpl->bar->isSliderDown())
                {
                    // While the slider is down, keep the handle shown until sliderReleased --
                    // see the constructor's connection to that signal.
                    pimpl->hideTimer->cancel();
                    pimpl->fadeTo(false,pimpl->fadeOutMs);
                }
            break;

            case QEvent::Hide:
                pimpl->hovered=false;
                pimpl->hideTimer->cancel();
                pimpl->snapTo(ScrollBarHolder_p::State::Hidden);
            break;

            default:
            break;
        }
    }

    return QFrame::eventFilter(watched,event);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
