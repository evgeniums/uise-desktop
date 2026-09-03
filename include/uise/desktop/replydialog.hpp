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

/** @file uise/desktop/replydialog.hpp
*
*  Declares ReplyDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_REPLYDIALOG_HPP
#define UISE_DESKTOP_REPLYDIALOG_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/dialog.hpp>
#include <uise/desktop/modaldialog.hpp>
#include <uise/desktop/abstractreplydialog.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class ReplyDialog_p;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4661)
#endif

/**
 * @brief Default AbstractReplyDialog implementation.
 *
 * Content, top to bottom: a ScrollArea holding the bubble installed via setMessage(), an
 * optional comment label, and a ButtonsList of actions -- all inside Dialog<>::setWidget(), so
 * they sit above Dialog<>'s own Cancel/Save button row (Save relabelled "Quote selected" while
 * message()'s body has a text selection, see updateSaveButton()).
 */
class UISE_DESKTOP_EXPORT ReplyDialog : public Dialog<AbstractReplyDialog>
{
    Q_OBJECT

    public:

        using Base=Dialog<AbstractReplyDialog>;

        explicit ReplyDialog(QWidget* parent=nullptr);

        ~ReplyDialog();
        ReplyDialog(const ReplyDialog&)=delete;
        ReplyDialog(ReplyDialog&&)=delete;
        ReplyDialog& operator=(const ReplyDialog&)=delete;
        ReplyDialog& operator=(ReplyDialog&&)=delete;

        void setMessage(AbstractChatMessage* message) override;
        AbstractChatMessage* message() const override;

        void setActions(std::vector<ReplyDialogActionConfig> actions) override;

        void setComment(const QString& text) override;
        QString comment() const override;

        void setCommentVisible(bool enable) override;
        bool isCommentVisible() const override;

        QString selectedText() const override;

        void setQuoteTrimLength(int length) override;
        int quoteTrimLength() const override;

        void prepareToShow() override;

    private:

        void updateSaveButton();
        void updateCommentVisibility();
        void updateMessageAreaHeight();

        std::unique_ptr<ReplyDialog_p> pimpl;
};

using ModalReplyDialogType=ModalDialog<AbstractReplyDialog,ReplyDialog,-1,80,-1,80>;

class UISE_DESKTOP_EXPORT ModalReplyDialog : public ModalReplyDialogType
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor. Same parameters/defaults as ModalReplyDialogType, plus popup
         *  auto-height enabled unconditionally.
         */
        explicit ModalReplyDialog(
                QWidget* parent=nullptr,
                int defaultMaxWidthPercent=80,
                int defaultPopupMaxWidth=-1,
                int defaultMaxHeightPercent=80,
                int defaultPopupMaxHeight=-1
            ) : ModalReplyDialogType(parent,defaultMaxWidthPercent,defaultPopupMaxWidth,defaultMaxHeightPercent,defaultPopupMaxHeight)
        {
            // The message bubble's height varies a lot by content (a one-line text reply vs. a
            // multi-image album) -- auto-height reflows the popup to the dialog's real
            // sizeHint()/heightForWidth() (see FrameWithModalPopup::setPopupAutoHeight()'s own
            // doc comment) capped at maxHeightPercent() of the host frame, instead of a fixed
            // percentage that would either clip a tall bubble or leave a short one floating in
            // empty space. Same pairing as ModalFileUploadDialog's identical constructor.
            setPopupAutoHeight(true);

            // ModalDialog<>'s own ctor calls setShortcutEnabled(false); re-enable it since
            // Escape already tears down through the same ModalPopup::close() ->
            // AbstractDialog::closeDialog() chain as Cancel -- see ModalFileUploadDialog's
            // identical rationale.
            setShortcutEnabled(true);
        }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}

#endif // UISE_DESKTOP_REPLYDIALOG_HPP
