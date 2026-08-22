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

/** @file uise/desktop/abstractforwarddialog.hpp
*
*  Declares AbstractForwardDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTFORWARDDIALOG_HPP
#define UISE_DESKTOP_ABSTRACTFORWARDDIALOG_HPP

#include <vector>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractdialog.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class AbstractChatMessage;

enum class ForwardDialogAction : int
{
    ChangeRecipient=1,
    ShowInChat=2,
    DoNotForward=3
};

/**
 * @brief One row of the dialog's action list -- see AbstractForwardDialog::setActions().
 *
 * Mirrors AbstractDialog::ButtonConfig, including the implicit constructor from
 * ForwardDialogAction, so a caller can write plain
 * `setActions({ForwardDialogAction::ChangeRecipient, ForwardDialogAction::ShowInChat})`.
 */
struct UISE_DESKTOP_EXPORT ForwardDialogActionConfig
{
    int id=0;
    QString text;
    std::shared_ptr<SvgIcon> icon;

    ForwardDialogActionConfig(int id, QString text, std::shared_ptr<SvgIcon> icon={})
        : id(id),
          text(std::move(text)),
          icon(std::move(icon))
    {}

    ForwardDialogActionConfig(ForwardDialogAction action, QWidget* parent=nullptr);
};

/**
 * @brief Interface of the full "configure forward" modal -- task-message-forwarding.md's
 *  "Full preview of reply operation"-equivalent for forwarding.
 *
 * Single-message mode content: a scrollable copy of the message bubble being forwarded (see
 * setMessage()), a "Hide sender name" checkbox (unchecked by default, see setHideSenderName()),
 * a list of actions (see setActions()), an optional comment for text messages (see setComment()),
 * and Cancel/Save dialog buttons. Save's text/icon swap to "Quote selected" whenever message()'s
 * body has a non-empty text selection -- see AbstractChatMessageBody::selectionChanged() and
 * selectedText() -- exactly like AbstractReplyDialog.
 *
 * Multi-message mode (see setMessageCount()) shows a count instead of a bubble: no selection, no
 * Save/"Quote selected" swap, no quote-hint comment -- only the count and the hide-sender
 * checkbox.
 */
class UISE_DESKTOP_EXPORT AbstractForwardDialog : public AbstractDialog
{
    Q_OBJECT

    public:

        using AbstractDialog::AbstractDialog;

        /**
         * @brief Install the bubble to preview (single-message mode).
         * @param message A fully built AbstractChatMessage -- the HOST builds it, since this
         *  library has no knowledge of app-specific message bodies. Ownership passes to this
         *  dialog, which reparents it into its own scroll area; a second call destroys the
         *  previous one. Ignored while messageCount()>1.
         */
        virtual void setMessage(AbstractChatMessage* message) =0;
        virtual AbstractChatMessage* message() const =0;

        /**
         * @brief Switch between single-message and multi-message mode.
         * @param count 0 or 1 shows message() as usual; >1 hides it and shows a plain count
         *  instead, disabling selection/quoting -- task-message-forwarding.md: "If there are
         *  multiple messages selected for forwarding ... those messages can not be edited before
         *  sending."
         */
        virtual void setMessageCount(int count) =0;
        virtual int messageCount() const =0;

        //! Renders as a vertical list of icon+text rows (see ButtonsList). Default policy if
        //! never called: {ChangeRecipient, ShowInChat}, both built via standardAction().
        virtual void setActions(std::vector<ForwardDialogActionConfig> actions) =0;

        //! Icon/text pair for a built-in ForwardDialogAction, mirroring
        //! AbstractReplyDialog::standardAction().
        static ForwardDialogActionConfig standardAction(ForwardDialogAction action, QWidget* parent=nullptr);

        /**
         * @brief Whether the forwarded message hides its original author.
         * @param enable Unchecked (false) by default. task-message-forwarding.md: "If
         *  hide-sender-name is checked then message will be sent as plain message without
         *  forward field ... If hide-sender-name is not checked then user can edit comments in
         *  text editor and those comments will be sent along with original content." Both
         *  behaviours are entirely host policy at send time -- this widget only carries the
         *  flag.
         */
        virtual void setHideSenderName(bool enable) =0;
        virtual bool isHideSenderName() const =0;

        //! Default tr("You can select a part of the text to quote only that part.") -- shown
        //! only while isCommentVisible() and messageCount()<=1.
        virtual void setComment(const QString& text) =0;
        virtual QString comment() const =0;

        //! Default policy: visible whenever messageCount()<=1 and message()'s body is a text
        //! body (see AbstractChatMessageText) -- overridable either way.
        virtual void setCommentVisible(bool enable) =0;
        virtual bool isCommentVisible() const =0;

        //! Equivalent to message()->selectedText() (empty if no message() is set or
        //! messageCount()>1).
        virtual QString selectedText() const =0;

        //! Character-count cap applied to selectedText() before it is emitted by
        //! saveRequested() -- see trimReplyText() and AbstractReplyDialog::quoteTrimLength()'s
        //! own doc comment for why a quote gets a limit distinct from a plain forward's trim
        //! length. Default DefaultReplyQuoteTrimLength.
        virtual void setQuoteTrimLength(int length) =0;
        virtual int quoteTrimLength() const =0;

    signals:

        //! One of the action rows set via setActions() was picked.
        void actionTriggered(int id);

        //! setHideSenderName()'s state changed, from either the checkbox or the setter.
        void hideSenderNameChanged(bool enable);

        /**
         * @brief The Save (or "Quote selected") button was pressed.
         * @param quotedText Empty for a plain Save, or in multi-message mode; selectedText()
         *  already passed through trimReplyText() at quoteTrimLength(), for "Quote selected".
         * @param hideSenderName isHideSenderName() at the moment Save was pressed.
         */
        void saveRequested(const QString& quotedText, bool hideSenderName);
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACTFORWARDDIALOG_HPP
