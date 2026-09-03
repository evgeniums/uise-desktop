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

/** @file uise/desktop/forwarddialog.hpp
*
*  Declares ForwardDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FORWARDDIALOG_HPP
#define UISE_DESKTOP_FORWARDDIALOG_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/dialog.hpp>
#include <uise/desktop/modaldialog.hpp>
#include <uise/desktop/abstractforwarddialog.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class ForwardDialog_p;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4661)
#endif

/**
 * @brief Default AbstractForwardDialog implementation.
 *
 * Content, top to bottom: a ScrollArea holding the bubble installed via setMessage() (hidden in
 * favour of a plain count label while messageCount()>1), a "Hide sender name" checkbox, an
 * optional comment label, and a ButtonsList of actions -- all inside Dialog<>::setWidget(), so
 * they sit above Dialog<>'s own Cancel/Save button row (Save relabelled "Quote selected" while
 * message()'s body has a text selection, see updateSaveButton()) -- structured exactly like
 * ReplyDialog.
 */
class UISE_DESKTOP_EXPORT ForwardDialog : public Dialog<AbstractForwardDialog>
{
    Q_OBJECT

    public:

        using Base=Dialog<AbstractForwardDialog>;

        explicit ForwardDialog(QWidget* parent=nullptr);

        ~ForwardDialog();
        ForwardDialog(const ForwardDialog&)=delete;
        ForwardDialog(ForwardDialog&&)=delete;
        ForwardDialog& operator=(const ForwardDialog&)=delete;
        ForwardDialog& operator=(ForwardDialog&&)=delete;

        void setMessage(AbstractChatMessage* message) override;
        AbstractChatMessage* message() const override;

        void setMessageCount(int count) override;
        int messageCount() const override;

        void setActions(std::vector<ForwardDialogActionConfig> actions) override;

        void setHideSenderName(bool enable) override;
        bool isHideSenderName() const override;

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
        void updateMode();

        std::unique_ptr<ForwardDialog_p> pimpl;
};

using ModalForwardDialogType=ModalDialog<AbstractForwardDialog,ForwardDialog,-1,80,-1,80>;

class UISE_DESKTOP_EXPORT ModalForwardDialog : public ModalForwardDialogType
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor. Same parameters/defaults as ModalForwardDialogType, plus popup
         *  auto-height enabled unconditionally -- see ModalReplyDialog's identical rationale.
         */
        explicit ModalForwardDialog(
                QWidget* parent=nullptr,
                int defaultMaxWidthPercent=80,
                int defaultPopupMaxWidth=-1,
                int defaultMaxHeightPercent=80,
                int defaultPopupMaxHeight=-1
            ) : ModalForwardDialogType(parent,defaultMaxWidthPercent,defaultPopupMaxWidth,defaultMaxHeightPercent,defaultPopupMaxHeight)
        {
            setPopupAutoHeight(true);

            // ModalDialog<>'s own ctor calls setShortcutEnabled(false); re-enable it since
            // Escape already tears down through the same ModalPopup::close() ->
            // AbstractDialog::closeDialog() chain as Cancel -- see ModalReplyDialog's identical
            // rationale.
            setShortcutEnabled(true);
        }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}

#endif // UISE_DESKTOP_FORWARDDIALOG_HPP
