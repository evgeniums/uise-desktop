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

/** @file uise/desktop/chatmessagereply.hpp
*
*  Declares ChatMessageReply.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATMESSAGEREPLY_HPP
#define UISE_DESKTOP_CHATMESSAGEREPLY_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractchatmessage.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class ChatMessageReply_p;

/**
 * @brief Default AbstractChatMessageReply implementation -- a thin, clickable wrapper around
 *  one AbstractReplyPreview block (the same block AbstractReplyBar/AbstractReplyDialog embed).
 *  Decoration (accent bar, tinted background) is entirely QSS-driven -- see replypreview.qss.
 */
class UISE_DESKTOP_EXPORT ChatMessageReply : public AbstractChatMessageReply
{
    Q_OBJECT

    public:

        explicit ChatMessageReply(QWidget* parent=nullptr);

        ~ChatMessageReply();
        ChatMessageReply(const ChatMessageReply&)=delete;
        ChatMessageReply(ChatMessageReply&&)=delete;
        ChatMessageReply& operator=(const ChatMessageReply&)=delete;
        ChatMessageReply& operator=(ChatMessageReply&&)=delete;

        void setReplyData(ReplyPreviewData data) override;
        const ReplyPreviewData& replyData() const override;

        AbstractReplyPreview* preview() const override;

        void setOriginalDeleted(bool enable) override;
        bool isOriginalDeleted() const override;

        int bubbleWidthHint(int forMaxWidth) override;
        void updateMaximumBubbleWidth() override;

        void setSelected(bool enable) override;
        void setSent(bool enable) override;

    private:

        std::unique_ptr<ChatMessageReply_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATMESSAGEREPLY_HPP
