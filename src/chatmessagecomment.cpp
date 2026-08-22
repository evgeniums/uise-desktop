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

/** @file uise/desktop/src/chatmessagecomment.cpp
*
*  Defines ChatMessageComment.
*
*/

/****************************************************************************/

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/chatmessagetext.hpp>
#include <uise/desktop/chatmessagecomment.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class ChatMessageComment_p
{
    public:

        QBoxLayout* layout=nullptr;

        // constructed here so it exists regardless of whether updateChatMessage() has run yet
        // (e.g. comment() queried before this section is attached to an outer message); wired up
        // for real -- setChatMessage()/setChatContent() forwarded, reparented into our own
        // layout -- from updateChatMessage(), mirroring ChatMessageFiles's own embedded comment.
        ChatMessageText* text=nullptr;
        QString commentText;
        bool commentMarkdown=true;
};

//--------------------------------------------------------------------------

ChatMessageComment::ChatMessageComment(QWidget* parent)
    : AbstractChatMessageComment(parent),
      pimpl(std::make_unique<ChatMessageComment_p>())
{
    pimpl->layout=Layout::horizontal(this);

    pimpl->text=new ChatMessageText(this);
    pimpl->layout->addWidget(pimpl->text,1);

    // Relayed here so a host (e.g. ForwardDialog's Save/"Quote selected" button swap) can react
    // to selection changes on this section via AbstractChatMessageComment alone, without
    // depending on the fact that the text lives in an embedded ChatMessageText -- same idiom
    // ChatMessageFiles uses for its own embedded comment.
    connect(pimpl->text,&AbstractChatMessageBody::selectionChanged,this,&AbstractChatMessageComment::selectionChanged);

    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

ChatMessageComment::~ChatMessageComment()
{}

//--------------------------------------------------------------------------

void ChatMessageComment::setComment(const QString& text, bool markdown)
{
    pimpl->commentText=text;
    pimpl->commentMarkdown=markdown;

    if (text.isEmpty())
    {
        pimpl->text->clearText();
    }
    else
    {
        pimpl->text->loadText(text,markdown);
    }
}

//--------------------------------------------------------------------------

void ChatMessageComment::clearComment()
{
    setComment(QString(),true);
}

//--------------------------------------------------------------------------

QString ChatMessageComment::comment() const
{
    return pimpl->commentText;
}

//--------------------------------------------------------------------------

void ChatMessageComment::clearContentSelection()
{
    pimpl->text->clearContentSelection();
}

//--------------------------------------------------------------------------

QString ChatMessageComment::selectedText() const
{
    return pimpl->text->selectedText();
}

//--------------------------------------------------------------------------

bool ChatMessageComment::hasSelectableText() const
{
    return pimpl->text->hasSelectableText();
}

//--------------------------------------------------------------------------

void ChatMessageComment::setCopyable(bool enable)
{
    pimpl->text->setCopyable(enable);
}

//--------------------------------------------------------------------------

void ChatMessageComment::selectText(const QString& text)
{
    pimpl->text->selectText(text);
}

//--------------------------------------------------------------------------

int ChatMessageComment::bubbleWidthHint(int forMaxWidth)
{
    // pimpl->text is embedded, not wired through the normal section-attach flow, so its own
    // chatContent() must be forwarded manually before it can compute anything -- same reason
    // ChatMessageFiles::bubbleWidthHint() does this for its own embedded comment.
    pimpl->text->setChatContent(chatContent());
    return pimpl->text->bubbleWidthHint(forMaxWidth-horizontalTotalMargin(this))+horizontalTotalMargin(this);
}

//--------------------------------------------------------------------------

void ChatMessageComment::updateMaximumBubbleWidth()
{
    pimpl->text->setChatContent(chatContent());
    pimpl->text->updateMaximumBubbleWidth();
    updateGeometry();
}

//--------------------------------------------------------------------------

void ChatMessageComment::setSelected(bool enable)
{
    Style::setStyleProperty(this,"selected",enable);
}

//--------------------------------------------------------------------------

void ChatMessageComment::setSent(bool enable)
{
    Style::setStyleProperty(this,"sent",enable);
}

//--------------------------------------------------------------------------

void ChatMessageComment::updateChatMessage()
{
    // setChatMessage() no longer reparents pimpl->text away (see AbstractChatMessageChild::
    // setChatMessage()), so this addWidget() is now just asserting the layout slot -- and it
    // short-circuits the reparent inside QLayout::addChildWidget(), since pimpl->text is
    // already a child of this widget.
    pimpl->text->setChatMessage(chatMessage());
    pimpl->layout->addWidget(pimpl->text,1);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
