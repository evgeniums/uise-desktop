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

/** @file uise/desktop/chatmessageforwardheader.hpp
*
*  Declares ChatMessageForwardHeader.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATMESSAGEFORWARDHEADER_HPP
#define UISE_DESKTOP_CHATMESSAGEFORWARDHEADER_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractchatmessage.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class ChatMessageForwardHeader_p;

/**
 * @brief Default (and, as of this writing, only) AbstractChatMessageHeader implementation --
 *  renders "Forwarded from {author}" at the top of a bubble, task-message-forwarding.md's
 *  bubble header. Decoration is entirely QSS-driven -- see forwardpreview.qss. Style rules for
 *  this section must target uise--ChatMessageForwardHeader specifically, not the abstract
 *  uise--AbstractChatMessageHeader base -- Qt type selectors match subclasses, and that base is
 *  shared by every future header implementation.
 */
class UISE_DESKTOP_EXPORT ChatMessageForwardHeader : public AbstractChatMessageHeader
{
    Q_OBJECT

    //! QSS: qproperty-maxWidthHint: 320; -- 0 disables this section's influence on bubble-width
    //! negotiation, same idiom as AbstractReplyPreview::maxWidthHint.
    Q_PROPERTY(int maxWidthHint READ maxWidthHint WRITE setMaxWidthHint)

    public:

        explicit ChatMessageForwardHeader(QWidget* parent=nullptr);

        ~ChatMessageForwardHeader();
        ChatMessageForwardHeader(const ChatMessageForwardHeader&)=delete;
        ChatMessageForwardHeader(ChatMessageForwardHeader&&)=delete;
        ChatMessageForwardHeader& operator=(const ChatMessageForwardHeader&)=delete;
        ChatMessageForwardHeader& operator=(ChatMessageForwardHeader&&)=delete;

        //! Original author's display title -- shown in place of titleFormat()'s %1. A host
        //! should always populate this (never leave it empty while an async character-title
        //! fetch is in flight) with a preset fallback title, per task-message-forwarding.md.
        void setAuthorTitle(QString title);
        QString authorTitle() const;

        //! Opaque host identifier of the original author -- not used by this widget layer
        //! itself, carried through so a host wiring authorClicked() can tell which character to
        //! open (see task-message-forwarding.md: "currently show Not implemented toast and add
        //! todo to handle character opening"). Same contract as ReplyPreviewData::messageId().
        void setAuthorId(QString id);
        QString authorId() const;

        //! Default tr("Forwarded from %1") -- %1 is authorTitle(). A format with no %1 at all
        //! renders literally with no clickable region. An empty prefix or suffix around %1 (a
        //! translator reordering the sentence) hides that side entirely rather than leaving a
        //! zero-width label with visible QSS padding/margin.
        void setTitleFormat(const QString& format);
        QString titleFormat() const;

        //! Whether the author-title portion reacts to clicks (cursor, hover, authorClicked()).
        //! Default true.
        void setAuthorClickable(bool enable);
        bool isAuthorClickable() const;

        void setMaxWidthHint(int width) noexcept;
        int maxWidthHint() const noexcept;

        int bubbleWidthHint(int forMaxWidth) override;
        void updateMaximumBubbleWidth() override;

        void setSelected(bool enable) override;
        void setSent(bool enable) override;

    signals:

        //! The author-title portion was clicked -- see setAuthorId()'s doc comment.
        void authorClicked();

    protected:

        bool eventFilter(QObject* obj, QEvent* event) override;

    private:

        //! Re-splits titleFormat() at %1 and re-applies it to prefixLabel/authorLabel/
        //! suffixLabel -- see setTitleFormat()'s own doc comment for the three cases handled.
        void refresh();

        std::unique_ptr<ChatMessageForwardHeader_p> pimpl;
};

}

#endif // UISE_DESKTOP_CHATMESSAGEFORWARDHEADER_HPP
