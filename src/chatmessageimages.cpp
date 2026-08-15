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

#include <QResizeEvent>

#include <uise/desktop/frame.hpp>
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

// Edge length a placeholder tile (an item whose real pixel size is not known -- never resolved,
// or a failed/synthesized entry that has no image behind it at all) is laid out at.
//
// Without this, such an item reaches albumLayout() as QSize(1,1), i.e. aspect 1.0, and the
// single-image template's "fill the width budget" rule turns it into a maxWidth x maxWidth
// square -- a bubble-width blank tile, which is visibly wrong for something that has no image
// to show. The clamps below feed albumLayout() a width/height budget sized for placeholders
// instead of for real photo content, so every template (not just the single-image one) lays
// them out at roughly this extent and the bubble ends up sized to match.
constexpr int PlaceholderTileExtent=100;

}

//--------------------------------------------------------------------------

class ChatMessageImages_p
{
    public:

        //! Total footprint of the album grid, as computed by albumLayout() -- previously the
        //! fixed size of a now-removed #grid QFrame (see the class doc comment).
        QSize gridSize;

        std::vector<ChatMessageImageItem*> tiles;

        ChatFileItems items;

        ChatMessageText* comment=nullptr;
        QString commentText;
        bool commentMarkdown=true;

        ImageLabel::AnimationMode animationMode=ImageLabel::DefaultAnimationMode;

        // Signature of the last full rebuildGrid() layout pass -- lets a later call skip
        // albumLayout()/setFixedSize() when nothing that affects tile geometry actually changed
        // (see rebuildGrid()). Deliberately does NOT gate the per-tile setItem() refresh at the
        // end of rebuildGrid(), so a setItems() call carrying only non-geometry changes (e.g. a
        // transfer-progress tick, see ChatMessage::refreshAllItems() in whitemdesktop) still
        // reaches the tiles even when the layout itself is reused.
        int lastLayoutForMaxWidth=-1;
        qreal lastLayoutDpr=-1.0;
        std::vector<QSize> lastLayoutPixelSizes;
        std::vector<QRect> lastLayoutRects;
};

//--------------------------------------------------------------------------

ChatMessageImages::ChatMessageImages(QWidget* parent)
    : AbstractChatMessageImages(parent),
      pimpl(std::make_unique<ChatMessageImages_p>())
{
    // No QLayout, no child widgets created here -- tiles are positioned with manual QRects from
    // albumLayout() (see rebuildGrid()/layoutChildren()), the same "manual setGeometry()" idiom
    // FileUploadListItem uses for its imageFrame/rowFrame children. The comment is created
    // lazily on first use (see ensureComment()) since most albums carry no comment at all.
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
            // pixelSize changing (which template/aspect class the album picked) means the album
            // laid out from a stale value (e.g. the QSize(1,1) placeholder) and must run again,
            // not just repaint the one tile in its unchanged rect. Checked before the copy below
            // so the comparison is against the OLD stored item, not the new one.
            bool geometryChanged=pimpl->items[i].pixelSize()!=item.pixelSize();

            pimpl->items[i]=item;

            if (geometryChanged)
            {
                auto forMaxWidth=(chatContent()!=nullptr && chatContent()->maximumBubbleWidth()>0)
                    ? chatContent()->maximumBubbleWidth()
                    : DefaultMaxWidth;
                rebuildGrid(forMaxWidth);
                if (chatContent()!=nullptr)
                {
                    // rebuildGrid() alone only resizes THIS section's own grid -- the bubble as
                    // a whole (and any sibling section, e.g. a comment below the album) must
                    // renegotiate too, exactly as if a resize had triggered it.
                    chatContent()->renegotiateBubbleWidth();
                }
                return;
            }

            auto incoming=(chatMessage()!=nullptr) && chatMessage()->isIncoming();
            pimpl->tiles[i]->setItem(item,incoming);
            return;
        }
    }
}

