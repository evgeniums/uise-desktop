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

/** @file uise/desktop/chatdatesubtitle.cpp
*
*  Defines ChatDateSubtitle.
*
*/

/****************************************************************************/

#include <QEvent>
#include <QResizeEvent>
#include <QPointer>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/datetime.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/chatmessage.hpp>
#include <uise/desktop/chatdatesubtitle.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

class ChatDateSubtitle_p
{
    public:

        enum class State : int
        {
            Hidden,
            FadingIn,
            Visible,
            FadingOut
        };

        AbstractChatSeparatorSection* section=nullptr;

        QGraphicsOpacityEffect* opacityEffect=nullptr;
        QPropertyAnimation* animation=nullptr;

        SingleShotTimer* showTimer=nullptr;
        SingleShotTimer* hideTimer=nullptr;

        State state=State::Hidden;

        //! Whether the current scroll session wants the pill on screen, independent of whether
        //! it is currently suppressed by occlusion (see `occluded` below).
        bool scrollActive=false;

        //! Whether an inline date separator is currently overlapping the pill (see
        //! ChatDateSubtitle::setOccluded()).
        bool occluded=false;

        QDateTime dateTime;
        qreal opacity=0.0;

        int showDelayMs=ChatDateSubtitle::DefaultShowDelayMs;
        int hideDelayMs=ChatDateSubtitle::DefaultHideDelayMs;
        int fadeInDurationMs=ChatDateSubtitle::DefaultFadeInDurationMs;
        int fadeOutDurationMs=ChatDateSubtitle::DefaultFadeOutDurationMs;
        int topOffset=ChatDateSubtitle::DefaultTopOffset;
        qreal maxOpacity=ChatDateSubtitle::DefaultMaxOpacity;
        int occlusionMargin=ChatDateSubtitle::DefaultOcclusionMargin;

        QPointer<QWidget> filterInstalledOn;
};

//--------------------------------------------------------------------------

ChatDateSubtitle::ChatDateSubtitle(QWidget* parent)
    : WidgetQFrame(parent),
      pimpl(std::make_unique<ChatDateSubtitle_p>())
{
    setObjectName("chatDateSubtitle");

    auto l=Layout::horizontal(this);

    pimpl->section=makeWidget<AbstractChatSeparatorSection,ChatSeparatorSection>(this);
    pimpl->section->setType(AbstractChatSeparatorSection::TypeDate);
    pimpl->section->setHLineVisible(false);
    pimpl->section->setClickable(true);
    l->addWidget(pimpl->section,0,Qt::AlignCenter);

    connect(
        pimpl->section,
        &AbstractChatSeparatorSection::clicked,
        this,
        &ChatDateSubtitle::clicked
    );

    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    setVisible(false);

    pimpl->opacityEffect=new QGraphicsOpacityEffect(this);
    pimpl->opacityEffect->setOpacity(0.0);
    setGraphicsEffect(pimpl->opacityEffect);

    pimpl->animation=new QPropertyAnimation(this,"subtitleOpacity",this);
    pimpl->animation->setEasingCurve(QEasingCurve::InOutSine);
    connect(
        pimpl->animation,
        &QPropertyAnimation::finished,
        this,
        [this]()
        {
            if (pimpl->animation->endValue().toReal()==0.0)
            {
                pimpl->state=ChatDateSubtitle_p::State::Hidden;
                setVisible(false);
            }
            else
            {
                pimpl->state=ChatDateSubtitle_p::State::Visible;
            }
        }
    );

    pimpl->showTimer=new SingleShotTimer(this);
    pimpl->hideTimer=new SingleShotTimer(this);

    pimpl->filterInstalledOn=parentWidget();
    if (pimpl->filterInstalledOn)
    {
        pimpl->filterInstalledOn->installEventFilter(this);
    }
}

//--------------------------------------------------------------------------

ChatDateSubtitle::~ChatDateSubtitle()
{}

//--------------------------------------------------------------------------

AbstractChatSeparatorSection* ChatDateSubtitle::section() const
{
    return pimpl->section;
}

