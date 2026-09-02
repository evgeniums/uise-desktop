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

/** @file uise/desktop/chatfloatingavatar.cpp
*
*  Defines ChatFloatingAvatar.
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
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/avatar.hpp>
#include <uise/desktop/abstractchatmessage.hpp>
#include <uise/desktop/chatfloatingavatar.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

class ChatFloatingAvatar_p
{
    public:

        enum class State : int
        {
            Hidden,
            FadingIn,
            Visible,
            FadingOut
        };

        AvatarWidget* avatar=nullptr;

        QGraphicsOpacityEffect* opacityEffect=nullptr;
        QPropertyAnimation* animation=nullptr;

        SingleShotTimer* showTimer=nullptr;
        SingleShotTimer* hideTimer=nullptr;

        State state=State::Hidden;

        //! The embedding view wants a floating avatar shown at all -- see ChatFloatingAvatar's
        //! own doc comment. The only input to this widget's visibility.
        bool wanted=false;

        QPointer<AbstractChatMessage> message;

        int targetColumnLeft=0;
        int targetColumnWidth=0;
        int clampTopY=0;
        std::optional<int> anchoredTopY;

        qreal opacity=0.0;

        int showDelayMs=ChatFloatingAvatar::DefaultShowDelayMs;
        int hideDelayMs=ChatFloatingAvatar::DefaultHideDelayMs;
        int fadeInDurationMs=ChatFloatingAvatar::DefaultFadeInDurationMs;
        int fadeOutDurationMs=ChatFloatingAvatar::DefaultFadeOutDurationMs;
        int bottomOffset=ChatFloatingAvatar::DefaultBottomOffset;
        qreal maxOpacity=ChatFloatingAvatar::DefaultMaxOpacity;
        int occlusionMargin=ChatFloatingAvatar::DefaultOcclusionMargin;
        int modeChangeBlockMs=ChatFloatingAvatar::DefaultModeChangeBlockMs;

        QPointer<QWidget> filterInstalledOn;
};

//--------------------------------------------------------------------------

ChatFloatingAvatar::ChatFloatingAvatar(QWidget* parent)
    : WidgetQFrame(parent),
      pimpl(std::make_unique<ChatFloatingAvatar_p>())
{
    setObjectName("chatFloatingAvatar");

    auto l=Layout::horizontal(this);

    pimpl->avatar=new AvatarWidget(this);
    l->addWidget(pimpl->avatar,0,Qt::AlignCenter);

    // Clickable affordance -- mirrors ChatMessageAvatar's own anchored avatar wiring
    // (src/chatmessage.cpp), so the floating copy looks and behaves identically.
    pimpl->avatar->setClickable(true);
    pimpl->avatar->setCursor(Qt::PointingHandCursor);
    connect(
        pimpl->avatar,
        &AvatarWidget::clicked,
        this,
        &ChatFloatingAvatar::clicked
    );

    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    setVisible(false);

    pimpl->opacityEffect=new QGraphicsOpacityEffect(this);
    pimpl->opacityEffect->setOpacity(0.0);
    setGraphicsEffect(pimpl->opacityEffect);

    pimpl->animation=new QPropertyAnimation(this,"avatarOpacity",this);
    pimpl->animation->setEasingCurve(QEasingCurve::InOutSine);
    connect(
        pimpl->animation,
        &QPropertyAnimation::finished,
        this,
        [this]()
        {
            if (pimpl->animation->endValue().toReal()==0.0)
            {
                pimpl->state=ChatFloatingAvatar_p::State::Hidden;
                setVisible(false);
            }
            else
            {
                pimpl->state=ChatFloatingAvatar_p::State::Visible;
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

ChatFloatingAvatar::~ChatFloatingAvatar()
{}

//--------------------------------------------------------------------------

AvatarWidget* ChatFloatingAvatar::avatar() const
{
    return pimpl->avatar;
}

//--------------------------------------------------------------------------

void ChatFloatingAvatar::setMessage(AbstractChatMessage* msg)
{
    if (msg==nullptr)
    {
        pimpl->message.clear();
        return;
    }

    auto* src=msg->avatarWidget();
    if (src==nullptr)
    {
        pimpl->message.clear();
        return;
    }

    pimpl->message=msg;

    // Each guarded on equality: setAvatarSource()/setAvatarPath() both re-trigger the underlying
    // pixmap source's own fetch, and this method runs on every scroll frame -- re-applying
    // unchanged data would spam that fetch for nothing. Same order as whitemdesktop's
    // ChatMessage::refreshAvatar() (source, path, name, size).
    if (pimpl->avatar->avatarSource()!=src->avatarSource())
    {
        pimpl->avatar->setAvatarSource(src->avatarSource());
    }
    if (pimpl->avatar->avatarPath().toString()!=src->avatarPath().toString())
    {
        pimpl->avatar->setAvatarPath(src->avatarPath());
    }
    if (pimpl->avatar->avatarName()!=src->avatarName())
    {
        // AvatarWidget::setAvatarName() only stores the name -- it does not itself schedule a
        // repaint (see its own doc comment), unlike setAvatarSource()/setAvatarPath() which call
        // updateBackgroundColor()->update() internally.
        pimpl->avatar->setAvatarName(src->avatarName());
        pimpl->avatar->update();
    }
    if (pimpl->avatar->size()!=src->size())
    {
        // Mirrors ChatMessageAvatar::setAvatarSize(): just resize the widget itself: RoundedImage
        // self-heals its own imageSize() from size() on the next paintEvent() while autoSize()
        // (the default) is on -- no need to also call setAvatarSize(QSize) here.
        pimpl->avatar->setFixedSize(src->size());
        adjustSize();
        updatePosition();
    }
}

//--------------------------------------------------------------------------

AbstractChatMessage* ChatFloatingAvatar::message() const
{
    return pimpl->message;
}

//--------------------------------------------------------------------------

void ChatFloatingAvatar::setTargetColumn(int left, int width)
{
    if (pimpl->targetColumnLeft!=left || pimpl->targetColumnWidth!=width)
    {
        pimpl->targetColumnLeft=left;
        pimpl->targetColumnWidth=width;
        updatePosition();
    }
}

//--------------------------------------------------------------------------

void ChatFloatingAvatar::setClampTopY(int y)
{
    if (pimpl->clampTopY!=y)
    {
        pimpl->clampTopY=y;
        updatePosition();
    }
}

//--------------------------------------------------------------------------

void ChatFloatingAvatar::setAnchoredTopY(std::optional<int> y)
{
    if (pimpl->anchoredTopY!=y)
    {
        pimpl->anchoredTopY=y;
        updatePosition();
    }
}

//--------------------------------------------------------------------------

int ChatFloatingAvatar::showDelayMs() const
{
    return pimpl->showDelayMs;
}

//--------------------------------------------------------------------------

void ChatFloatingAvatar::setShowDelayMs(int value)
{
    pimpl->showDelayMs=value;
}

//--------------------------------------------------------------------------

int ChatFloatingAvatar::hideDelayMs() const
{
    return pimpl->hideDelayMs;
}

//--------------------------------------------------------------------------

void ChatFloatingAvatar::setHideDelayMs(int value)
{
    pimpl->hideDelayMs=value;
}

//--------------------------------------------------------------------------

int ChatFloatingAvatar::fadeInDurationMs() const
{
    return pimpl->fadeInDurationMs;
}

//--------------------------------------------------------------------------

void ChatFloatingAvatar::setFadeInDurationMs(int value)
{
    pimpl->fadeInDurationMs=value;
}

//--------------------------------------------------------------------------

int ChatFloatingAvatar::fadeOutDurationMs() const
{
    return pimpl->fadeOutDurationMs;
}

//--------------------------------------------------------------------------

void ChatFloatingAvatar::setFadeOutDurationMs(int value)
{
    pimpl->fadeOutDurationMs=value;
}

//--------------------------------------------------------------------------

int ChatFloatingAvatar::bottomOffset() const
{
    return pimpl->bottomOffset;
}

//--------------------------------------------------------------------------

void ChatFloatingAvatar::setBottomOffset(int value)
{
    if (pimpl->bottomOffset!=value)
    {
        pimpl->bottomOffset=value;
        updatePosition();
    }
}

//--------------------------------------------------------------------------

qreal ChatFloatingAvatar::maxOpacity() const
{
    return pimpl->maxOpacity;
}

//--------------------------------------------------------------------------

void ChatFloatingAvatar::setMaxOpacity(qreal value)
{
    pimpl->maxOpacity=value;
}

//--------------------------------------------------------------------------

qreal ChatFloatingAvatar::avatarOpacity() const
{
    return pimpl->opacity;
}

//--------------------------------------------------------------------------

void ChatFloatingAvatar::setAvatarOpacity(qreal value)
{
    pimpl->opacity=value;
    pimpl->opacityEffect->setOpacity(value);
}

//--------------------------------------------------------------------------

int ChatFloatingAvatar::occlusionMargin() const
{
    return pimpl->occlusionMargin;
}

//--------------------------------------------------------------------------

void ChatFloatingAvatar::setOcclusionMargin(int value)
{
    pimpl->occlusionMargin=value;
}

//--------------------------------------------------------------------------

int ChatFloatingAvatar::modeChangeBlockMs() const
{
    return pimpl->modeChangeBlockMs;
}

//--------------------------------------------------------------------------

void ChatFloatingAvatar::setModeChangeBlockMs(int value)
{
    pimpl->modeChangeBlockMs=value;
}

//--------------------------------------------------------------------------

bool ChatFloatingAvatar::isWanted() const
{
    return pimpl->wanted;
}

//--------------------------------------------------------------------------

void ChatFloatingAvatar::setWanted(bool enable)
{
    if (pimpl->wanted==enable)
    {
        return;
    }
    pimpl->wanted=enable;

    if (!enable)
    {
        pimpl->showTimer->cancel();
        if (hideDelayMs()<=0)
        {
            fadeOut();
        }
        else
        {
            pimpl->hideTimer->shot(static_cast<size_t>(hideDelayMs()),[this](){fadeOut();},true);
        }
        return;
    }

    pimpl->hideTimer->cancel();
    if (showDelayMs()<=0)
    {
        fadeIn();
    }
    else
    {
        pimpl->showTimer->shot(
            static_cast<size_t>(showDelayMs()),
            [this]()
            {
                // Re-checks at fire time: the view may have stopped wanting it while pending.
                if (pimpl->wanted)
                {
                    fadeIn();
                }
            },
            false
        );
    }
}

//--------------------------------------------------------------------------

void ChatFloatingAvatar::hideNow()
{
    pimpl->showTimer->cancel();
    pimpl->hideTimer->cancel();
    pimpl->animation->stop();

    pimpl->state=ChatFloatingAvatar_p::State::Hidden;
    pimpl->wanted=false;
    pimpl->message.clear();
    setAvatarOpacity(0.0);
    setVisible(false);
}

//--------------------------------------------------------------------------

void ChatFloatingAvatar::fadeIn()
{
    if (!pimpl->wanted)
    {
        return;
    }

    setVisible(true);
    updatePosition();
    raise();

    pimpl->animation->stop();

    // Instant by default -- see fadeInDurationMs()'s own doc comment. This copy appears in the
    // exact place a row's own avatar is being hidden in the same pass, so anything less than a
    // straight swap shows up as a blink where neither is on screen.
    if (fadeInDurationMs()<=0)
    {
        pimpl->state=ChatFloatingAvatar_p::State::Visible;
        setAvatarOpacity(maxOpacity());
        return;
    }

    pimpl->state=ChatFloatingAvatar_p::State::FadingIn;
    pimpl->animation->setDuration(fadeInDurationMs());
    pimpl->animation->setStartValue(avatarOpacity());
    pimpl->animation->setEndValue(maxOpacity());
    pimpl->animation->start();
}

//--------------------------------------------------------------------------

void ChatFloatingAvatar::fadeOut()
{
    if (pimpl->state==ChatFloatingAvatar_p::State::Hidden ||
        pimpl->state==ChatFloatingAvatar_p::State::FadingOut)
    {
        return;
    }

    pimpl->animation->stop();

    // Instant by default, same reasoning as fadeIn(): the row this copy was standing in for gets
    // its own avatar back in the same pass.
    if (fadeOutDurationMs()<=0)
    {
        pimpl->state=ChatFloatingAvatar_p::State::Hidden;
        setAvatarOpacity(0.0);
        setVisible(false);
        return;
    }

    pimpl->state=ChatFloatingAvatar_p::State::FadingOut;
    pimpl->animation->setDuration(fadeOutDurationMs());
    pimpl->animation->setStartValue(avatarOpacity());
    pimpl->animation->setEndValue(0.0);
    pimpl->animation->start();
}

//--------------------------------------------------------------------------

void ChatFloatingAvatar::updatePosition()
{
    auto parent=parentWidget();
    if (parent==nullptr)
    {
        return;
    }

    resize(sizeHint());

    if (layout()!=nullptr)
    {
        // So the image's offset inside this wrapper, read below, is current rather than one
        // layout pass stale. Everything here is computed for the IMAGE -- that is what has to
        // line up with the rows' own avatars -- and only converted back to this wrapper's
        // top-left at the end, so any inset the wrapper picks up cannot shift the result.
        layout()->activate();
    }
    auto image=pimpl->avatar->geometry();

    // Centred in the target column exactly the way a row centres its own avatar
    // (QBoxLayout + Qt::AlignHCenter), so the two share the same left margin to the pixel.
    auto imageX=pimpl->targetColumnLeft+(pimpl->targetColumnWidth-image.width())/2;

    // Natural resting place: just above the viewport's bottom edge. Then pulled UP to the row's
    // own anchored avatar once that has scrolled high enough (so the two coincide exactly rather
    // than this copy sinking past it), and finally pushed DOWN again if that would take it above
    // the batch's first bubble. The order matters: the batch-head clamp wins, so a batch only
    // partly scrolled in never has its avatar floating above where the batch itself starts.
    auto imageY=parent->height()-bottomOffset()-image.height();
    if (pimpl->anchoredTopY.has_value())
    {
        imageY=qMin(imageY,*pimpl->anchoredTopY);
    }
    imageY=qMax(imageY,pimpl->clampTopY);

    move(imageX-image.left(),imageY-image.top());
}

//--------------------------------------------------------------------------

bool ChatFloatingAvatar::event(QEvent* event)
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

bool ChatFloatingAvatar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched==pimpl->filterInstalledOn && event->type()==QEvent::Resize)
    {
        updatePosition();
    }

    return WidgetQFrame::eventFilter(watched,event);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
