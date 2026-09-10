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

/** @file uise/desktop/abstracthyperlinkdialog.hpp
*
*  Declares AbstractHyperlinkDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTHYPERLINKDIALOG_HPP
#define UISE_DESKTOP_ABSTRACTHYPERLINKDIALOG_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractdialog.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

/**
 * @brief Interface of the "Insert link" / "Edit link" modal (task-message-formatting-plan.md,
 *  Stage 5b), used by MessageEditor to collect a URL and an optional display title.
 *
 * Content: a URL field and a title field, plus OK/Cancel dialog buttons. setUrl()/setLinkTitle()
 * pre-fill the two fields -- MessageEditor calls both with the link's current values when the
 * caret is already inside a link (the "edit" case), or leaves them empty for a fresh insert.
 * linkAccepted() is emitted on OK once the URL passes validation; like ReplyDialog's own Save
 * button, accepting never closes the dialog itself -- the host does that from its
 * linkAccepted() handler.
 */
class UISE_DESKTOP_EXPORT AbstractHyperlinkDialog : public AbstractDialog
{
    Q_OBJECT

    public:

        using AbstractDialog::AbstractDialog;

        virtual void setUrl(const QString& url) =0;
        virtual QString url() const =0;

        virtual void setLinkTitle(const QString& title) =0;
        virtual QString linkTitle() const =0;

        //! Shown under the URL field, e.g. when OK is pressed with an empty or invalid URL.
        //! Cleared automatically the next time OK is pressed with a valid URL.
        virtual void setError(const QString& message) =0;

        //! Clears both fields and any error -- called on closeRequested() by the default
        //! implementation, so a dialog reused for a second link never shows the previous one's
        //! values.
        virtual void reset() =0;

    signals:

        /**
         * @brief OK was pressed with a URL that passed validation.
         * @param url Trimmed, non-empty.
         * @param title Trimmed; equal to url if the title field was left empty (MessageEditor
         *  treats an empty title as "display the URL itself", same as a plain autolinked URL).
         */
        void linkAccepted(const QString& url, const QString& title);
};

}

#endif // UISE_DESKTOP_ABSTRACTHYPERLINKDIALOG_HPP
