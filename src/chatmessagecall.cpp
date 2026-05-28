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
        WithRoundedImage* directionIcon=nullptr;
        WithRoundedImage* failedDirectionIcon=nullptr;

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
    pimpl->callIcon->image()->setSvgIcon(Style::instance().svgIconLocator().icon("ChatMessageCall::call",this));

    pimpl->directionIcon=new WithRoundedImage(this);
    pimpl->directionIcon->setObjectName("directionIcon");
    pimpl->layout->addWidget(pimpl->directionIcon);

    pimpl->failedDirectionIcon=new WithRoundedImage(this);
    pimpl->failedDirectionIcon->setObjectName("failedDirectionIcon");
    pimpl->layout->addWidget(pimpl->failedDirectionIcon);

    pimpl->text=new QLabel(this);
    pimpl->text->setObjectName("text");
    pimpl->layout->addWidget(pimpl->text);

    setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Fixed);

    pimpl->directionIcon->setVisible(false);
    pimpl->failedDirectionIcon->setVisible(false);
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

    pimpl->directionIcon->setVisible(status()==Status::Complete);
    pimpl->failedDirectionIcon->setVisible(status()!=Status::Complete);
}

//--------------------------------------------------------------------------

void ChatMessageCall::updateChatMessage()
{
    if (chatMessage()->isIncoming())
    {
        pimpl->directionIcon->image()->setSvgIcon(Style::instance().svgIconLocator().icon("ChatMessageCall::incoming",this));
        pimpl->failedDirectionIcon->image()->setSvgIcon(Style::instance().svgIconLocator().icon("ChatMessageCallFailed::incoming",this));
    }
    else
    {
        pimpl->directionIcon->image()->setSvgIcon(Style::instance().svgIconLocator().icon("ChatMessageCall::outgoing",this));
        pimpl->failedDirectionIcon->image()->setSvgIcon(Style::instance().svgIconLocator().icon("ChatMessageCallFailed::outgoing",this));
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
