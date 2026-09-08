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

/** @file uise/desktop/chatmessagecall.cpp
*
*  Defines ChatMessageCall.
*
*/

/****************************************************************************/

#include <QLabel>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/chatmessagecall.hpp>
#include <uise/desktop/roundedimage.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/********************************ChatMessageCall****************************/

//--------------------------------------------------------------------------

class ChatMessageCall_p
{
    public:

        QBoxLayout* layout=nullptr;
        WithRoundedImage* callIcon=nullptr;
        QLabel* text=nullptr;
};

//--------------------------------------------------------------------------

ChatMessageCall::ChatMessageCall(QWidget* parent)
    : AbstractChatMessageCall(parent),
      pimpl(std::make_unique<ChatMessageCall_p>())
{
    pimpl->layout=Layout::horizontal(this);

    pimpl->callIcon=new WithRoundedImage(this);
    pimpl->callIcon->setObjectName("callIcon");
    pimpl->layout->addWidget(pimpl->callIcon);

    pimpl->text=new QLabel(this);
    pimpl->text->setObjectName("text");
    pimpl->layout->addWidget(pimpl->text);

    setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

ChatMessageCall::~ChatMessageCall()=default;

//--------------------------------------------------------------------------

void ChatMessageCall::updateDuration()
{
    if (status()==Status::Complete)
    {
        pimpl->text->setText(formatText());
    }
}

//--------------------------------------------------------------------------

void ChatMessageCall::updateStatus()
{
    pimpl->text->setText(formatText());
    updateIcon();
}

//--------------------------------------------------------------------------

void ChatMessageCall::updateChatMessage()
{
    pimpl->text->setText(formatText());
    updateIcon();
}

//--------------------------------------------------------------------------

void ChatMessageCall::updateIcon()
{
    // chatMessage() can be null here if a caller drives status/duration before setWidgets() has
    // attached the chat message -- same precondition formatText() already guards. Bail rather
    // than dereference; the caller is expected to (re)call updateIcon() once the message is set.
    if (chatMessage()==nullptr)
    {
        return;
    }

    QString icon;
    if (status()==Status::Complete)
    {
        if (chatMessage()->isIncoming())
        {
            icon="CallIcon::incoming";
        }
        else
        {
            icon="CallIcon::outgoing";
        }
    }
    else
    {
        if (chatMessage()->isIncoming())
        {
            icon="CallFailedIcon::incoming";
        }
        else
        {
            icon="CallFailedIcon::outgoing";
        }
    }
    pimpl->callIcon->image()->setSvgIcon(Style::instance().svgIconLocator().icon(icon,this));
}

//--------------------------------------------------------------------------

void ChatMessageCall::presetText(const QString& text)
{
    pimpl->text->setText(text);
}

//--------------------------------------------------------------------------

void ChatMessageCall::presetIcon(const QString& icon)
{
    pimpl->callIcon->image()->setSvgIcon(Style::instance().svgIconLocator().icon(icon,this));
}

//--------------------------------------------------------------------------

QRect ChatMessageCall::lastTextLineRect() const
{
    // No bubbleWidthHint()/updateMaximumBubbleWidth() override exists for this body -- its width
    // negotiation already goes through the ChatMessageContentSection base default (sizeHint().
    // width()), so treating that same sizeHint() as this body's one "line" is consistent with
    // what the bubble was actually negotiated against, and needs no separate wrap-pin bookkeeping
    // the way ChatMessageText needs (there is nothing here that ever wraps).
    auto s=sizeHint();
    if (s.width()<=0)
    {
        return {};
    }
    return QRect{0,0,s.width(),s.height()};
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
