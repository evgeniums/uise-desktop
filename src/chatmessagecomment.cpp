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

#include <limits>

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
        TextFormat commentFormat=TextFormat::Markdown;
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

    // Same idiom, for a clicked hyperlink (task-urls-and characters-in-messages.md, Stage 1).
    connect(pimpl->text,&AbstractChatMessageBody::linkActivated,this,&AbstractChatMessageComment::linkActivated);

    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

ChatMessageComment::~ChatMessageComment()
{}

//--------------------------------------------------------------------------

void ChatMessageComment::setComment(const QString& text, TextFormat format)
{
    pimpl->commentText=text;
    pimpl->commentFormat=format;

    if (text.isEmpty())
    {
        pimpl->text->clearText();
    }
    else
    {
        pimpl->text->loadText(text,format);
    }
}

//--------------------------------------------------------------------------

void ChatMessageComment::clearComment()
{
    setComment(QString(),TextFormat::Markdown);
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

void ChatMessageComment::setOwnContextMenuEnabled(bool enable)
{
    pimpl->text->setOwnContextMenuEnabled(enable);
}

//--------------------------------------------------------------------------

void ChatMessageComment::selectText(const QString& text)
{
    pimpl->text->selectText(text);
}

//--------------------------------------------------------------------------

QString ChatMessageComment::linkAt(const QPoint& pos) const
{
    return pimpl->text->linkAt(pimpl->text->mapFrom(this,pos));
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

QRect ChatMessageComment::lastTextLineRect() const
{
    auto rect=pimpl->text->lastTextLineRect();
    if (!rect.isValid())
    {
        return {};
    }
    // pimpl->text's own lastTextLineRect() is in ITS coordinates (== this section's contents
    // rect, since the embedded ChatMessageText carries no margins of its own); translate by
    // THIS section's own left/top margins (the padding forwardpreview.qss sets on
    // uise--AbstractChatMessageComment) to land in this section's own coordinates, matching how
    // bubbleWidthHint() above adds this section's own horizontalTotalMargin() back onto the
    // embedded text's width hint.
    return rect.translated(contentsMargins().left(),contentsMargins().top());
}

//--------------------------------------------------------------------------

int ChatMessageComment::ownWidthCeiling() const
{
    auto textCeiling=pimpl->text->ownWidthCeiling();
    if (textCeiling>=std::numeric_limits<int>::max()-horizontalTotalMargin(this))
    {
        // No cap on the embedded text -- avoid overflowing by adding the margin below.
        return std::numeric_limits<int>::max();
    }
    // Matches bubbleWidthHint()'s own treatment: the embedded text's own cap plus THIS section's
    // own margin (forwardpreview.qss's padding), since lastTextLineRect() above adds that same
    // margin back onto the embedded text's coordinates.
    return textCeiling+horizontalTotalMargin(this);
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
