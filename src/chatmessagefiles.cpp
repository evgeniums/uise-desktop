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

/** @file uise/desktop/src/chatmessagefiles.cpp
*
*  Defines ChatMessageFiles.
*
*/

/****************************************************************************/

#include <algorithm>

#include <QBoxLayout>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/chatmessagetext.hpp>
#include <uise/desktop/chatmessagefileitem.hpp>
#include <uise/desktop/chatmessagefiles.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

class ChatMessageFiles_p
{
    public:

        QBoxLayout* layout=nullptr;

        QFrame* contentsFrame=nullptr;
        QBoxLayout* contentsLayout=nullptr;
        std::vector<ChatMessageFileItem*> rows;

        ChatFileItems items;

        ChatMessageText* comment=nullptr;
        QString commentText;
        bool commentMarkdown=true;

        // matches ChatMessageFileItem's own default -- see its docs for the rationale
        Qt::Alignment textVerticalAlignment=Qt::AlignVCenter;
};

//--------------------------------------------------------------------------

ChatMessageFiles::ChatMessageFiles(QWidget* parent)
    : AbstractChatMessageFiles(parent),
      pimpl(std::make_unique<ChatMessageFiles_p>())
{
    pimpl->layout=Layout::vertical(this);

    pimpl->contentsFrame=new QFrame(this);
    pimpl->contentsFrame->setObjectName("contents");
    pimpl->contentsLayout=Layout::vertical(pimpl->contentsFrame);
    pimpl->layout->addWidget(pimpl->contentsFrame);

    // constructed here so it exists regardless of whether updateChatMessage() has run yet (e.g.
    // items()/comment() queried before this body is attached to an outer message); wired up for
    // real -- setChatMessage()/setChatContent() forwarded, reparented into our own layout -- from
    // updateChatMessage(), mirroring how ChatMessageContent::updateWidgets() wires a top-level
    // section
    pimpl->comment=new ChatMessageText(this);
    pimpl->comment->setVisible(false);
    pimpl->layout->addWidget(pimpl->comment);

    // Relayed here so a host (e.g. ReplyDialog's Save/"Quote selected" button swap) can react to
    // selection changes on this body's comment via AbstractChatMessageBody alone, without
    // depending on the fact that the text lives in an embedded ChatMessageText -- same idiom
    // ChatMessageText itself uses for its own underlying QTextEdit signal.
    connect(pimpl->comment,&AbstractChatMessageBody::selectionChanged,this,&AbstractChatMessageBody::selectionChanged);

    setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

ChatMessageFiles::~ChatMessageFiles()
{}

//--------------------------------------------------------------------------

void ChatMessageFiles::setItems(ChatFileItems items)
{
    pimpl->items=std::move(items);
    rebuildList();
}

//--------------------------------------------------------------------------

const ChatFileItems& ChatMessageFiles::items() const
{
    return pimpl->items;
}

//--------------------------------------------------------------------------

void ChatMessageFiles::updateItem(const QUuid& id, const ChatFileItem& item)
{
    for (size_t i=0;i<pimpl->items.size();++i)
    {
        if (pimpl->items[i].id()==id)
        {
            pimpl->items[i]=item;
            auto incoming=(chatMessage()!=nullptr) && chatMessage()->isIncoming();

            // pimpl->items and pimpl->rows are only kept in step by rebuildList(); a caller that
            // reaches updateItem() before rebuildList() has created a row for this index would
            // otherwise index pimpl->rows out of bounds.
            if (i>=pimpl->rows.size())
            {
                return;
            }

            pimpl->rows[i]->setItem(item,incoming);
            return;
        }
    }
}

//--------------------------------------------------------------------------

void ChatMessageFiles::rebuildList()
{
    for (auto* row : pimpl->rows)
    {
        destroyWidget(row);
    }
    pimpl->rows.clear();

    auto incoming=(chatMessage()!=nullptr) && chatMessage()->isIncoming();

    for (const auto& item : pimpl->items)
    {
        auto row=new ChatMessageFileItem(pimpl->contentsFrame);
        row->setTextVerticalAlignment(pimpl->textVerticalAlignment);
        row->setItem(item,incoming);

        auto id=item.id();
        connect(row,&ChatMessageFileItem::clicked,this,[this,id](){emit itemClicked(id);});
        connect(row,&ChatMessageFileItem::loadControlClicked,this,[this,id](){emit loadControlClicked(id);});
        connect(row,&ChatMessageFileItem::dragPrepareRequested,this,[this,id](){emit dragPrepareRequested(id);});
        connect(row,&ChatMessageFileItem::dragStartRequested,this,[this,id](){emit dragStartRequested(id);});
        connect(row,&ChatMessageFileItem::menuTriggered,this,
            [this,id](int action)
            {
                switch (static_cast<ChatFileMenuAction>(action))
                {
                    case (ChatFileMenuAction::Open):
                        emit openRequested(id);
                        break;

                    case (ChatFileMenuAction::OpenWith):
                        emit openWithRequested(id);
                        break;

                    case (ChatFileMenuAction::SaveAs):
                        emit saveAsRequested(id);
                        break;

                    case (ChatFileMenuAction::Forward):
                        emit forwardRequested(id);
                        break;

                    case (ChatFileMenuAction::ShowInFolder):
                        emit showInFolderRequested(id);
                        break;

                    case (ChatFileMenuAction::CopyFileName):
                        emit copyFileNameRequested(id);
                        break;

                    case (ChatFileMenuAction::Pause):
                        emit pauseRequested(id);
                        break;

                    case (ChatFileMenuAction::Resume):
                        emit resumeRequested(id);
                        break;

                    case (ChatFileMenuAction::Cancel):
                        emit cancelRequested(id);
                        break;

                    case (ChatFileMenuAction::Download):
                        emit downloadRequested(id);
                        break;
                }
            }
        );
        connect(row,&ChatMessageFileItem::pauseRequested,this,[this,id](){emit pauseRequested(id);});
        connect(row,&ChatMessageFileItem::cancelRequested,this,[this,id](){emit cancelRequested(id);});

        pimpl->contentsLayout->addWidget(row);
        pimpl->rows.push_back(row);

        // MUST be explicit, and MUST be here rather than left to Qt: QLayout::addChildWidget()
        // (qlayout.cpp) only auto-shows a widget added to an ALREADY-VISIBLE parent via
        //     QMetaObject::invokeMethod(w,"_q_showIfNotHidden",Qt::QueuedConnection) //show later
        // i.e. asynchronously, on a LATER event-loop turn. Until that runs the row is still
        // isHidden(), so QWidgetItem::isEmpty() is true and every enclosing layout leaves it out
        // of sizeHint() entirely -- this body then measures 0-width (observed: bodySizeHint=0x4
        // with a perfectly valid row[0]=123x64) and the bubble negotiates itself down to a
        // squashed, content-less box. That is exactly what happens when setItems() runs on a
        // message ALREADY on screen (e.g. once resolveLocalUids() resolves and refreshAllItems()
        // re-pushes the item list), which is why the bug only ever showed up on already-rendered
        // messages and "fixed itself" after any later relayout -- a scroll, or the deferred
        // updatePosition() a selection toggle happens to schedule -- once the queued show landed.
        // show() clears WA_WState_Hidden synchronously, so the very next sizeHint() counts this
        // row. Safe when the parent is not visible either: the row is merely marked
        // explicitly-shown and still only maps once its parent does.
        row->show();

        // bubbleWidthHint() below reads row->sizeHint(), which is only meaningful once
        // chatmessagefiles.qss's min-width/padding rules are actually applied -- ensure that
        // before this row is ever measured, rather than relying on whatever repolish this
        // message's ancestors happen to get later (see makeMessage() in chatmessagesview.ipp).
        row->ensurePolished();

        // Seed the width cap from whatever was last negotiated -- setItems() can rebuild the
        // list on a message already on screen (see the comment above row->show()), and without
        // this a freshly-created row would render at its full, uncapped name-label width until
        // the next resize/negotiation pass happens to touch this message again.
        if (chatContent()!=nullptr && chatContent()->maximumBubbleWidth()>0)
        {
            row->limitWidth(clampToMaxBubbleWidth(chatContent()->maximumBubbleWidth()));
        }
    }

    updateGeometry();
}

//--------------------------------------------------------------------------

void ChatMessageFiles::setComment(const QString& text, bool markdown)
{
    pimpl->commentText=text;
    pimpl->commentMarkdown=markdown;

    if (text.isEmpty())
    {
        pimpl->comment->clearText();
        pimpl->comment->setVisible(false);
    }
    else
    {
        pimpl->comment->loadText(text,markdown);
        pimpl->comment->setVisible(true);
    }
}

//--------------------------------------------------------------------------

void ChatMessageFiles::clearComment()
{
    setComment(QString(),true);
}

//--------------------------------------------------------------------------

QString ChatMessageFiles::comment() const
{
    return pimpl->commentText;
}

//--------------------------------------------------------------------------

void ChatMessageFiles::closeMenus()
{
    for (auto* row : pimpl->rows)
    {
        row->closeMenu();
    }
}

//--------------------------------------------------------------------------

void ChatMessageFiles::setTextVerticalAlignment(Qt::Alignment alignment)
{
    // normalized the same way ChatMessageFileItem::setTextVerticalAlignment() does, so
    // textVerticalAlignment() reports exactly what every row actually ends up using
    auto vAlign=alignment & Qt::AlignVertical_Mask;
    if (vAlign!=Qt::AlignTop && vAlign!=Qt::AlignVCenter)
    {
        vAlign=Qt::AlignTop;
    }

    pimpl->textVerticalAlignment=vAlign;
    for (auto* row : pimpl->rows)
    {
        row->setTextVerticalAlignment(vAlign);
    }
}

//--------------------------------------------------------------------------

Qt::Alignment ChatMessageFiles::textVerticalAlignment() const
{
    return pimpl->textVerticalAlignment;
}

//--------------------------------------------------------------------------

void ChatMessageFiles::startItemDrag(const QUuid& id, const QList<QUrl>& urls)
{
    // Same linear scan as updateItem() -- items and rows are only kept in step by
    // rebuildList(), see its own comment.
    for (size_t i=0;i<pimpl->items.size();++i)
    {
        if (pimpl->items[i].id()==id)
        {
            if (i<pimpl->rows.size())
            {
                pimpl->rows[i]->startDrag(urls);
            }
            return;
        }
    }
}

//--------------------------------------------------------------------------

void ChatMessageFiles::clearContentSelection()
{
    pimpl->comment->clearContentSelection();
}

//--------------------------------------------------------------------------

QString ChatMessageFiles::selectedText() const
{
    return pimpl->comment->selectedText();
}

//--------------------------------------------------------------------------

bool ChatMessageFiles::hasSelectableText() const
{
    return pimpl->comment->hasSelectableText();
}

//--------------------------------------------------------------------------

void ChatMessageFiles::setCopyable(bool enable)
{
    pimpl->comment->setCopyable(enable);
}

//--------------------------------------------------------------------------

void ChatMessageFiles::selectText(const QString& text)
{
    pimpl->comment->selectText(text);
}

//--------------------------------------------------------------------------

int ChatMessageFiles::bubbleWidthHint(int forMaxWidth)
{
    auto capped=clampToMaxBubbleWidth(forMaxWidth);

    int width=0;
    for (auto* row : pimpl->rows)
    {
        row->limitWidth(capped);
        width=std::max(width,row->sizeHint().width());
    }

    if (!pimpl->commentText.isEmpty())
    {
        pimpl->comment->setChatContent(chatContent());
        width=std::max(width,pimpl->comment->bubbleWidthHint(forMaxWidth));
    }

    return std::min(width,forMaxWidth);
}

//--------------------------------------------------------------------------

void ChatMessageFiles::updateMaximumBubbleWidth()
{
    auto width=clampToMaxBubbleWidth(chatContent()->maximumBubbleWidth());
    for (auto* row : pimpl->rows)
    {
        row->limitWidth(width);
    }

    if (!pimpl->commentText.isEmpty())
    {
        pimpl->comment->setChatContent(chatContent());
        pimpl->comment->updateMaximumBubbleWidth();
    }
    updateGeometry();
}

//--------------------------------------------------------------------------

void ChatMessageFiles::updateChatMessage()
{
    // setChatMessage() no longer reparents the comment away (see AbstractChatMessageChild::
    // setChatMessage()), so this addWidget() is now just asserting the layout slot -- and it
    // short-circuits the reparent inside QLayout::addChildWidget(), since the comment is
    // already a child of this widget.
    pimpl->comment->setChatMessage(chatMessage());
    pimpl->layout->addWidget(pimpl->comment);

    auto incoming=(chatMessage()!=nullptr) && chatMessage()->isIncoming();
    for (size_t i=0;i<pimpl->rows.size() && i<pimpl->items.size();++i)
    {
        pimpl->rows[i]->setItem(pimpl->items[i],incoming);
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
