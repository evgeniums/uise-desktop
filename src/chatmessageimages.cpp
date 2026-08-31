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

// Defaults for the QSS-settable minTileSize/tileMaxUpscale properties (todo-album-layout-small-
// tile-packing.md) -- see their own doc comments (chatmessageimages.hpp) and
// resources/style/chatmessagefiles.qss's qproperty- block for the shipped values.
//
// DefaultMinTileSize doubles as the extent a PLACEHOLDER tile (an item whose real pixel size is
// not known -- never resolved, or a failed/synthesized entry with no image behind it at all) is
// laid out at: rebuildGrid()'s allPlaceholders branch derives its width/height budget from
// pimpl->minTileSize rather than from a constant of its own, so a QSS override moves both
// together and a genuinely small image and an unresolved placeholder always read at the same
// scale. That budget matters because such an item reaches albumLayout() as QSize(1,1), i.e.
// aspect 1.0, and the single-image template's "fill the width budget" rule would otherwise turn
// it into a maxWidth x maxWidth square -- a bubble-width blank tile, visibly wrong for something
// with no image to show.
constexpr int DefaultMinTileSize=100;
//
// DefaultTileMaxUpscale is the paint-time allowance that makes the floor above actually FILL --
// see ChatMessageImageItem::setMaxUpscale()'s own doc comment. albumLayout()'s natural-size cap
// already keeps a tile from exceeding its image's own size, so in practice this only has to cover
// the one case that cap cannot: an image smaller than AlbumLayoutOptions::minCappedTile, whose
// tile is floored there and would otherwise show the image centred on a padded canvas. 2.5x
// covers a source down to ~80px on a DPR-2 display (100*2/80=2.5) reaching the 100px floor;
// smaller sources still pad rather than being blown up further, which is the correct trade-off.
constexpr qreal DefaultTileMaxUpscale=2.5;