//--------------------------------------------------------------------------

void ChatDateSubtitle::setDateTime(const QDateTime& dt, bool withYear)
{
    pimpl->dateTime=dt;

    auto str=dateAsMonthAndDay(dt);
    if (withYear)
    {
        str=QString{"%1, %2"}.arg(str,dt.date().year());
    }
    pimpl->section->setText(str);

    adjustSize();
    updatePosition();
}

//--------------------------------------------------------------------------

QDateTime ChatDateSubtitle::dateTime() const
{
    return pimpl->dateTime;
}

//--------------------------------------------------------------------------

QString ChatDateSubtitle::text() const
{
    return pimpl->section->text();
}

//--------------------------------------------------------------------------

int ChatDateSubtitle::showDelayMs() const
{
    return pimpl->showDelayMs;
}

//--------------------------------------------------------------------------

void ChatDateSubtitle::setShowDelayMs(int value)
{
    pimpl->showDelayMs=value;
}

//--------------------------------------------------------------------------

int ChatDateSubtitle::hideDelayMs() const
{
    return pimpl->hideDelayMs;
}

//--------------------------------------------------------------------------

void ChatDateSubtitle::setHideDelayMs(int value)
{
    pimpl->hideDelayMs=value;
}

//--------------------------------------------------------------------------

int ChatDateSubtitle::fadeInDurationMs() const
{
    return pimpl->fadeInDurationMs;
}

//--------------------------------------------------------------------------

void ChatDateSubtitle::setFadeInDurationMs(int value)
{
    pimpl->fadeInDurationMs=value;
}

//--------------------------------------------------------------------------

int ChatDateSubtitle::fadeOutDurationMs() const
{
    return pimpl->fadeOutDurationMs;
}

//--------------------------------------------------------------------------

void ChatDateSubtitle::setFadeOutDurationMs(int value)
{
    pimpl->fadeOutDurationMs=value;
}

//--------------------------------------------------------------------------

int ChatDateSubtitle::topOffset() const
{
    return pimpl->topOffset;
}

//--------------------------------------------------------------------------

void ChatDateSubtitle::setTopOffset(int value)
{
    pimpl->topOffset=value;
    updatePosition();
}

//--------------------------------------------------------------------------

qreal ChatDateSubtitle::maxOpacity() const
{
    return pimpl->maxOpacity;
}

//--------------------------------------------------------------------------

void ChatDateSubtitle::setMaxOpacity(qreal value)
{
    pimpl->maxOpacity=value;
}

//--------------------------------------------------------------------------

qreal ChatDateSubtitle::subtitleOpacity() const
{
    return pimpl->opacity;
}

//--------------------------------------------------------------------------

void ChatDateSubtitle::setSubtitleOpacity(qreal value)
{
    pimpl->opacity=value;
    pimpl->opacityEffect->setOpacity(value);
}

//--------------------------------------------------------------------------

int ChatDateSubtitle::occlusionMargin() const
{
    return pimpl->occlusionMargin;
}

//--------------------------------------------------------------------------

void ChatDateSubtitle::setOcclusionMargin(int value)
{
    pimpl->occlusionMargin=value;
}

//--------------------------------------------------------------------------

bool ChatDateSubtitle::isOccluded() const
{
    return pimpl->occluded;
}

//--------------------------------------------------------------------------

void ChatDateSubtitle::notifyScrolled()
{
    if (!pimpl->scrollActive)
    {
        if (pimpl->state==ChatDateSubtitle_p::State::FadingOut)
        {
            pimpl->showTimer->cancel();
            pimpl->scrollActive=true;
            fadeIn();
        }
        else
        {
            pimpl->showTimer->shot(
                static_cast<size_t>(showDelayMs()),
                [this]()
                {
                    pimpl->scrollActive=true;
                    fadeIn();
                },
                false
            );
        }
    }

    hideDelayed();
}

//--------------------------------------------------------------------------

void ChatDateSubtitle::hideDelayed()
{
    pimpl->hideTimer->shot(
        static_cast<size_t>(hideDelayMs()),
        [this]()
        {
            pimpl->scrollActive=false;
            fadeOut();
        },
        true
    );
}

