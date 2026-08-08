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

/** @file uise/desktop/src/chatmessageimages.cpp
*
*  Defines ChatMessageImages.
*
*/

/****************************************************************************/

#include <algorithm>

#include <QBoxLayout>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/albumlayout.hpp>
#include <uise/desktop/chatmessagetext.hpp>
#include <uise/desktop/chatmessageimageitem.hpp>
#include <uise/desktop/chatmessageimages.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

// Bubble width to lay the album out against before any real bubble-width negotiation has run
// yet (e.g. items() set right after construction) -- overwritten by the real forMaxWidth as
// soon as bubbleWidthHint()/updateMaximumBubbleWidth() are called by the owning
// AbstractChatMessageContent.
constexpr int DefaultMaxWidth=320;

}

//--------------------------------------------------------------------------

class ChatMessageImages_p
{
    public:

        QBoxLayout* layout=nullptr;

        QFrame* gridFrame=nullptr;
        std::vector<ChatMessageImageItem*> tiles;

        ChatFileItems items;

        ChatMessageText* comment=nullptr;
        QString commentText;
        bool commentMarkdown=true;

        ImageLabel::AnimationMode animationMode=ImageLabel::DefaultAnimationMode;
};

//--------------------------------------------------------------------------

ChatMessageImages::ChatMessageImages(QWidget* parent)
    : AbstractChatMessageImages(parent),
      pimpl(std::make_unique<ChatMessageImages_p>())
{
    pimpl->layout=Layout::vertical(this);

    // no internal layout of its own -- tiles are positioned with manual QRects from
    // albumLayout(), the same "plain QFrame + manual setGeometry()" idiom FileUploadListItem
    // uses for its imageFrame/rowFrame children
    pimpl->gridFrame=new QFrame(this);
    pimpl->gridFrame->setObjectName("grid");
    pimpl->layout->addWidget(pimpl->gridFrame);

    // see ChatMessageFiles's constructor comment: wired up for real -- setChatMessage()/
    // setChatContent() forwarded, reparented into our own layout -- from updateChatMessage()
    pimpl->comment=new ChatMessageText(this);
    pimpl->comment->setVisible(false);
    pimpl->layout->addWidget(pimpl->comment);

    setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

ChatMessageImages::~ChatMessageImages()
{}

//--------------------------------------------------------------------------

void ChatMessageImages::setItems(ChatFileItems items)
{
    pimpl->items=std::move(items);

    auto forMaxWidth=(chatContent()!=nullptr && chatContent()->maximumBubbleWidth()>0)
        ? chatContent()->maximumBubbleWidth()
        : DefaultMaxWidth;
    rebuildGrid(forMaxWidth);
}

//--------------------------------------------------------------------------

const ChatFileItems& ChatMessageImages::items() const
{
    return pimpl->items;
}

//--------------------------------------------------------------------------

void ChatMessageImages::updateItem(const QUuid& id, const ChatFileItem& item)
{
    for (size_t i=0;i<pimpl->items.size();++i)
    {
        if (pimpl->items[i].id()==id)
        {
            pimpl->items[i]=item;
            auto incoming=(chatMessage()!=nullptr) && chatMessage()->isIncoming();
            pimpl->tiles[i]->setItem(item,incoming);
            return;
        }
    }
}

//--------------------------------------------------------------------------

void ChatMessageImages::rebuildGrid(int forMaxWidth)
{
    std::vector<QSize> sizes;
    sizes.reserve(pimpl->items.size());
    for (const auto& item : pimpl->items)
    {
        auto sz=item.pixelSize();
        sizes.push_back((sz.isValid() && sz.width()>0 && sz.height()>0) ? sz : QSize(1,1));
    }

    AlbumLayoutOptions options;
    options.maxWidth=(forMaxWidth>0) ? forMaxWidth : DefaultMaxWidth;

    QSize totalSize;
    auto rects=albumLayout(sizes,options,&totalSize);
    pimpl->gridFrame->setFixedSize(totalSize);

    auto incoming=(chatMessage()!=nullptr) && chatMessage()->isIncoming();

    // rebuildGrid() runs on every bubble-width negotiation, i.e. on every view resize -- reuse the
    // existing tiles whenever the item count did not change instead of destroying and recreating
    // them, so a resize does not tear down (and thereby restart) any animated ImageLabel content.
    // Tiles are addressed by index elsewhere in the class (see updateChatMessage()), so this keeps
    // the same addressing scheme rather than introducing a new one.
    if (pimpl->tiles.size()!=pimpl->items.size())
    {
        for (auto* tile : pimpl->tiles)
        {
            destroyWidget(tile);
        }
        pimpl->tiles.clear();

        for (size_t i=0;i<pimpl->items.size();++i)
        {
            auto tile=new ChatMessageImageItem(pimpl->gridFrame);
            tile->setAnimationMode(pimpl->animationMode);

            // the item id is read from the tile at emit time (not captured by value) so that a
            // later reuse of this same tile for a different item still reports the right id
            connect(tile,&ChatMessageImageItem::clicked,this,[this,tile](){emit itemClicked(tile->item().id());});
            connect(tile,&ChatMessageImageItem::loadControlClicked,this,[this,tile](){emit loadControlClicked(tile->item().id());});
            connect(tile,&ChatMessageImageItem::menuTriggered,this,
                [this,tile](int action)
                {
                    auto id=tile->item().id();
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
                    }
                }
            );

            pimpl->tiles.push_back(tile);
        }
    }

    for (size_t i=0;i<pimpl->items.size();++i)
    {
        pimpl->tiles[i]->setGeometry(rects[i]);
        pimpl->tiles[i]->setItem(pimpl->items[i],incoming);
    }

    updateGeometry();
}