// Whether the comment section takes part in this body's geometry.
//
// Deliberately isHidden() rather than isVisible(): isVisible() is false for every widget whose
// ancestors are not shown yet, which is exactly the state a chat message is in while the
// flyweight list builds and measures it off-screen. Asked then, isVisible() says "no comment" and
// the body reports a sizeHint() with no room for it at all, so the comment ends up overlapping
// whatever sits below the album until some later relayout happens to run while the message is on
// screen. isHidden() is the widget's own explicit show/hide state, independent of its ancestors,
// and is exactly what QWidgetItem::isEmpty() consults to make the same decision inside a real
// QLayout -- i.e. what ChatMessageFiles gets for free from its QVBoxLayout.
inline bool commentShown(const ChatMessageText* comment)
{
    return comment!=nullptr && !comment->isHidden();
}

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
        TextFormat commentFormat=TextFormat::Markdown;

        // Cached the same way commentText/commentFormat are: setCopyable() may be called before
        // the comment widget exists (most albums carry none at all, so it is created lazily on
        // first non-empty setComment() -- see ensureComment()), and the flag must still apply once
        // it is.
        bool commentCopyable=false;

        // Same lazy-creation caching as commentCopyable above -- setOwnContextMenuEnabled() may
        // likewise be called before the comment widget exists.
        bool commentOwnContextMenu=true;

        ImageLabel::AnimationMode animationMode=ImageLabel::DefaultAnimationMode;

        // QSS-settable (qproperty-minTileSize/qproperty-tileMaxUpscale, see
        // chatmessageimages.hpp's Q_PROPERTY declarations) -- todo-album-layout-small-tile-
        // packing.md. Read into AlbumLayoutOptions::minCappedTile / ChatMessageImageItem::
        // setMaxUpscale() by rebuildGrid().
        int minTileSize=DefaultMinTileSize;
        qreal tileMaxUpscale=DefaultTileMaxUpscale;

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

    auto previousGrid=pimpl->gridSize;
    rebuildGrid(forMaxWidth);

    // The budget used above is the CURRENT bubble width, which was negotiated for whatever album
    // this widget held before -- a different set of images can want a different width, and on the
    // re-bind path of a flyweight chat list that stale width would simply stick, leaving the
    // tiles in a bubble sized for the previous message. renegotiateBubbleWidth() re-runs the
    // whole negotiation against the real available width (and no-ops before the first one has
    // ever happened, see its own doc comment), after which updateMaximumBubbleWidth() keeps the
    // result rather than re-laying it out -- see its own comment.
    //
    // Guarded on the album's footprint actually changing: setItems() is also the path every
    // non-geometry refresh takes (a transfer-progress tick, see ChatMessage::refreshAllItems() in
    // whitemdesktop), and those must stay as cheap as the layoutUnchanged memo makes them rather
    // than dragging every sibling section through a re-measure several times a second.
    if (pimpl->gridSize!=previousGrid && chatContent()!=nullptr)
    {
        chatContent()->renegotiateBubbleWidth();
    }
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
        if (pimpl->items.empty())
        {
            // layoutUnchanged is trivially satisfied by an empty item list against a previous
            // empty one, but it is ALSO satisfied against a stale signature left over from the
            // last non-empty album (lastLayoutForMaxWidth/lastLayoutDpr unchanged, both
            // pixel-size vectors empty) -- without this, gridSize/rects would keep describing
            // that old album instead of the now-empty one.
            pimpl->gridSize=QSize(0,0);
        }
    }
    else
    {
        // Doubles as both the albumLayout() input AND the layoutUnchanged signature stored below
        // -- these used to be two separate vectors, one substituting unresolved items with a
        // placeholder QSize(1,1), but albumLayout() already treats a non-positive width/height as
        // aspect 1:1 (aspectOf()), so passing pixelSize() through as-is serves both purposes.
        std::vector<QSize> sizes;
        sizes.reserve(pimpl->items.size());
        bool allPlaceholders=!pimpl->items.empty();
        for (const auto& item : pimpl->items)
        {
            auto sz=item.pixelSize();
            bool known=sz.isValid() && sz.width()>0 && sz.height()>0;
            allPlaceholders=allPlaceholders && !known;
            sizes.push_back(sz);
        }

        AlbumLayoutOptions options;
        options.maxWidth=(forMaxWidth>0) ? forMaxWidth : DefaultMaxWidth;
        // Feeds albumLayout()'s per-tile natural-size cap -- pixelSize() is in real pixels while
        // the layout works in logical units, so a 200px-wide original covers exactly 100 logical
        // px on a 2x display and must not be handed a tile wider than that.
        options.devicePixelRatio=dpr;
        // todo-album-layout-small-tile-packing.md: QSS-settable floor a small image's tile is
        // scaled up to (see minTileSize()'s own doc comment) -- separate from AlbumLayoutOptions'
        // own default minTile, which also bounds ordinary template row heights.
        options.minCappedTile=pimpl->minTileSize;

        if (allPlaceholders)
        {
            // See DefaultMinTileSize's own comment for what a placeholder tile is and why it
            // needs a budget of its own. Two tiles wide is the budget every template then works
            // within: the single-image one clamps to a square of exactly the extent (its own
            // maxHeight branch), the two-image one splits the width into two such squares, and
            // three-or-more fall out of their own templates (or the justified-rows fallback) at
            // comparable sizes, with albumLayout()'s uniform scale-down catching anything taller
            // than the height budget.
            //
            // Derived from pimpl->minTileSize with 5% slack rather than pinned to a hardcoded
            // two-tiles-wide budget: at the shipped 100px floor a hardcoded 202 clamp is EXACTLY
            // two floored tiles wide, so any rounding from a non-square placeholder rect (e.g. a
            // 67x66 growing to 102x100 once albumLayout()'s minCappedTile floor -- fed
            // options.minCappedTile=pimpl->minTileSize above -- runs) tips a row over and wraps
            // it to one tile per line, a shape regression even though every tile still clears the
            // floor. The 5% headroom absorbs that rounding; a QSS override of minTileSize is
            // reflected here too, since both read the same property.
            auto placeholderExtent=qRound(pimpl->minTileSize*1.05);
            options.maxWidth=qMin(options.maxWidth,placeholderExtent*2+options.spacing);
            options.maxHeight=(pimpl->items.size()==1)
                ? placeholderExtent
                : qMin(options.maxHeight,placeholderExtent*2+options.spacing);
        }
        // No single-image special case here any more: "never blow a SMALL image up to the full
        // bubble budget" is now albumLayout()'s own per-tile natural-size cap (fed by
        // options.devicePixelRatio above), which applies to every album size rather than only to
        // n==1 -- that asymmetry was itself the reported bug, since a thumbnail sharing a message
        // with a photo got a photo-sized tile while the same thumbnail sent alone got a
        // thumbnail-sized one.

        QSize totalSize;
        rects=albumLayout(sizes,options,&totalSize);
        pimpl->gridSize=totalSize;

        pimpl->lastLayoutForMaxWidth=forMaxWidth;
        pimpl->lastLayoutDpr=dpr;
        pimpl->lastLayoutPixelSizes=std::move(sizes);
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

                        case (ChatFileMenuAction::Download):
                            emit downloadRequested(id);
                            break;

                        case (ChatFileMenuAction::CopyImage):
                            emit copyImageRequested(id);
                            break;
                    }
                }
            );
            connect(tile,&ChatMessageImageItem::pauseRequested,this,[this,tile](){emit pauseRequested(tile->item().id());});
            connect(tile,&ChatMessageImageItem::cancelRequested,this,[this,tile](){emit cancelRequested(tile->item().id());});
            connect(tile,&ChatMessageImageItem::dragPrepareRequested,this,[this,tile](){emit dragPrepareRequested(tile->item().id());});
            connect(tile,&ChatMessageImageItem::dragStartRequested,this,[this,tile](){emit dragStartRequested(tile->item().id());});

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
        // setMaxUpscale() no-ops when unchanged (see its own doc comment), so unconditionally
        // restating pimpl->tileMaxUpscale here for every tile -- new or reused -- is cheap; keeps
        // the paint-time allowance explicit at the one place tiles are populated, rather than
        // relying on ChatMessageImageItem's own constructor default to happen to match. A QSS
        // change reaching setTileMaxUpscale() after tiles already exist updates them directly
        // (see its own doc comment), so this is not the only place it is applied -- just the one
        // that keeps a freshly (re)created tile in sync too.
        pimpl->tiles[i]->setMaxUpscale(pimpl->tileMaxUpscale);
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

    if (commentShown(pimpl->comment))
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
            // The comment must stay a child of THIS widget: layoutChildren() positions it in
            // this widget's coordinate space, so leaving it parented to the MESSAGE widget puts
            // it somewhere over the message instead of below the album. It is created with
            // `this` as parent just above and setChatMessage() no longer reparents (see
            // AbstractChatMessageChild::setChatMessage()), so nothing to restore here -- the
            // explicit setParent(this) this used to need is gone, since Qt does not short-circuit
            // a same-parent setParent() and it would have cost a full subtree repolish.
            pimpl->comment->setChatMessage(chatMessage());
        }
        if (chatContent()!=nullptr)
        {
            pimpl->comment->setChatContent(chatContent());
        }
        if (pimpl->commentCopyable)
        {
            pimpl->comment->setCopyable(true);
        }
        if (!pimpl->commentOwnContextMenu)
        {
            pimpl->comment->setOwnContextMenuEnabled(false);
        }
        // Relayed here so a host (e.g. ReplyDialog's Save/"Quote selected" button swap) can react
        // to selection changes on this body's comment via AbstractChatMessageBody alone -- same
        // idiom ChatMessageFiles uses in its own (eagerly created) comment's ctor.
        connect(pimpl->comment,&AbstractChatMessageBody::selectionChanged,this,&AbstractChatMessageBody::selectionChanged);
        // Same idiom, for a clicked hyperlink in the caption (task-urls-and characters-in-
        // messages.md, Stage 1).
        connect(pimpl->comment,&AbstractChatMessageBody::linkActivated,this,&AbstractChatMessageBody::linkActivated);
        // See rebuildGrid()'s identical comment: freshly created QSS-dependent content must be
        // polished before its first paint.
        pimpl->comment->ensurePolished();
    }
    return pimpl->comment;
}

