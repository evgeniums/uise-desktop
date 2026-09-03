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

/** @file uise/desktop/chatmessagecomment.hpp
*
*  Declares ChatMessageComment.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATMESSAGECOMMENT_HPP
#define UISE_DESKTOP_CHATMESSAGECOMMENT_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractchatmessage.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class ChatMessageComment_p;

/**
 * @brief Default AbstractChatMessageComment implementation -- a thin wrapper around one
 *  embedded ChatMessageText, the same recipe ChatMessageFiles/ChatMessageImages already use for
 *  their own optional comment (see chatmessagefiles.cpp). Decoration is entirely QSS-driven --
 *  see forwardpreview.qss.
 */
class UISE_DESKTOP_EXPORT ChatMessageComment : public AbstractChatMessageComment
{
    Q_OBJECT

    public:

        explicit ChatMessageComment(QWidget* parent=nullptr);

        ~ChatMessageComment();
        ChatMessageComment(const ChatMessageComment&)=delete;
        ChatMessageComment(ChatMessageComment&&)=delete;
        ChatMessageComment& operator=(const ChatMessageComment&)=delete;
        ChatMessageComment& operator=(ChatMessageComment&&)=delete;

        void setComment(const QString& text, TextFormat format=TextFormat::Markdown) override;
        void clearComment() override;
        QString comment() const override;

        void clearContentSelection() override;
        QString selectedText() const override;
        bool hasSelectableText() const override;
        void setCopyable(bool enable) override;
        void setOwnContextMenuEnabled(bool enable) override;
        void selectText(const QString& text) override;
        QString linkAt(const QPoint& pos) const override;

        int bubbleWidthHint(int forMaxWidth) override;
        void updateMaximumBubbleWidth() override;

        void setSelected(bool enable) override;
        void setSent(bool enable) override;

    protected:

        void updateChatMessage() override;

    private:

        std::unique_ptr<ChatMessageComment_p> pimpl;
};

}

#endif // UISE_DESKTOP_CHATMESSAGECOMMENT_HPP