//--------------------------------------------------------------------------

void ChatMessageImages::setComment(const QString& text, bool markdown)
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

void ChatMessageImages::clearComment()
{
    setComment(QString(),true);
}

//--------------------------------------------------------------------------

QString ChatMessageImages::comment() const
{
    return pimpl->commentText;
}

//--------------------------------------------------------------------------

void ChatMessageImages::closeMenus()
{
    for (auto* tile : pimpl->tiles)
    {
        tile->closeMenu();
    }
}

//--------------------------------------------------------------------------

void ChatMessageImages::setAnimationMode(ImageLabel::AnimationMode mode)
{
    pimpl->animationMode=mode;
    for (auto* tile : pimpl->tiles)
    {
        tile->setAnimationMode(mode);
    }
}

//--------------------------------------------------------------------------

ImageLabel::AnimationMode ChatMessageImages::animationMode() const
{
    return pimpl->animationMode;
}

//--------------------------------------------------------------------------

void ChatMessageImages::clearContentSelection()
{
    pimpl->comment->clearContentSelection();
}

//--------------------------------------------------------------------------

QString ChatMessageImages::selectedText() const
{
    return pimpl->comment->selectedText();
}

//--------------------------------------------------------------------------

int ChatMessageImages::bubbleWidthHint(int forMaxWidth)
{
    rebuildGrid(forMaxWidth);

    int width=pimpl->gridFrame->width();
    if (!pimpl->commentText.isEmpty())
    {
        pimpl->comment->setChatContent(chatContent());
        width=std::max(width,pimpl->comment->bubbleWidthHint(forMaxWidth));
    }

    return std::min(width,forMaxWidth);
}

//--------------------------------------------------------------------------

void ChatMessageImages::updateMaximumBubbleWidth()
{
    auto forMaxWidth=(chatContent()!=nullptr) ? chatContent()->maximumBubbleWidth() : DefaultMaxWidth;
    rebuildGrid(forMaxWidth);

    if (!pimpl->commentText.isEmpty())
    {
        pimpl->comment->setChatContent(chatContent());
        pimpl->comment->updateMaximumBubbleWidth();
    }

    updateGeometry();
}

//--------------------------------------------------------------------------

void ChatMessageImages::updateChatMessage()
{
    // reparents comment to chatMessage() as a side effect (see AbstractChatMessageChild::
    // setChatMessage()) -- put it back into our own layout right after, exactly like
    // ChatMessageContent::updateWidgets() does for a top-level header/body/bottom section
    pimpl->comment->setChatMessage(chatMessage());
    pimpl->layout->addWidget(pimpl->comment);

    auto incoming=(chatMessage()!=nullptr) && chatMessage()->isIncoming();
    for (size_t i=0;i<pimpl->tiles.size() && i<pimpl->items.size();++i)
    {
        pimpl->tiles[i]->setItem(pimpl->items[i],incoming);
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
