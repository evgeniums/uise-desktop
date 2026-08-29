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

/** @file uise/desktop/replypreviewdata.hpp
*
*  Declares ReplyPreviewData.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_REPLYPREVIEWDATA_HPP
#define UISE_DESKTOP_REPLYPREVIEWDATA_HPP

#include <QString>
#include <QDateTime>
#include <QImage>
#include <QMetaType>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief What kind of content the replied-to message carries.
 *
 * Drives exactly one rendering decision in AbstractReplyPreview: the icon slot is shown (with
 * ReplyPreviewData::thumbnail()) only for Image, and stays hidden for every other kind -- see
 * ReplyPreview::isIconSlotVisible(). There is no other per-kind icon; a non-image reply carries
 * no icon at all.
 */
enum class ReplyMessageKind : uint8_t
{
    Text,
    Image,
    File,
    Call,
    Deleted,
    Unknown
};

//! Default number of characters of a plain (non-quote) original message's text carried into a
//! reply preview -- see trimReplyText() and AbstractReplyPreview::textTrimLength().
constexpr const int DefaultReplyTextTrimLength=200;

//! Default number of characters of a QUOTED fragment (ReplyPreviewData::isQuote()) carried into
//! a reply preview or emitted by AbstractReplyDialog's "Quote selected" -- kept as a distinct
//! limit from DefaultReplyTextTrimLength/textTrimLength(): a quote is already the user's own
//! deliberately hand-picked selection, not necessarily the same length as the trim policy
//! applied to a full original message, see AbstractReplyPreview::quoteTrimLength().
constexpr const int DefaultReplyQuoteTrimLength=200;

//! Default AbstractReplyPreview::maxWidthHint() -- see that property's own doc comment for why
//! a reply preview block caps its own contribution to bubble-width negotiation.
constexpr const int DefaultReplyMaxWidthHint=320;

/**
 * @brief Data shown by AbstractReplyPreview -- the block reused by the short reply bar above
 *  the chat editor, the reply section inside a message bubble, and the full-preview dialog.
 *
 * A plain, cheaply-copyable value type (QString/QImage are implicitly shared) -- no pimpl,
 * modelled after ChatFileItem. Every field is a plain stored property; this widget layer has no
 * knowledge of where the data came from (a message DU, a server-side descriptor, ...) -- that
 * is entirely the host's concern.
 */
class UISE_DESKTOP_EXPORT ReplyPreviewData
{
    public:

        ReplyPreviewData()=default;

        /**
         * @brief Get the host's own opaque identifier of the replied-to message.
         * @return Empty unless the host sets one. Not used by this widget layer itself --
         *  carried through so a host wiring AbstractChatMessageReply::clicked() or a "Show in
         *  chat" action can tell which message to jump to, without the widget needing to know
         *  the host's id scheme.
         */
        QString messageId() const
        {
            return m_messageId;
        }

        void setMessageId(QString id)
        {
            m_messageId=std::move(id);
        }

        QString senderTitle() const
        {
            return m_senderTitle;
        }

        void setSenderTitle(QString title)
        {
            m_senderTitle=std::move(title);
        }

        QDateTime dateTime() const
        {
            return m_dateTime;
        }

        void setDateTime(QDateTime dateTime)
        {
            m_dateTime=std::move(dateTime);
        }

        /**
         * @brief Get the (untrimmed) text carried by this reply.
         * @return The full text as set -- trimming for DISPLAY happens in AbstractReplyPreview
         *  itself (see its textTrimLength() property and trimReplyText()), not here. A host
         *  trimming for SENDING should call trimReplyText() directly on its own copy of the
         *  original message's text before ever constructing a ReplyPreviewData, rather than
         *  relying on this class to do it -- see trimReplyText()'s own doc comment.
         */
        QString text() const
        {
            return m_text;
        }

        void setText(QString text)
        {
            m_text=std::move(text);
        }

        ReplyMessageKind kind() const noexcept
        {
            return m_kind;
        }

        void setKind(ReplyMessageKind kind) noexcept
        {
            m_kind=kind;
        }

        /**
         * @brief Get the decoded thumbnail image, if one has been supplied.
         * @return A possibly-null QImage -- only consulted when kind()==ReplyMessageKind::Image;
         *  a null image there means the icon slot stays hidden (see
         *  ReplyPreview::isIconSlotVisible()) rather than falling back to any glyph. Thumbnails
         *  are decoded off-thread by the host and handed in here, never decoded by this class
         *  itself.
         */
        QImage thumbnail() const
        {
            return m_thumbnail;
        }

