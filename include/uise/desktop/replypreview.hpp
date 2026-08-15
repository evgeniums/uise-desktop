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

/** @file uise/desktop/replypreview.hpp
*
*  Declares ReplyPreview.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_REPLYPREVIEW_HPP
#define UISE_DESKTOP_REPLYPREVIEW_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractreplypreview.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class ReplyPreview_p;

/**
 * @brief Default AbstractReplyPreview implementation.
 *
 * Layout, left to right: a fixed-width accent bar, a fixed-size icon slot (shown ONLY for a
 * reply to an image message, holding its thumbnail -- see updateIconSlot()/isIconSlotVisible()),
 * and a text column holding a title row (the elided title line plus an optional quote glyph,
 * see isQuoteIconVisible()) and an elided text line. The whole block is clickable (see
 * clicked()). Decoration (accent bar/background colours, tint on selection) is entirely
 * QSS-driven, see replypreview.qss.
 */
class UISE_DESKTOP_EXPORT ReplyPreview : public AbstractReplyPreview
{
    Q_OBJECT

    public:

        explicit ReplyPreview(QWidget* parent=nullptr);

        ~ReplyPreview();
        ReplyPreview(const ReplyPreview&)=delete;
        ReplyPreview(ReplyPreview&&)=delete;
        ReplyPreview& operator=(const ReplyPreview&)=delete;
        ReplyPreview& operator=(ReplyPreview&&)=delete;

        void setData(ReplyPreviewData data) override;
        const ReplyPreviewData& data() const override;
        void clear() override;

        void setTitleFormat(const QString& format) override;
        QString titleFormat() const override;

        void setDeletedText(const QString& text) override;
        QString deletedText() const override;

        void setDateTimeFormat(const QString& format) override;
        QString dateTimeFormat() const override;

        int contentWidthHint(int forMaxWidth) const override;
        void setContentMaxWidth(int width) override;

    protected:

        void updateTextTrimLength() override;
        void updateQuoteTrimLength() override;
        void updateAccentBarVisible() override;
        void updateQuoteIconVisible() override;

        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;

    private:

        void refresh();
        void updateIconSlot();

        //! True only for a reply to an image message with a real thumbnail available, and
        //! never while deleted -- the single place this policy is defined, consulted by both
        //! updateIconSlot() and the width-hint math (contentWidthHint()/setContentMaxWidth()).
        bool isIconSlotVisible() const;

        //! True while the title row itself is shown -- see refresh()'s own deleted/empty-title
        //! handling. Factored out so isQuoteIconShown() and refresh() share one definition.
        bool isTitleShown() const;

        //! isTitleShown() && isQuoteIconVisible() -- the quote glyph never shows without a
        //! title row to sit in, regardless of the property.
        bool isQuoteIconShown() const;

        std::unique_ptr<ReplyPreview_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_REPLYPREVIEW_HPP