//--------------------------------------------------------------------------

void ChatMessageImages::setComment(const QString& text, TextFormat format)
{
    pimpl->commentText=text;
    pimpl->commentFormat=format;

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
        comment->loadText(text,format);
        comment->setVisible(true);
    }

    updateGeometry();
    layoutChildren();
}

//--------------------------------------------------------------------------

void ChatMessageImages::clearComment()
{
    setComment(QString(),TextFormat::Markdown);
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

QUuid ChatMessageImages::fileItemAt(const QPoint& pos) const
{
    for (auto* tile : pimpl->tiles)
    {
        if (!tile->isHidden() && tile->rect().contains(tile->mapFrom(this,pos)))
        {
            return tile->item().id();
        }
    }
    return QUuid{};
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

void ChatMessageImages::setMinTileSize(int size)
{
    if (pimpl->minTileSize==size)
    {
        return;
    }
    pimpl->minTileSize=size;

    // QSS qproperty- values land at POLISH time, which can follow the first setItems() call --
    // invalidate the layout memo so rebuildGrid() cannot reuse a layout computed with the old
    // floor (see its layoutUnchanged check), then re-run the negotiation now if there is already
    // an album to redo.
    pimpl->lastLayoutForMaxWidth=-1;
    if (!pimpl->items.empty())
    {
        auto forMaxWidth=(chatContent()!=nullptr && chatContent()->maximumBubbleWidth()>0)
            ? chatContent()->maximumBubbleWidth()
            : DefaultMaxWidth;
        rebuildGrid(forMaxWidth);
        updateGeometry();
    }
}

//--------------------------------------------------------------------------

int ChatMessageImages::minTileSize() const noexcept
{
    return pimpl->minTileSize;
}

//--------------------------------------------------------------------------

void ChatMessageImages::setTileMaxUpscale(qreal maxUpscale)
{
    if (qFuzzyCompare(pimpl->tileMaxUpscale,maxUpscale))
    {
        return;
    }
    pimpl->tileMaxUpscale=maxUpscale;

    // Unlike minTileSize() above, this is a pure paint-time allowance (ChatMessageImageItem::
    // setMaxUpscale()) -- it does not feed albumLayout() at all, so the existing tiles can just
    // be told directly, no re-layout needed.
    for (auto* tile : pimpl->tiles)
    {
        tile->setMaxUpscale(maxUpscale);
    }
}

//--------------------------------------------------------------------------

qreal ChatMessageImages::tileMaxUpscale() const noexcept
{
    return pimpl->tileMaxUpscale;
}

//--------------------------------------------------------------------------

void ChatMessageImages::startItemDrag(const QUuid& id, const QList<QUrl>& urls, const QString& sourceTag)
{
    // Same linear scan as updateItem() -- tiles are only recreated when the item count changes
    // (rebuildGrid()), so pimpl->tiles[i] is the right tile for pimpl->items[i] at any given time.
    for (size_t i=0;i<pimpl->items.size();++i)
    {
        if (pimpl->items[i].id()==id)
        {
            if (i<pimpl->tiles.size())
            {
                pimpl->tiles[i]->startDrag(urls,sourceTag);
            }
            return;
        }
    }
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

bool ChatMessageImages::hasSelectableText() const
{
    // Deliberately does NOT call ensureComment() -- most albums carry no comment at all, and this
    // is a const query that must not allocate the widget just to answer "no text here".
    return pimpl->comment!=nullptr && pimpl->comment->hasSelectableText();
}

//--------------------------------------------------------------------------

void ChatMessageImages::setCopyable(bool enable)
{
    pimpl->commentCopyable=enable;
    if (pimpl->comment!=nullptr)
    {
        pimpl->comment->setCopyable(enable);
    }
}

//--------------------------------------------------------------------------

void ChatMessageImages::setOwnContextMenuEnabled(bool enable)
{
    pimpl->commentOwnContextMenu=enable;
    if (pimpl->comment!=nullptr)
    {
        pimpl->comment->setOwnContextMenuEnabled(enable);
    }
}

//--------------------------------------------------------------------------

void ChatMessageImages::selectText(const QString& text)
{
    if (pimpl->comment!=nullptr)
    {
        pimpl->comment->selectText(text);
    }
}

//--------------------------------------------------------------------------

QString ChatMessageImages::linkAt(const QPoint& pos) const
{
    // Deliberately does NOT call ensureComment() -- same reasoning as hasSelectableText() above.
    if (pimpl->comment==nullptr)
    {
        return QString();
    }
    return pimpl->comment->linkAt(pimpl->comment->mapFrom(this,pos));
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

    // This runs from AbstractChatMessageContent::setMaximumBubbleWidth(), i.e. immediately AFTER
    // the negotiation pass that asked bubbleWidthHint() for this album's own width and then sized
    // the bubble from it. Re-running the layout against that narrower number is not a refinement
    // but a different question ("how would this album look in a bubble this wide?"), and the
    // answer routinely differs: the album's width is not a fixed point of the layout, because how
    // many tiles fit per row -- hence how tall the album is, hence how hard the maxHeight rescue
    // shrinks it -- depends on the budget. Observed with a real 8-image message: negotiating at a
    // 800px viewport produced a 381px-wide album, and laying that same album out again at 381
    // produced a 179px-wide one. The bubble keeps the width it was already given, so the
    // difference shows up as a band of empty space to the right of the tiles, nearly as wide as
    // the album itself.
    //
    // So: keep the layout the negotiation settled on whenever it still fits, by re-running
    // rebuildGrid() against the budget it was computed for (which hits its layoutUnchanged memo,
    // leaving geometry untouched while still refreshing the tiles). Only a budget the album no
    // longer fits into is a real constraint change worth re-laying out for. The bubble then ends
    // up at exactly max(album width, whatever minimum the other sections impose -- see
    // ChatMessageBottom::bubbleWidthHint()'s narrow-body rule).
    if (pimpl->lastLayoutForMaxWidth>0 && pimpl->gridSize.width()<=forMaxWidth)
    {
        forMaxWidth=pimpl->lastLayoutForMaxWidth;
    }
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
        // setChatMessage() no longer reparents (and so no longer hides) the comment -- see
        // AbstractChatMessageChild::setChatMessage(). It stays the child of `this` that
        // ensureComment() created, so only its visibility still needs asserting here.
        pimpl->comment->setChatMessage(chatMessage());
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
    if (commentShown(pimpl->comment))
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