//--------------------------------------------------------------------------

void ChatDateSubtitle::hideNow()
{
    pimpl->showTimer->cancel();
    pimpl->hideTimer->cancel();
    pimpl->animation->stop();

    pimpl->state=ChatDateSubtitle_p::State::Hidden;
    pimpl->scrollActive=false;
    pimpl->occluded=false;
    setSubtitleOpacity(0.0);
    setVisible(false);
}

//--------------------------------------------------------------------------

void ChatDateSubtitle::setOccluded(bool enable)
{
    if (pimpl->occluded==enable)
    {
        return;
    }
    pimpl->occluded=enable;

    if (enable)
    {
        // Note: showTimer is deliberately left alone here -- it may be the initial-appearance
        // timer from notifyScrolled(), whose handler also flips scrollActive to true; cancelling
        // it would lose that bookkeeping. fadeIn() already no-ops while occluded, so a pending
        // timer firing during occlusion has no visible effect.
        pimpl->animation->stop();
        pimpl->state=ChatDateSubtitle_p::State::Hidden;
        setSubtitleOpacity(0.0);
        setVisible(false);
    }
    else if (pimpl->scrollActive)
    {
        // Same show delay/fade used for a session's very first reveal, rather than popping the
        // pill back in the instant the inline separator clears it -- avoids the two pills
        // visually "swapping" on the same frame. Re-checks scrollActive at fire time: the scroll
        // session may have ended (hideTimer already fired) while this was pending.
        pimpl->showTimer->shot(
            static_cast<size_t>(showDelayMs()),
            [this]()
            {
                if (pimpl->scrollActive)
                {
                    fadeIn();
                }
            },
            false
        );
    }
}

//--------------------------------------------------------------------------

void ChatDateSubtitle::fadeIn()
{
    if (pimpl->occluded)
    {
        return;
    }

    pimpl->state=ChatDateSubtitle_p::State::FadingIn;

    setVisible(true);
    updatePosition();
    raise();

    pimpl->animation->stop();
    pimpl->animation->setDuration(fadeInDurationMs());
    pimpl->animation->setStartValue(subtitleOpacity());
    pimpl->animation->setEndValue(maxOpacity());
    pimpl->animation->start();
}

//--------------------------------------------------------------------------

void ChatDateSubtitle::fadeOut()
{
    if (pimpl->state==ChatDateSubtitle_p::State::Hidden ||
        pimpl->state==ChatDateSubtitle_p::State::FadingOut)
    {
        return;
    }

    pimpl->state=ChatDateSubtitle_p::State::FadingOut;

    pimpl->animation->stop();
    pimpl->animation->setDuration(fadeOutDurationMs());
    pimpl->animation->setStartValue(subtitleOpacity());
    pimpl->animation->setEndValue(0.0);
    pimpl->animation->start();
}

//--------------------------------------------------------------------------

void ChatDateSubtitle::updatePosition()
{
    auto parent=parentWidget();
    if (parent==nullptr)
    {
        return;
    }

    resize(sizeHint());

    auto x=(parent->width()-width())/2;
    move(x,topOffset());
    raise();
}

//--------------------------------------------------------------------------

bool ChatDateSubtitle::event(QEvent* event)
{
    if (event->type()==QEvent::ParentChange)
    {
        if (pimpl->filterInstalledOn)
        {
            pimpl->filterInstalledOn->removeEventFilter(this);
        }

        pimpl->filterInstalledOn=parentWidget();
        if (pimpl->filterInstalledOn)
        {
            pimpl->filterInstalledOn->installEventFilter(this);
            updatePosition();
        }
    }

    return WidgetQFrame::event(event);
}

//--------------------------------------------------------------------------

bool ChatDateSubtitle::eventFilter(QObject* watched, QEvent* event)
{
    if (watched==pimpl->filterInstalledOn && event->type()==QEvent::Resize)
    {
        updatePosition();
    }

    return WidgetQFrame::eventFilter(watched,event);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
