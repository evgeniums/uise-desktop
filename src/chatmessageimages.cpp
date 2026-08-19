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

// How far a tile may upscale its content beyond the item's own natural (logical) resolution --
// see ChatMessageImageItem::setMaxUpscale()'s own doc comment. albumLayout()'s natural-size cap
// already keeps a tile from exceeding its image's own size, so in practice this only has to cover
// the one case that cap cannot: an image smaller than AlbumLayoutOptions::minTile, whose tile is
// floored at minTile and would otherwise show the image centred on a padded canvas.
constexpr qreal TileMaxUpscale=2.0;

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
        // setMaxUpscale() no-ops when unchanged (see its own doc comment), so unconditionally
        // restating TileMaxUpscale here for every tile -- new or reused -- is cheap; keeps the
        // paint-time allowance explicit at the one place tiles are populated, rather than relying
        // on ChatMessageImageItem's own constructor default to happen to match.
        pimpl->tiles[i]->setMaxUpscale(TileMaxUpscale);
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
            // AbstractChatMessageChild::setChatMessage() reparents the comment onto
            // chatMessage() as a side effect -- put it straight back under this widget, exactly
            // as updateChatMessage() does for an already-existing comment. Without this the
            // comment is left as a direct child of the MESSAGE widget while layoutChildren()
            // keeps positioning it in THIS widget's coordinate space, so it lands somewhere over
            // the message instead of below the album. This is the normal path, not an edge case:
            // the body is wired into the content (and so given its chatMessage()) by
            // setWidgets() before setComment() ever runs, so the very first ensureComment()
            // always takes this branch. ChatMessageFiles never hit this because it creates its
            // comment in the constructor -- before any chatMessage exists -- and its
            // updateChatMessage() re-adds it to the body's own QLayout afterwards.
            pimpl->comment->setChatMessage(chatMessage());
            pimpl->comment->setParent(this);
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
