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

/** @file uise/desktop/abstractreplypreview.hpp
*
*  Declares AbstractReplyPreview.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTREPLYPREVIEW_HPP
#define UISE_DESKTOP_ABSTRACTREPLYPREVIEW_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/replypreviewdata.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Interface of the reply-preview block: an accent bar, an icon slot (a thumbnail --
 *  ONLY for a reply to an image message, otherwise not shown at all, see contentWidthHint()),
 *  a "Reply to {sender}, {datetime}" title (with an optional quote glyph, see
 *  quoteIconVisible) and an elided line of the replied-to message's text.
 *
 * The SAME block, embedded via makeWidget<AbstractReplyPreview,ReplyPreview>(), is reused by
 * three call sites -- the short reply bar shown above the chat editor (AbstractReplyBar), the
 * reply section inside a message bubble (AbstractChatMessageReply), and the full-preview modal
 * (AbstractReplyDialog) -- so a host substituting a custom implementation changes the look
 * everywhere at once.
 */
class UISE_DESKTOP_EXPORT AbstractReplyPreview : public WidgetQFrame
{
    Q_OBJECT

    //! QSS: qproperty-textTrimLength: 200; -- applied to a PLAIN reply's text(), see setData().
    Q_PROPERTY(int textTrimLength READ textTrimLength WRITE setTextTrimLength)
    //! QSS: qproperty-quoteTrimLength: 200; -- applied instead of textTrimLength() whenever
    //! data().isQuote() -- kept as a separate limit since a quote is already the user's own
    //! deliberately hand-picked fragment, see setData()'s own doc comment.
    Q_PROPERTY(int quoteTrimLength READ quoteTrimLength WRITE setQuoteTrimLength)
    //! QSS: qproperty-maxWidthHint: 320; -- 0 disables this block's influence on bubble-width
    //! negotiation, see contentWidthHint().
    Q_PROPERTY(int maxWidthHint READ maxWidthHint WRITE setMaxWidthHint)
    Q_PROPERTY(bool accentBarVisible READ isAccentBarVisible WRITE setAccentBarVisible)
    //! QSS: qproperty-quoteIconVisible: true; -- distinguishes the block embedded in an
    //! already-sent/received bubble (AbstractChatMessageReply) from the same block shown while
    //! still PREPARING the reply (AbstractReplyBar): the glyph marks "this is a reply" on a
    //! finalized message, which reads as premature/confusing before the reply is even sent, so
    //! the default here is false and replypreview.qss turns it on only for the bubble-embedded
    //! instance. Never shown at all while the title itself is hidden, see contentWidthHint().
    Q_PROPERTY(bool quoteIconVisible READ isQuoteIconVisible WRITE setQuoteIconVisible)

    public:

        using WidgetQFrame::WidgetQFrame;

        /**
         * @brief Set the data to show.
         *
         * Trimming (trimReplyText(), a DATA operation deciding what is stored/sent) is applied
         * here, before the result reaches the text label; the label's own elision (a PAINT
         * operation reacting to the label's current width) then shortens THAT further as
         * needed -- see trimReplyText()'s own doc comment for why no ellipsis is appended at
         * this layer, only by the label. data().isQuote() picks WHICH limit applies --
         * quoteTrimLength() for a quote, textTrimLength() for a plain reply -- kept as two
         * separate properties since a quote is already the user's own deliberately hand-picked
         * fragment, not necessarily needing the same limit as a full original message.
         */
        virtual void setData(ReplyPreviewData data) =0;
        virtual const ReplyPreviewData& data() const =0;

        //! Equivalent to setData(ReplyPreviewData{}).
        virtual void clear() =0;

        //! Default tr("Reply to %1, %2") -- %1 is data().senderTitle(), %2 is
        //! data().dateTime() formatted per setDateTimeFormat(). %2 is optional: a format with
        //! %1 alone omits the datetime entirely (only %1 is substituted); a format with neither
        //! marker is rendered literally.
        virtual void setTitleFormat(const QString& format) =0;
        virtual QString titleFormat() const =0;

        //! Shown instead of data().text()/the type icon whenever data().isDeleted() or
        //! data().kind()==ReplyMessageKind::Deleted. Default tr("Deleted message").
        virtual void setDeletedText(const QString& text) =0;
        virtual QString deletedText() const =0;

        virtual void setDateTimeFormat(const QString& format) =0;
        virtual QString dateTimeFormat() const =0;

        /**
         * @brief Get the natural content width for a bubble-width negotiation pass.
         * @param forMaxWidth Upper bound offered by the negotiation pass.
         * @return 0 if maxWidthHint() is 0 (this block never widens the bubble); otherwise the
         *  natural width of the accent bar + icon slot + longest of the title/text lines,
         *  capped by both maxWidthHint() and @a forMaxWidth.
         */
        virtual int contentWidthHint(int forMaxWidth) const =0;

        //! Cap applied to the text column; the title/text labels re-elide themselves from
        //! their own resizeEvent() once relaid out at this width, no further call needed here.
        virtual void setContentMaxWidth(int width) =0;

        void setTextTrimLength(int length)
        {
            m_textTrimLength=length;
            updateTextTrimLength();
        }

        int textTrimLength() const noexcept
        {
            return m_textTrimLength;
        }

        void setQuoteTrimLength(int length)
        {
            m_quoteTrimLength=length;
            updateQuoteTrimLength();
        }

        int quoteTrimLength() const noexcept
        {
            return m_quoteTrimLength;
        }

        void setMaxWidthHint(int width) noexcept
        {
            m_maxWidthHint=width;
        }

        int maxWidthHint() const noexcept
        {
            return m_maxWidthHint;
        }

        void setAccentBarVisible(bool enable)
        {
            m_accentBarVisible=enable;
            updateAccentBarVisible();
        }

        bool isAccentBarVisible() const noexcept
        {
            return m_accentBarVisible;
        }

        void setQuoteIconVisible(bool enable)
        {
            m_quoteIconVisible=enable;
            updateQuoteIconVisible();
        }

        bool isQuoteIconVisible() const noexcept
        {
            return m_quoteIconVisible;
        }

    signals:

        void clicked();

    protected:

        //! Re-apply textTrimLength() to the currently held data() -- no-op in the base class.
        //! Called after setTextTrimLength(), so a QSS-driven qproperty write takes effect on
        //! whatever is already shown, not just the next setData().
        virtual void updateTextTrimLength() {}

        //! Re-apply quoteTrimLength() to the currently held data() -- no-op in the base class.
        //! Called after setQuoteTrimLength(), same rationale as updateTextTrimLength().
        virtual void updateQuoteTrimLength() {}

        //! Called after setAccentBarVisible() -- no-op in the base class.
        virtual void updateAccentBarVisible() {}

        //! Called after setQuoteIconVisible() -- no-op in the base class.
        virtual void updateQuoteIconVisible() {}

    private:

        int m_textTrimLength=DefaultReplyTextTrimLength;
        int m_quoteTrimLength=DefaultReplyQuoteTrimLength;
        int m_maxWidthHint=DefaultReplyMaxWidthHint;
        bool m_accentBarVisible=true;
        bool m_quoteIconVisible=false;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACTREPLYPREVIEW_HPP
