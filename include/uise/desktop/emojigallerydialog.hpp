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

/** @file uise/desktop/emojigallerydialog.hpp
*
*  Declares EmojiGalleryDialog and FloatingEmojiGalleryDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_EMOJIGALLERYDIALOG_HPP
#define UISE_DESKTOP_EMOJIGALLERYDIALOG_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/dialog.hpp>
#include <uise/desktop/floatingdialog.hpp>
#include <uise/desktop/abstractemojigallerydialog.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class EmojiGalleryDialog_p;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4661)
#endif

/**
 * @brief Default AbstractEmojiGalleryDialog implementation: a ChatReactionGallery wrapped in
 *  ordinary dialog chrome.
 *
 * Built in construct() rather than the constructor (the HyperlinkDialog/PasswordDialog pattern)
 * so a factory-substituted implementation participates the same way.
 *
 * Carries no dialog BUTTONS at all -- setButtons({}) in construct() drops the Close row
 * Dialog<>'s own constructor installs by default. A picker is dismissed by its title-bar X, by
 * Escape, or by the editor's emoji button being unchecked; a bottom button row would just eat
 * vertical space and add a second, redundant way to do the same thing.
 */
class UISE_DESKTOP_EXPORT EmojiGalleryDialog : public Dialog<AbstractEmojiGalleryDialog>
{
    Q_OBJECT

    public:

        using Base=Dialog<AbstractEmojiGalleryDialog>;

        explicit EmojiGalleryDialog(QWidget* parent=nullptr);

        ~EmojiGalleryDialog();
        EmojiGalleryDialog(const EmojiGalleryDialog&)=delete;
        EmojiGalleryDialog(EmojiGalleryDialog&&)=delete;
        EmojiGalleryDialog& operator=(const EmojiGalleryDialog&)=delete;
        EmojiGalleryDialog& operator=(EmojiGalleryDialog&&)=delete;

        void setPack(std::shared_ptr<AbstractReactionIconPack> pack) override;
        std::shared_ptr<AbstractReactionIconPack> pack() const override;

        void resetSearch() override;

        void construct() override;

        /**
         * @brief Clears the search and rebuilds the grid immediately before the host measures
         *  this dialog.
         *
         * Both halves matter. Clearing means a picker reopened later never comes up still
         * filtered by the previous session's search. Rebuilding HERE rather than at setPack()
         * time is what makes the grid's height clamp correct on the very first open: this hook
         * runs after FloatingDialogFrame has polished the whole subtree, so the cells finally
         * carry their QSS icon size and padding, and ChatReactionGallery measures the clamp off a
         * real one. Called before adjustSize(), so the frame is sized once, correctly.
         */
        void prepareToShow() override;

        //! The grid has no meaningful size beyond its natural one, so a host must never offer
        //! mouse resize -- the same call CalendarDialog makes, for the same reason.
        bool isResizable() const override
        {
            return false;
        }

    protected:

        //! Re-applies this dialog's own emoji wording over ChatReactionGallery's reaction
        //! defaults -- see ChatReactionGallery::setSearchPlaceholderText().
        void changeEvent(QEvent* event) override;

    private:

        std::unique_ptr<EmojiGalleryDialog_p> pimpl;
};

// Max width/height of 0 on purpose: FloatingDialog::openDialog() applies its caps only when they
// are positive, so this leaves the gallery's own natural size -- which ChatReactionGallery now
// clamps itself, via galleryVisibleRows -- in charge, instead of imposing a second, competing
// limit that could cut the grid off mid-row.
using FloatingEmojiGalleryDialogType=FloatingDialog<AbstractEmojiGalleryDialog,EmojiGalleryDialog,0,0>;

/**
 * @brief Floating, draggable host for EmojiGalleryDialog.
 *
 * A named class rather than a bare typedef so it can be forward-declared (MessageEditor holds
 * one) without dragging this whole header into messageeditor.hpp.
 *
 * Overrides FloatingDialogFrame's window TYPE, from Qt::Dialog to Qt::Tool -- the one thing about
 * this host that is not the base class's default, and the reason is what "stay above the composer"
 * costs on each platform:
 *
 * A Qt::Dialog is an ordinary top-level window. Nothing in the window system keeps one above the
 * window that spawned it, so FloatingDialogFrame compensates by raise()ing itself whenever its host
 * is activated -- which is a race against the OS, since the click that activates the host has
 * already ordered the host in front. In practice the picker sinks behind the composer's window.
 *
 * A Qt::Tool is a utility/palette window: the platform puts it in a level ABOVE the application's
 * ordinary windows and keeps it there, with no raise() needed and nothing to race. That is exactly
 * what an emoji picker is -- a palette the user keeps open beside the thing they are typing into.
 *
 * The trade-off, and why FloatingDialogFrame does not default to this: a Qt::Tool window hides when
 * the APPLICATION is deactivated (macOS). For a dialog meant to stay put across app switches that
 * would be wrong; for a picker it is the wanted behaviour and matches this feature's own rule that
 * the gallery closes when the chat page stops being the thing in front of the user.
 */
class UISE_DESKTOP_EXPORT FloatingEmojiGalleryDialog : public FloatingEmojiGalleryDialogType
{
    Q_OBJECT

    public:

        explicit FloatingEmojiGalleryDialog(QWidget* parent=nullptr);
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}

#endif // UISE_DESKTOP_EMOJIGALLERYDIALOG_HPP