//--------------------------------------------------------------------------

void ChatMessageImages::rebuildGrid(int forMaxWidth)
{
    const qreal dpr=(devicePixelRatioF()>0.0) ? devicePixelRatioF() : 1.0;

    // rebuildGrid() runs on every bubble-width negotiation (i.e. every resize) AND on every
    // setItems() call carrying non-geometry updates (e.g. ChatMessage::refreshAllItems() in
    // whitemdesktop, ticking transfer progress) -- skip the expensive albumLayout() pass and
    // reuse the last computed rects when neither the width budget, the display's pixel ratio,
    // nor any item's pixelSize() changed since then. The per-tile setItem() refresh below still
    // always runs, so content updates are never skipped, only the layout recomputation.
    bool layoutUnchanged=forMaxWidth==pimpl->lastLayoutForMaxWidth
        && dpr==pimpl->lastLayoutDpr
        && pimpl->lastLayoutPixelSizes.size()==pimpl->items.size()
        && pimpl->lastLayoutRects.size()==pimpl->items.size();
    for (size_t i=0;layoutUnchanged && i<pimpl->items.size();++i)
    {
        layoutUnchanged=pimpl->lastLayoutPixelSizes[i]==pimpl->items[i].pixelSize();
    }

    std::vector<QRect> rects;
    if (layoutUnchanged)
    {
        rects=pimpl->lastLayoutRects;
    }
    else
    {
        std::vector<QSize> sizes;
        sizes.reserve(pimpl->items.size());
        std::vector<QSize> rawPixelSizes;
        rawPixelSizes.reserve(pimpl->items.size());
        bool allPlaceholders=!pimpl->items.empty();
        for (const auto& item : pimpl->items)
        {
            auto sz=item.pixelSize();
            bool known=sz.isValid() && sz.width()>0 && sz.height()>0;
            allPlaceholders=allPlaceholders && !known;
            sizes.push_back(known ? sz : QSize(1,1));
            // Stored as the layoutUnchanged signature below -- must be the RAW pixelSize(), not
            // the placeholder-substituted value above, since that is what the next call's
            // comparison (against pimpl->items[i].pixelSize()) actually checks against.
            rawPixelSizes.push_back(sz);
        }

        AlbumLayoutOptions options;
        options.maxWidth=(forMaxWidth>0) ? forMaxWidth : DefaultMaxWidth;

        if (allPlaceholders)
        {
            // See PlaceholderTileExtent. Two tiles wide is the budget every template then works
            // within: the single-image one clamps to a square of exactly the extent (its own
            // maxHeight branch), the two-image one splits the width into two such squares, and
            // three-or-more fall out of their own templates (or the justified-rows fallback) at
            // comparable sizes, with albumLayout()'s uniform scale-down catching anything taller
            // than the height budget.
            options.maxWidth=qMin(options.maxWidth,PlaceholderTileExtent*2+options.spacing);
            options.maxHeight=(pimpl->items.size()==1)
                ? PlaceholderTileExtent
                : qMin(options.maxHeight,PlaceholderTileExtent*2+options.spacing);
        }
        else if (pimpl->items.size()==1)
        {
            // Never blow a SMALL image up to the full bubble budget -- albumLayout()'s n==1
            // branch otherwise always spends the whole width budget, which turned a
            // thumbnail-sized original into a tile several times its own resolution (observed at
            // 2x-8x). Confirmed requirement: "show the full original image, only scaled DOWN to
            // tile size; if the original is smaller, no scaling needed."
            //
            // Divided by devicePixelRatio because the budget is in LOGICAL units while
            // pixelSize() is in real pixels: a 200px-wide original fills exactly 100 logical px
            // on a 2x display, and asking for more than that is upscaling however good the
            // source rung is.
            //
            // This caps against the ORIGINAL's own dimensions (pixelSize(), the sender's real
            // image), NOT against whichever preview rung happens to be resolved right now -- tile
            // geometry must never depend on the latter (confirmed requirement), and does not
            // here.
            //
            // Deliberately n==1 only: for an album, one small image among large ones would drag
            // the whole grid down with it (albumLayout()'s scale-downs are uniform, to preserve
            // the ratios between tiles), which is worse than letting that one tile upscale.
            const auto& sz=sizes.front();
            options.maxWidth=qMin(options.maxWidth,qMax(options.minTile,qRound(sz.width()/dpr)));
            options.maxHeight=qMin(options.maxHeight,qMax(options.minTile,qRound(sz.height()/dpr)));
        }

        QSize totalSize;
        rects=albumLayout(sizes,options,&totalSize);
        pimpl->gridSize=totalSize;

        pimpl->lastLayoutForMaxWidth=forMaxWidth;
        pimpl->lastLayoutDpr=dpr;
        pimpl->lastLayoutPixelSizes=std::move(rawPixelSizes);
        pimpl->lastLayoutRects=rects;
    }

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
            auto tile=new ChatMessageImageItem(this);
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

                        case (ChatFileMenuAction::Cancel):
                            emit cancelRequested(id);
                            break;
                    }
                }
            );
            connect(tile,&ChatMessageImageItem::pauseRequested,this,[this,tile](){emit pauseRequested(tile->item().id());});
            connect(tile,&ChatMessageImageItem::cancelRequested,this,[this,tile](){emit cancelRequested(tile->item().id());});

            pimpl->tiles.push_back(tile);

            // A freshly created tile must be polished before its first paint -- e.g. its
            // [placeholder="true"] outline (chatmessagefiles.qss) is QSS-driven and, like every
            // dynamic-property rule, only takes effect after a repolish. Matches the file-row
            // treatment in ChatMessageFiles::rebuildList().
            tile->ensurePolished();

            // No QLayout to add it to any more -- a plain new child stays hidden until shown
            // when its parent is already visible (a QLayout::addWidget() used to do this
            // implicitly).
            tile->show();
        }
    }

    for (size_t i=0;i<pimpl->items.size();++i)
    {
        pimpl->tiles[i]->setItem(pimpl->items[i],incoming);
    }

    updateGeometry();
    layoutChildren();
}