        void setThumbnail(QImage thumbnail)
        {
            m_thumbnail=std::move(thumbnail);
        }

        /**
         * @brief Check whether the original message has been deleted.
         * @return True if AbstractReplyPreview should render its tombstone (deletedText()
         *  instead of text(), no title, no icon, thumbnail() ignored) rather than the normal
         *  preview -- see AbstractReplyPreview::setDeletedText().
         */
        bool isDeleted() const noexcept
        {
            return m_deleted;
        }

        void setDeleted(bool enable) noexcept
        {
            m_deleted=enable;
        }

        /**
         * @brief Check whether text() is a user-picked fragment of the original message rather
         *  than its (possibly trimmed) full text.
         * @return True for a reply built from ReplyDialog's "Quote selected" action. Purely
         *  informational for this widget layer -- ReplyPreview marks it with a QSS "quote"
         *  property (see replypreview.qss) but renders text() identically either way.
         */
        bool isQuote() const noexcept
        {
            return m_quote;
        }

        void setQuote(bool enable) noexcept
        {
            m_quote=enable;
        }

        /**
         * @brief Check whether senderTitle() is only a height-reservation placeholder, not real
         *  content yet.
         * @return True when the host has not yet resolved a real sender title for this reply
         *  (todo-reply-block-bubble-width-flicker.md) -- senderTitle() is still non-empty (kept
         *  non-empty so AbstractReplyPreview::isTitleShown() reserves the title row's height from
         *  the very first frame), but AbstractReplyPreview::contentWidthHint() must not let it
         *  drive width negotiation, since it carries no real width information yet. Cleared
         *  implicitly once the host applies the real, resolved data (setTitlePending() is never
         *  called there, so it defaults back to false).
         */
        bool isTitlePending() const noexcept
        {
            return m_titlePending;
        }

        void setTitlePending(bool enable) noexcept
        {
            m_titlePending=enable;
        }

        /**
         * @brief Check whether there is nothing at all to show.
         * @return True if every field is at its default -- a host can use this to decide
         *  whether to show/hide a reply bar or bubble section entirely, rather than calling
         *  AbstractReplyPreview::setData() with an empty struct and letting it render blank.
         */
        bool isEmpty() const noexcept
        {
            return m_messageId.isEmpty() && m_senderTitle.isEmpty() && m_text.isEmpty()
                && m_thumbnail.isNull() && !m_deleted;
        }

    private:

        QString m_messageId;
        QString m_senderTitle;
        QDateTime m_dateTime;
        QString m_text;
        ReplyMessageKind m_kind=ReplyMessageKind::Unknown;
        QImage m_thumbnail;
        bool m_deleted=false;
        bool m_quote=false;
        bool m_titlePending=false;
};

/**
 * @brief Trim a message's text down to a single-line preview, the single place this rule is
 *  defined so a host trims for SENDING (the reply actually stored/transmitted) exactly the way
 *  AbstractReplyPreview trims for DISPLAY.
 * @param text Source text, as typed/received -- may contain newlines/multiple paragraphs.
 * @param maxLength Maximum length of the result, in characters; 0 or negative disables
 *  truncation (the text is still collapsed to one line).
 * @return @a text with all whitespace runs (including newlines) collapsed to single spaces and
 *  leading/trailing whitespace removed, then truncated to at most @a maxLength characters at
 *  the last word boundary at or before that length (or at exactly @a maxLength if no boundary
 *  falls within range, i.e. a single word longer than the limit). No ellipsis is appended --
 *  that is ElidedLabel's job when the result is shown at a width narrower than its own natural
 *  size, and appending one here would risk doubling up as "...trimmed text......" once elided
 *  on top of an already-trimmed string.
 */
UISE_DESKTOP_EXPORT QString trimReplyText(const QString& text, int maxLength=DefaultReplyTextTrimLength);

UISE_DESKTOP_NAMESPACE_END

Q_DECLARE_METATYPE(UISE_DESKTOP_NAMESPACE::ReplyPreviewData)

#endif // UISE_DESKTOP_REPLYPREVIEWDATA_HPP
