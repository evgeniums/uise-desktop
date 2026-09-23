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
#include <limits>

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

        AbstractChatMessageFiles::ChatFileItemBuilder itemBuilder;

        ChatMessageText* comment=nullptr;
        QString commentText;
        TextFormat commentFormat=TextFormat::Markdown;

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

    // Same idiom, for a clicked hyperlink in the caption (task-urls-and characters-in-
    // messages.md, Stage 1).
    connect(pimpl->comment,&AbstractChatMessageBody::linkActivated,this,&AbstractChatMessageBody::linkActivated);

    setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

ChatMessageFiles::~ChatMessageFiles()
{}

//--------------------------------------------------------------------------

void ChatMessageFiles::setItemBuilder(ChatFileItemBuilder builder)
{
    pimpl->itemBuilder=std::move(builder);
    // Rebuild unconditionally so the host may install this before OR after setItems() -- with no
    // items yet this is an empty loop, and with items already in place the existing rows are
    // replaced by whatever the new builder produces.
    rebuildList();
}

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

void ChatMessageFiles::setPlaybackProgress(const QUuid& id, qreal fraction)
{
    // Same linear scan, and the same items/rows-only-in-step-after-rebuildList() guard, as
    // updateItem() above.
    for (size_t i=0;i<pimpl->items.size();++i)
    {
        if (pimpl->items[i].id()==id)
        {
            if (i<pimpl->rows.size())
            {
                pimpl->rows[i]->setPlaybackProgress(fraction);
            }
            return;
        }
    }
}

//--------------------------------------------------------------------------

void ChatMessageFiles::setPlaybackPosition(const QUuid& id, qint64 positionMs, qint64 durationMs)
{
    // Same linear scan as setPlaybackProgress() above -- its companion, pushed on the same ticks.
    for (size_t i=0;i<pimpl->items.size();++i)
    {
        if (pimpl->items[i].id()==id)
        {
            if (i<pimpl->rows.size())
            {
                pimpl->rows[i]->setPlaybackPosition(positionMs,durationMs);
            }
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
        // A host-installed builder may substitute a specialized row for certain kinds of
        // attachment (see setItemBuilder()); anything it declines (nullptr) or any row it was
        // never asked about falls back to the generic item. Whatever comes back is a
        // ChatMessageFileItem, so every connect() below applies unchanged either way.
        ChatMessageFileItem* row=nullptr;
        if (pimpl->itemBuilder)
        {
            row=pimpl->itemBuilder(item,pimpl->contentsFrame);
        }
        if (row==nullptr)
        {
            row=new ChatMessageFileItem(pimpl->contentsFrame);
        }
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

                    case (ChatFileMenuAction::CopyImage):
                        // Unreachable for a file row -- buildChatFileMenuItems()'s own
                        // imageItem gate never offers this action here. Present only so this
                        // switch stays exhaustive, same as ChatMessageImages' own relay below.
                        break;

                    case (ChatFileMenuAction::Play):
                        emit playRequested(id);
                        break;

                    case (ChatFileMenuAction::Stop):
                        emit stopRequested(id);
                        break;
                }
            }
        );
        connect(row,&ChatMessageFileItem::pauseRequested,this,[this,id](){emit pauseRequested(id);});
        connect(row,&ChatMessageFileItem::cancelRequested,this,[this,id](){emit cancelRequested(id);});
        // an audio row's own controls, fanned into the same signals as the menu's Play/Stop
        connect(row,&ChatMessageFileItem::playRequested,this,[this,id](){emit playRequested(id);});
        connect(row,&ChatMessageFileItem::stopRequested,this,[this,id](){emit stopRequested(id);});
        connect(row,&ChatMessageFileItem::seekRequested,this,[this,id](qreal fraction){emit seekRequested(id,fraction);});

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

void ChatMessageFiles::setComment(const QString& text, TextFormat format)
{
    pimpl->commentText=text;
    pimpl->commentFormat=format;

    if (text.isEmpty())
    {
        pimpl->comment->clearText();
        pimpl->comment->setVisible(false);
    }
    else
    {
        pimpl->comment->loadText(text,format);
        pimpl->comment->setVisible(true);
    }
}

//--------------------------------------------------------------------------