//--------------------------------------------------------------------------

void ChatMessageImages::layoutChildren()
{
    // Single manual placement path for every child -- tiles and the (optional, lazily created)
    // comment -- replacing the QLayout this class used to have. Called from resizeEvent() and
    // from the QEvent::LayoutRequest handler in event(), which is how a child's
    // updateGeometry() (e.g. the comment re-wrapping) reaches a layout-less parent -- a
    // QLayout would normally intercept that event itself and re-run activate().
    auto cr=contentsRect();

    if (pimpl->lastLayoutRects.size()==pimpl->tiles.size())
    {
        for (size_t i=0;i<pimpl->tiles.size();++i)
        {
            pimpl->tiles[i]->setGeometry(pimpl->lastLayoutRects[i].translated(cr.topLeft()));
        }
    }

    if (pimpl->comment!=nullptr && pimpl->comment->isVisible())
    {
        auto h=pimpl->comment->sizeHint().height();
        pimpl->comment->setGeometry(cr.x(),cr.y()+pimpl->gridSize.height(),cr.width(),h);
    }
}

//--------------------------------------------------------------------------

void ChatMessageImages::resizeEvent(QResizeEvent* event)
{
    AbstractChatMessageImages::resizeEvent(event);
    layoutChildren();
}

//--------------------------------------------------------------------------

bool ChatMessageImages::event(QEvent* event)
{
    if (event->type()==QEvent::LayoutRequest)
    {
        updateGeometry();
        layoutChildren();
        return true;
    }
    return AbstractChatMessageImages::event(event);
}

//--------------------------------------------------------------------------

