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

/** @file uise/desktop/chatmessagesenderheader.hpp
*
*  Declares ChatMessageSenderHeader.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATMESSAGESENDERHEADER_HPP
#define UISE_DESKTOP_CHATMESSAGESENDERHEADER_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractchatmessage.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class ChatMessageSenderHeader_p;

/**
 * @brief Default (and, as of this writing, only) AbstractChatMessageSenderHeader implementation
 *  -- renders the sender's bare display name above every other bubble header content,
 *  group chats only (task-basic-group-chats-plan.md Stage 8). Decoration is entirely QSS-driven
 *  -- see chatmessagesenderheader.qss. Style rules for this section must target
 *  uise--ChatMessageSenderHeader specifically, not the abstract uise--AbstractChatMessageHeader/
 *  uise--AbstractChatMessageSenderHeader bases -- Qt type selectors match subclasses.
 *
 * Deliberately a single label, unlike ChatMessageForwardHeader's three (prefix/author/suffix):
 * there is no surrounding sentence to split around a %1, just a bare name.
 */
class UISE_DESKTOP_EXPORT ChatMessageSenderHeader : public AbstractChatMessageSenderHeader
{
    Q_OBJECT

    //! QSS: qproperty-maxWidthHint: 320; -- 0 disables this section's influence on bubble-width
    //! negotiation, same idiom as ChatMessageForwardHeader::maxWidthHint.
    Q_PROPERTY(int maxWidthHint READ maxWidthHint WRITE setMaxWidthHint)

    public:

        explicit ChatMessageSenderHeader(QWidget* parent=nullptr);

        ~ChatMessageSenderHeader();
        ChatMessageSenderHeader(const ChatMessageSenderHeader&)=delete;
        ChatMessageSenderHeader(ChatMessageSenderHeader&&)=delete;
        ChatMessageSenderHeader& operator=(const ChatMessageSenderHeader&)=delete;
        ChatMessageSenderHeader& operator=(ChatMessageSenderHeader&&)=delete;

        void setSenderTitle(QString title) override;
        QString senderTitle() const override;

        //! Whether the name reacts to clicks (cursor, hover, clicked()). Default true.
        void setClickable(bool enable);
        bool isClickable() const;

        void setMaxWidthHint(int width) noexcept;
        int maxWidthHint() const noexcept;

        int bubbleWidthHint(int forMaxWidth) override;
        void updateMaximumBubbleWidth() override;

        void setSelected(bool enable) override;
        void setSent(bool enable) override;

    protected:

        bool eventFilter(QObject* obj, QEvent* event) override;

    private:

        std::unique_ptr<ChatMessageSenderHeader_p> pimpl;
};

}

#endif // UISE_DESKTOP_CHATMESSAGESENDERHEADER_HPP
