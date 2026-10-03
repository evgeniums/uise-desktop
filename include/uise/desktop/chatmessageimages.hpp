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

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class ChatMessageText;
class ChatMessageImages_p;

/**
 * @brief Concrete image chat message body: an exact-aspect, gapless album grid (see
 *  albumLayout()) of ChatMessageImageItem tiles, followed by an optional embedded
 *  ChatMessageText comment -- exactly the same comment-reuse idiom as ChatMessageFiles.
 *
 * No QLayout is used anywhere in this class: both the tiles and the comment are positioned with
 * manual geometry in layoutChildren(), called from resizeEvent() and from the QEvent::
 * LayoutRequest handler in event() (the latter is how a child's updateGeometry() reaches a
 * layout-less parent -- see layoutChildren()'s own doc comment). The comment is created lazily,
 * on the first non-empty setComment() -- most albums carry no comment at all. The tile block is
 * horizontally centered against the comment's own width (see layoutChildren()) whenever a long
 * comment widens this body past the album's natural size -- e.g. a small image or two beside a
 * long description no longer sit hard against the left edge with empty space to their right.
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

    // QSS-settable, same idiom as qproperty-maxBubbleWidth on uise--ChatMessageFiles (see
    // chatmessagefiles.qss) -- declared here rather than on AbstractChatMessageImages because the
    // setters must invalidate THIS class's own layout memo (see rebuildGrid()'s layoutUnchanged
    // check).
    Q_PROPERTY(int minTileSize READ minTileSize WRITE setMinTileSize)
    Q_PROPERTY(qreal tileMaxUpscale READ tileMaxUpscale WRITE setTileMaxUpscale)
    Q_PROPERTY(qreal maxWidthRatio READ maxWidthRatio WRITE setMaxWidthRatio)
    Q_PROPERTY(int maxBubbleWidth READ maxBubbleWidth WRITE setMaxBubbleWidth)

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

        void setLayoutMode(AlbumLayoutMode mode) override;
        AlbumLayoutMode layoutMode() const override;

        void startItemDrag(const QUuid& id, const QList<QUrl>& urls, const QString& sourceTag) override;

        void clearContentSelection() override;

        QString selectedText() const override;

        bool hasSelectableText() const override;

        void setCopyable(bool enable) override;

        void setOwnContextMenuEnabled(bool enable) override;

        void selectText(const QString& text, int hintOffset=-1) override;

        int selectionStart() const override;

        bool highlightText(const QString& text, int hintOffset=-1) override;

        void setTextHighlightFactor(qreal factor) override;

        QRect textRect(const QString& text, int hintOffset=-1) const override;

        QString linkAt(const QPoint& pos) const override;

        QUuid fileItemAt(const QPoint& pos) const override;

        bool isBubbleTransparentHint() const override;

        int bubbleWidthHint(int forMaxWidth) override;

        void updateMaximumBubbleWidth() override;

        QRect lastTextLineRect() const override;

        QSize sizeHint() const override;

        QSize minimumSizeHint() const override;

        /**
         * @brief Floor (logical px) on a tile's short side for the uniform all-thumbnail shrink:
         *  an album made only of small images is scaled down as a whole towards their natural
         *  size, but never so far that any tile's short side drops below this -- see
         *  AlbumLayoutOptions::shrinkFloor's own doc comment (albumlayout.hpp). Doubles as the
         *  extent an unresolved placeholder tile is laid out at (see rebuildGrid()), so a
         *  genuinely small image and a placeholder read at the same scale. Settable from QSS via
         *  qproperty-minTileSize (see chatmessagefiles.qss).
         */
        void setMinTileSize(int size);

        int minTileSize() const noexcept;

        /**
         * @brief How far a tile may enlarge its content beyond the image's own natural
         *  resolution on its FALLBACK paint paths -- forwarded to every tile via
         *  ChatMessageImageItem::setMaxUpscale(), see its doc comment for which paths those are.
         *  A tile whose rect matches its image's aspect ratio (every tile albumLayout() lays out)
         *  covers its rect regardless of resolution and does not consult this. Settable from QSS
         *  via qproperty-tileMaxUpscale.
         */
        void setTileMaxUpscale(qreal maxUpscale);

        qreal tileMaxUpscale() const noexcept;

        /**
         * @brief Share of the width the view offers (the negotiated forMaxWidth, i.e. the chat's
         *  content width up to its maxMessageWidth) that the album may take -- 0.7 by default, so
         *  an image message never spans the whole viewport the way the Wide layout otherwise
         *  would. Values outside (0,1] mean "no ratio cap". Combined with maxBubbleWidth() below
         *  by taking the smaller budget. Settable from QSS via qproperty-maxWidthRatio.
         */
        void setMaxWidthRatio(qreal ratio);

        qreal maxWidthRatio() const noexcept;

        /**
         * @brief Absolute cap (logical px) on the album's width budget, same idea as
         *  AbstractChatMessageFiles::maxBubbleWidth(); 0 (the default) disables it. Settable from
         *  QSS via qproperty-maxBubbleWidth. The caption keeps its own text cap regardless.
         */
        void setMaxBubbleWidth(int width);

        int maxBubbleWidth() const noexcept;

    protected:

        void updateChatMessage() override;

        void resizeEvent(QResizeEvent* event) override;

        bool event(QEvent* event) override;

    private:

        void rebuildGrid(int forMaxWidth);

        //! Invalidates the layout memo and re-lays the album out after one of the width caps
        //! (maxWidthRatio/maxBubbleWidth) changed -- see setMinTileSize() for the same idiom.
        void relayoutForCapChange();

        //! Single placement path for every child (tiles + comment), replacing the QLayout this
        //! class used to have -- see the class doc comment. Also centers the tile block
        //! horizontally when the comment is wider than the album.
        void layoutChildren();

        //! Create the comment widget on first use -- see the class doc comment.
        ChatMessageText* ensureComment();

        std::unique_ptr<ChatMessageImages_p> pimpl;
};

}

#endif // UISE_DESKTOP_CHATMESSAGEIMAGES_HPP
