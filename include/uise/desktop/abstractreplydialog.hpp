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

/** @file uise/desktop/abstractreplydialog.hpp
*
*  Declares AbstractReplyDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTREPLYDIALOG_HPP
#define UISE_DESKTOP_ABSTRACTREPLYDIALOG_HPP

#include <vector>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractdialog.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class AbstractChatMessage;

enum class ReplyDialogAction : int
{
    ShowInChat=1,
    DoNotReply=2
};

/**
 * @brief One row of the dialog's action list -- see AbstractReplyDialog::setActions().
 *
 * Mirrors AbstractDialog::ButtonConfig, including the implicit constructor from
 * ReplyDialogAction, so a caller can write plain
 * `setActions({ReplyDialogAction::ShowInChat, ReplyDialogAction::DoNotReply})`.
 */
struct UISE_DESKTOP_EXPORT ReplyDialogActionConfig
{
    int id=0;
    QString text;
    std::shared_ptr<SvgIcon> icon;

    ReplyDialogActionConfig(int id, QString text, std::shared_ptr<SvgIcon> icon={})
        : id(id),
          text(std::move(text)),
          icon(std::move(icon))
    {}

    ReplyDialogActionConfig(ReplyDialogAction action, QWidget* parent=nullptr);
};

/**
 * @brief Interface of the full-preview "Reply to message" modal.
 *
 * Content: a scrollable copy of the message bubble being replied to (see setMessage()), a list
 * of actions (see setActions()), an optional comment for text messages (see setComment()), and
 * Cancel/Save dialog buttons. Save's text/icon swap to "Quote selected" whenever message()'s
 * body has a non-empty text selection -- see AbstractChatMessageBody::selectionChanged() and
 * selectedText().
 */
class UISE_DESKTOP_EXPORT AbstractReplyDialog : public AbstractDialog
{
    Q_OBJECT

    public:

        using AbstractDialog::AbstractDialog;

        /**
         * @brief Install the bubble to preview.
         * @param message A fully built AbstractChatMessage -- the HOST builds it, since this
         *  library has no knowledge of app-specific message bodies. Ownership passes to this
         *  dialog, which reparents it into its own scroll area; a second call destroys the
         *  previous one.
         */
        virtual void setMessage(AbstractChatMessage* message) =0;
        virtual AbstractChatMessage* message() const =0;

        //! Renders as a vertical list of icon+text rows (see ButtonsList). Default policy if
        //! never called: {ShowInChat, DoNotReply}, both built via standardAction().
        virtual void setActions(std::vector<ReplyDialogActionConfig> actions) =0;

        //! Icon/text pair for a built-in ReplyDialogAction, mirroring
        //! AbstractDialog::standardButton().
        static ReplyDialogActionConfig standardAction(ReplyDialogAction action, QWidget* parent=nullptr);

        //! Default tr("You can select a part of the text to quote only that part.") -- shown
        //! only while isCommentVisible().
        virtual void setComment(const QString& text) =0;
        virtual QString comment() const =0;

        //! Default policy: visible whenever message()'s body is a text body (see
        //! AbstractChatMessageText) -- overridable either way.
        virtual void setCommentVisible(bool enable) =0;
        virtual bool isCommentVisible() const =0;

        //! Equivalent to message()->selectedText() (empty if no message() is set).
        virtual QString selectedText() const =0;

        //! Character-count cap applied to selectedText() before it is emitted by
        //! saveRequested() -- see trimReplyText() and
        //! AbstractReplyPreview::quoteTrimLength()'s own doc comment for why a quote gets a
        //! limit distinct from a plain reply's trim length. Default
        //! DefaultReplyQuoteTrimLength.
        virtual void setQuoteTrimLength(int length) =0;
        virtual int quoteTrimLength() const =0;

    signals:

        //! One of the action rows set via setActions() was picked.
        void actionTriggered(int id);

        /**
         * @brief The Save (or "Quote selected") button was pressed.
         * @param quotedText Empty for a plain Save; selectedText() already passed through
         *  trimReplyText() at quoteTrimLength(), for "Quote selected".
         */
        void saveRequested(const QString& quotedText);
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACTREPLYDIALOG_HPP
