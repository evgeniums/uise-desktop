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

/** @file uise/desktop/abstractemojigallerydialog.hpp
*
*  Declares AbstractEmojiGalleryDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTEMOJIGALLERYDIALOG_HPP
#define UISE_DESKTOP_ABSTRACTEMOJIGALLERYDIALOG_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractdialog.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class AbstractReactionIconPack;

/**
 * @brief Interface of the emoji picker a message editor opens from its emoji button.
 *
 * Content is the very same ChatReactionGallery the reactions UI uses -- one searchable grid over
 * one icon pack. What differs is only the presentation: a reaction gallery is an anchored
 * DropdownFrame that closes the moment something is picked, whereas this is a DIALOG, hosted in a
 * FloatingDialogFrame, that the user can drag around by its title bar and that STAYS OPEN across
 * picks (inserting several emoji in a row is the normal case, not the exception).
 *
 * Being an AbstractDialog is what supplies the title bar, its close button, and -- through
 * FloatingDialogFrame::setWidget(), which adopts AbstractDialog::titleBar() as its drag handle --
 * the dragging itself, with no chrome of its own.
 */
class UISE_DESKTOP_EXPORT AbstractEmojiGalleryDialog : public AbstractDialog
{
    Q_OBJECT

    public:

        using AbstractDialog::AbstractDialog;

        /**
         * @brief Set the icon pack the gallery offers.
         * @param pack Pack to show.
         *
         * The editor picks the pack per editing mode and re-sets it when the mode changes while
         * this dialog is open: MessageEditingMode::Markdown can only insert a literal emoji
         * CHARACTER, so it is given an EmojiCodeReactionIconPack view that hides any icon with no
         * emojiCode, while MessageEditingMode::Wysiwyg inserts an image and takes the pack whole.
         */
        virtual void setPack(std::shared_ptr<AbstractReactionIconPack> pack) =0;

        virtual std::shared_ptr<AbstractReactionIconPack> pack() const =0;

        //! Clear the search box and re-show the whole pack -- called on every open, so a picker
        //! reopened later never comes up still filtered by the previous session's search.
        virtual void resetSearch() =0;

    signals:

        /**
         * @brief An emoji was clicked.
         * @param reactionId FULL reaction id, "<icon id>@<pack URI>" (ChatReactionId::make()).
         *
         * Note the difference from ChatReactionGallery::reactionPicked(), which emits only the
         * ICON ID half -- an implementation of this interface is responsible for recombining it
         * with pack()->uri(). That distinction is invisible for the default pack, whose empty URI
         * makes make() return the bare icon id, and matters for every other pack.
         *
         * The dialog does NOT close itself on a pick: see the class doc comment.
         */
        void emojiPicked(const QString& reactionId);
};

}

#endif // UISE_DESKTOP_ABSTRACTEMOJIGALLERYDIALOG_HPP