void ChatMessageFiles::clearComment()
{
    setComment(QString(),TextFormat::Markdown);
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

QUuid ChatMessageFiles::fileItemAt(const QPoint& pos) const
{
    for (auto* row : pimpl->rows)
    {
        if (!row->isHidden() && row->rect().contains(row->mapFrom(this,pos)))
        {
            return row->item().id();
        }
    }
    return QUuid{};
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

void ChatMessageFiles::startItemDrag(const QUuid& id, const QList<QUrl>& urls, const QString& sourceTag)
{
    // Same linear scan as updateItem() -- items and rows are only kept in step by
    // rebuildList(), see its own comment.
    for (size_t i=0;i<pimpl->items.size();++i)
    {
        if (pimpl->items[i].id()==id)
        {
            if (i<pimpl->rows.size())
            {
                pimpl->rows[i]->startDrag(urls,sourceTag);
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

void ChatMessageFiles::setOwnContextMenuEnabled(bool enable)
{
    pimpl->comment->setOwnContextMenuEnabled(enable);
}

//--------------------------------------------------------------------------

void ChatMessageFiles::selectText(const QString& text)
{
    pimpl->comment->selectText(text);
}

//--------------------------------------------------------------------------

QString ChatMessageFiles::linkAt(const QPoint& pos) const
{
    return pimpl->comment->linkAt(pimpl->comment->mapFrom(this,pos));
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

QRect ChatMessageFiles::lastTextLineRect() const
{
    if (!pimpl->comment->isHidden())
    {
        auto rect=pimpl->comment->lastTextLineRect();
        if (!rect.isValid())
        {
            return {};
        }

        // pimpl->comment sits in THIS body's own vertical layout, directly below
        // pimpl->contentsFrame (the file rows) -- translate by that frame's sizeHint() height,
        // not its possibly-stale geometry() (this can run mid-negotiation, before the layout has
        // necessarily run for real), plus this widget's own left/top contents margins -- matching
        // how bubbleWidthHint()/updateMaximumBubbleWidth() already treat pimpl->comment as this
        // widget's own trailing row.
        auto cm=contentsMargins();
        return rect.translated(cm.left(),cm.top()+pimpl->contentsFrame->sizeHint().height());
    }

    // No caption. Still not the inline path -- allowsInlineBottom() below refuses it -- but the
    // LAST row may have a trailing line of its own worth measuring (the voice row's duration/size
    // info line, well above its own row's bottom edge): letting the bottom row be pulled up out of
    // that row's own bottom padding, instead of sitting a full extra line below the whole body,
    // same rationale as a text bubble absorbing its own document margin. Every other row kind
    // (plain file/image/invitation-file/audio-file) returns an invalid rect from lastLineRect(),
    // which keeps today's full-width row unchanged for them.
    if (pimpl->rows.empty())
    {
        return {};
    }

    auto* lastRow=pimpl->rows.back();
    auto rowRect=lastRow->lastLineRect();
    if (!rowRect.isValid())
    {
        return {};
    }

    // Rows stack in pimpl->contentsLayout (zero margin/spacing, Layout::vertical's own reset), so
    // the last row's own top, within contentsFrame, is simply what is left after every row's
    // sizeHint() height once the frame's own bottom inset is excluded -- hint-derived, not
    // geometry(), for the same reason as the caption branch above.
    auto cm=contentsMargins();
    auto frameMargins=pimpl->contentsFrame->contentsMargins();
    auto rowTop=cm.top()+pimpl->contentsFrame->sizeHint().height()-frameMargins.bottom()-lastRow->sizeHint().height();
    return rowRect.translated(cm.left()+frameMargins.left(),rowTop);
}

//--------------------------------------------------------------------------

bool ChatMessageFiles::allowsInlineBottom() const
{
    // A caption's last line may share its own trailing space with the bottom row (inline), same
    // as any text body. A caption-less last row's line (see lastTextLineRect() above) is reported
    // ONLY as a measurement anchor -- the row keeps a line of its own, never overlaid on top of a
    // voice row's duration/size line.
    return !pimpl->comment->isHidden();
}

//--------------------------------------------------------------------------

int ChatMessageFiles::ownWidthCeiling() const
{
    // The observed body width (bubbleWidthHint()'s own `width`) can reach whichever cap governs
    // the LARGER of its two contributors -- the file rows (this section's own maxBubbleWidth,
    // applied via row->limitWidth()) or the caption (its own, possibly different, maxBubbleWidth)
    // -- so the ceiling here is the max of the two, not just this section's own cap alone.
    auto ownCeiling=clampToMaxBubbleWidth(std::numeric_limits<int>::max());
    if (!pimpl->comment->isHidden())
    {
        auto commentCeiling=pimpl->comment->ownWidthCeiling();
        if (ownCeiling>=std::numeric_limits<int>::max() || commentCeiling>=std::numeric_limits<int>::max())
        {
            return std::numeric_limits<int>::max();
        }
        ownCeiling=std::max(ownCeiling,commentCeiling);
    }
    return ownCeiling;
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
