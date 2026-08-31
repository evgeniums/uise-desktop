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

/** @file uise/desktop/chatmessageimages.hpp
*
*  Declares ChatMessageImages.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATMESSAGEIMAGES_HPP
#define UISE_DESKTOP_CHATMESSAGEIMAGES_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractchatmessageimages.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class ChatMessageText;
class ChatMessageImages_p;

/**
 * @brief Concrete image chat message body: a Telegram-style album grid (see albumLayout()) of
 *  ChatMessageImageItem tiles, followed by an optional embedded ChatMessageText comment --
 *  exactly the same comment-reuse idiom as ChatMessageFiles.
 *
 * No QLayout is used anywhere in this class: both the tiles and the comment are positioned with
 * manual geometry in layoutChildren(), called from resizeEvent() and from the QEvent::
 * LayoutRequest handler in event() (the latter is how a child's updateGeometry() reaches a
 * layout-less parent -- see layoutChildren()'s own doc comment). The comment is created lazily,
 * on the first non-empty setComment() -- most albums carry no comment at all.
 *
 * The grid geometry is recomputed against fresh QRects on every bubbleWidthHint()/
 * updateMaximumBubbleWidth() call, not cached -- the same "just redo it" approach
 * ChatMessageText::bubbleWidthHint() itself already uses for re-wrapping. The tiles themselves
 * are only destroyed and rebuilt when the item count changes, not on every such call, so that a
 * bubble-width renegotiation (e.g. a view resize) does not restart any tile's animated content
 * (see ChatMessageImages::rebuildGrid()).
 */
class UISE_DESKTOP_EXPORT ChatMessageImages : public AbstractChatMessageImages
{
    Q_OBJECT

    // todo-album-layout-small-tile-packing.md: QSS-settable, same idiom as qproperty-
    // maxBubbleWidth on uise--ChatMessageFiles (see chatmessagefiles.qss) -- declared here rather
    // than on AbstractChatMessageImages because the setters must invalidate THIS class's own
    // layout memo (see rebuildGrid()'s layoutUnchanged check).
    Q_PROPERTY(int minTileSize READ minTileSize WRITE setMinTileSize)
    Q_PROPERTY(qreal tileMaxUpscale READ tileMaxUpscale WRITE setTileMaxUpscale)

    public:

        explicit ChatMessageImages(QWidget* parent=nullptr);

        ~ChatMessageImages();

        ChatMessageImages(const ChatMessageImages&)=delete;
        ChatMessageImages(ChatMessageImages&&)=delete;
        ChatMessageImages& operator=(const ChatMessageImages&)=delete;
        ChatMessageImages& operator=(ChatMessageImages&&)=delete;

        void setItems(ChatFileItems items) override;
        const ChatFileItems& items() const override;
        void updateItem(const QUuid& id, const ChatFileItem& item) override;

        void setComment(const QString& text, TextFormat format=TextFormat::Markdown) override;
        void clearComment() override;
        QString comment() const override;

        void closeMenus() override;

        void setAnimationMode(ImageLabel::AnimationMode mode) override;
        ImageLabel::AnimationMode animationMode() const override;

        void startItemDrag(const QUuid& id, const QList<QUrl>& urls, const QString& sourceTag) override;

        void clearContentSelection() override;

        QString selectedText() const override;

        bool hasSelectableText() const override;

        void setCopyable(bool enable) override;

        void setOwnContextMenuEnabled(bool enable) override;

        void selectText(const QString& text) override;

        QString linkAt(const QPoint& pos) const override;

        QUuid fileItemAt(const QPoint& pos) const override;

        int bubbleWidthHint(int forMaxWidth) override;

        void updateMaximumBubbleWidth() override;

        QSize sizeHint() const override;

        QSize minimumSizeHint() const override;

        /**
         * @brief Hard floor (logical px) on BOTH dimensions of every tile, aspect preserved,
         *  instead of being left genuinely tiny -- see AlbumLayoutOptions::minCappedTile's own
         *  doc comment for the mechanism (albumlayout.hpp) and TileMaxUpscale/tileMaxUpscale()
         *  for the paint-time counterpart that actually fills the floored tile. Applies to any
         *  densely-packed tile a template happened to size small, not just a small-resolution
         *  image -- see albumLayout()'s own doc comment for why the two used to be conflated.
         *  Settable from QSS via qproperty-minTileSize (see chatmessagefiles.qss).
         */
        void setMinTileSize(int size);

        int minTileSize() const noexcept;

        /**
         * @brief How far a tile may enlarge its content beyond the image's own natural
         *  resolution -- forwarded to every tile via ChatMessageImageItem::setMaxUpscale(). Needs
         *  to be raised together with minTileSize() so a small source can actually reach the new
         *  floor rather than sitting centred on a padded canvas (see this class's own rebuildGrid()
         *  for the derivation). Settable from QSS via qproperty-tileMaxUpscale.
         */
        void setTileMaxUpscale(qreal maxUpscale);

        qreal tileMaxUpscale() const noexcept;

    protected:

        void updateChatMessage() override;

        void resizeEvent(QResizeEvent* event) override;

        bool event(QEvent* event) override;

    private:

        void rebuildGrid(int forMaxWidth);

        //! Single placement path for every child (tiles + comment), replacing the QLayout this
        //! class used to have -- see the class doc comment.
        void layoutChildren();

        //! Create the comment widget on first use -- see the class doc comment.
        ChatMessageText* ensureComment();

        std::unique_ptr<ChatMessageImages_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATMESSAGEIMAGES_HPP
