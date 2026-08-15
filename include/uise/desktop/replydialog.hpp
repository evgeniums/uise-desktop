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

UISE_DESKTOP_NAMESPACE_BEGIN

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

        std::unique_ptr<ReplyDialog_p> pimpl;
};

using ModalReplyDialogType=ModalDialog<AbstractReplyDialog,ReplyDialog,-1,80,-1,80>;

//! Convenience alias matching the naming of ModalFileUploadDialog/ModalPasswordDialog -- a
//! plain instantiation of ModalReplyDialogType with no behaviour of its own.
class UISE_DESKTOP_EXPORT ModalReplyDialog : public ModalReplyDialogType
{
    Q_OBJECT

    public:

        using ModalReplyDialogType::ModalReplyDialogType;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_REPLYDIALOG_HPP