ChatMessageText* ChatMessageImages::ensureComment()
{
    if (pimpl->comment==nullptr)
    {
        pimpl->comment=new ChatMessageText(this);
        pimpl->comment->setVisible(false);
        if (chatMessage()!=nullptr)
        {
            pimpl->comment->setChatMessage(chatMessage());
        }
        if (chatContent()!=nullptr)
        {
            pimpl->comment->setChatContent(chatContent());
        }
        // See rebuildGrid()'s identical comment: freshly created QSS-dependent content must be
        // polished before its first paint.
        pimpl->comment->ensurePolished();
    }
    return pimpl->comment;
}

//--------------------------------------------------------------------------

void ChatMessageImages::setComment(const QString& text, bool markdown)
{
    pimpl->commentText=text;
    pimpl->commentMarkdown=markdown;

    if (text.isEmpty())
    {
        // Never create the comment widget just to immediately clear/hide it -- most albums
        // never carry a comment at all.
        if (pimpl->comment!=nullptr)
        {
            pimpl->comment->clearText();
            pimpl->comment->setVisible(false);
        }
    }
    else
    {
        auto comment=ensureComment();
        comment->loadText(text,markdown);
        comment->setVisible(true);
    }

    updateGeometry();
    layoutChildren();
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
    if (pimpl->comment!=nullptr)
    {
        pimpl->comment->clearContentSelection();
    }
}

//--------------------------------------------------------------------------

QString ChatMessageImages::selectedText() const
{
    return (pimpl->comment!=nullptr) ? pimpl->comment->selectedText() : QString();
}

//--------------------------------------------------------------------------

int ChatMessageImages::bubbleWidthHint(int forMaxWidth)
{
    rebuildGrid(forMaxWidth);

    int width=pimpl->gridSize.width()+horizontalTotalMargin(this);
    if (!pimpl->commentText.isEmpty())
    {
        auto comment=ensureComment();
        comment->setChatContent(chatContent());
        width=std::max(width,comment->bubbleWidthHint(forMaxWidth));
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
        auto comment=ensureComment();
        comment->setChatContent(chatContent());
        comment->updateMaximumBubbleWidth();
    }

    updateGeometry();
    layoutChildren();
}

//--------------------------------------------------------------------------

void ChatMessageImages::updateChatMessage()
{
    if (pimpl->comment!=nullptr)
    {
        // AbstractChatMessageChild::setChatMessage() reparents the comment onto chatMessage()
        // as a side effect, hiding it -- put it back under this widget and restore its
        // visibility right after, exactly like ChatMessageContent::updateWidgets() does for a
        // top-level header/body/bottom section.
        pimpl->comment->setChatMessage(chatMessage());
        pimpl->comment->setParent(this);
        pimpl->comment->setVisible(!pimpl->commentText.isEmpty());
    }

    auto incoming=(chatMessage()!=nullptr) && chatMessage()->isIncoming();
    for (size_t i=0;i<pimpl->tiles.size() && i<pimpl->items.size();++i)
    {
        pimpl->tiles[i]->setItem(pimpl->items[i],incoming);
    }

    layoutChildren();
}

//--------------------------------------------------------------------------

QSize ChatMessageImages::sizeHint() const
{
    auto m=contentsMargins();

    int width=pimpl->gridSize.width();
    int height=pimpl->gridSize.height();
    if (pimpl->comment!=nullptr && pimpl->comment->isVisible())
    {
        auto commentSize=pimpl->comment->sizeHint();
        width=std::max(width,commentSize.width());
        height+=commentSize.height();
    }

    return QSize(width+m.left()+m.right(),height+m.top()+m.bottom());
}

//--------------------------------------------------------------------------

QSize ChatMessageImages::minimumSizeHint() const
{
    // The grid is already a hard size (from albumLayout(), no wrapping/eliding involved) and
    // the comment's own vertical policy is Fixed -- there is no smaller size this body could
    // usefully take, so the minimum is the same as the preferred size.
    return sizeHint();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
