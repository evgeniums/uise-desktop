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

class ChatMessageImages_p;

/**
 * @brief Concrete image chat message body: a Telegram-style album grid (see albumLayout()) of
 *  ChatMessageImageItem tiles, positioned with manual geometry inside a fixed-size grid frame,
 *  followed by an embedded ChatMessageText used as the optional comment -- exactly the same
 *  comment-reuse idiom as ChatMessageFiles.
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

        void setComment(const QString& text, bool markdown=true) override;
        void clearComment() override;
        QString comment() const override;

        void closeMenus() override;

        void setAnimationMode(ImageLabel::AnimationMode mode) override;
        ImageLabel::AnimationMode animationMode() const override;

        void clearContentSelection() override;

        QString selectedText() const override;

        int bubbleWidthHint(int forMaxWidth) override;

        void updateMaximumBubbleWidth() override;

    protected:

        void updateChatMessage() override;

    private:

        void rebuildGrid(int forMaxWidth);

        std::unique_ptr<ChatMessageImages_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATMESSAGEIMAGES_HPP
