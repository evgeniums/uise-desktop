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

/** @file uise/desktop/src/chatmessagereply.cpp
*
*  Defines ChatMessageReply.
*
*/

/****************************************************************************/

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/abstractreplypreview.hpp>
#include <uise/desktop/replypreview.hpp>
#include <uise/desktop/chatmessagereply.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class ChatMessageReply_p
{
    public:

        QBoxLayout* layout;
        AbstractReplyPreview* preview;
};

//--------------------------------------------------------------------------

ChatMessageReply::ChatMessageReply(QWidget* parent)
    : AbstractChatMessageReply(parent),
      pimpl(std::make_unique<ChatMessageReply_p>())
{
    pimpl->layout=Layout::horizontal(this);

    pimpl->preview=makeWidget<AbstractReplyPreview,ReplyPreview>(this);
    pimpl->layout->addWidget(pimpl->preview,1);

    connect(pimpl->preview,&AbstractReplyPreview::clicked,this,&AbstractChatMessageReply::clicked);

    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

ChatMessageReply::~ChatMessageReply()
{}

//--------------------------------------------------------------------------

void ChatMessageReply::setReplyData(ReplyPreviewData data)
{
    pimpl->preview->setData(std::move(data));
}

//--------------------------------------------------------------------------

const ReplyPreviewData& ChatMessageReply::replyData() const
{
    return pimpl->preview->data();
}

//--------------------------------------------------------------------------

AbstractReplyPreview* ChatMessageReply::preview() const
{
    return pimpl->preview;
}

//--------------------------------------------------------------------------

void ChatMessageReply::setOriginalDeleted(bool enable)
{
    auto data=pimpl->preview->data();
    data.setDeleted(enable);
    pimpl->preview->setData(std::move(data));
}

//--------------------------------------------------------------------------

bool ChatMessageReply::isOriginalDeleted() const
{
    return pimpl->preview->data().isDeleted();
}

//--------------------------------------------------------------------------

int ChatMessageReply::bubbleWidthHint(int forMaxWidth)
{
    return pimpl->preview->contentWidthHint(forMaxWidth-horizontalTotalMargin(this))+horizontalTotalMargin(this);
}

//--------------------------------------------------------------------------

void ChatMessageReply::updateMaximumBubbleWidth()
{
    pimpl->preview->setContentMaxWidth(chatContent()->maximumBubbleWidth()-horizontalTotalMargin(this));
    updateGeometry();
}

//--------------------------------------------------------------------------

void ChatMessageReply::setSelected(bool enable)
{
    Style::setStyleProperty(this,"selected",enable);
}

//--------------------------------------------------------------------------

void ChatMessageReply::setSent(bool enable)
{
    Style::setStyleProperty(this,"sent",enable);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
