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

/** @file uise/desktop/chatmessageerror.cpp
*
*  Defines ChatMessageError.
*
*/

/****************************************************************************/

#include <QLabel>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/chatmessageerror.hpp>
#include <uise/desktop/roundedimage.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/********************************ChatMessageError****************************/

//--------------------------------------------------------------------------

class ChatMessageError_p
{
    public:

        QBoxLayout* layout=nullptr;
        WithRoundedImage* errorIcon=nullptr;
        QLabel* title=nullptr;
        QLabel* description=nullptr;
};

//--------------------------------------------------------------------------

ChatMessageError::ChatMessageError(QWidget* parent)
    : AbstractChatMessageError(parent),
      pimpl(std::make_unique<ChatMessageError_p>())
{
    pimpl->layout=Layout::horizontal(this);

    pimpl->errorIcon=new WithRoundedImage(this);
    pimpl->errorIcon->setObjectName("errorIcon");
    pimpl->layout->addWidget(pimpl->errorIcon);

    auto textLayout=new QVBoxLayout();
    Layout::clear(textLayout);
    pimpl->layout->addLayout(textLayout);

    pimpl->title=new QLabel(this);
    pimpl->title->setObjectName("title");
    pimpl->title->setWordWrap(true);
    textLayout->addWidget(pimpl->title);

    pimpl->description=new QLabel(this);
    pimpl->description->setObjectName("description");
    pimpl->description->setWordWrap(true);
    textLayout->addWidget(pimpl->description);

    setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

ChatMessageError::~ChatMessageError()=default;

//--------------------------------------------------------------------------

void ChatMessageError::updateReason()
{
    pimpl->title->setText(formatTitle());

    auto description=formatDescription();
    pimpl->description->setText(description);
    pimpl->description->setVisible(!description.isEmpty());

    updateIcon();
}

//--------------------------------------------------------------------------

void ChatMessageError::updateChatMessage()
{
    updateReason();
}

//--------------------------------------------------------------------------

void ChatMessageError::updateIcon()
{
    pimpl->errorIcon->image()->setSvgIcon(
        Style::instance().svgIconLocator().icon("ChatMessageErrorIcon::warning",this)
    );
}

//--------------------------------------------------------------------------

void ChatMessageError::presetTitle(const QString& text)
{
    pimpl->title->setText(text);
}

//--------------------------------------------------------------------------

void ChatMessageError::presetDescription(const QString& text)
{
    pimpl->description->setText(text);
    pimpl->description->setVisible(!text.isEmpty());
}

//--------------------------------------------------------------------------

void ChatMessageError::presetIcon(const QString& icon)
{
    pimpl->errorIcon->image()->setSvgIcon(Style::instance().svgIconLocator().icon(icon,this));
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
