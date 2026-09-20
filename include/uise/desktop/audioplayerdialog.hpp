/**
@copyright Evgeny Sidorov 2022-2025

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/audioplayerdialog.hpp
*
*  Declares AudioPlayerDialog and FloatingAudioPlayerDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_AUDIOPLAYERDIALOG_HPP
#define UISE_DESKTOP_AUDIOPLAYERDIALOG_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/dialog.hpp>
#include <uise/desktop/floatingdialog.hpp>
#include <uise/desktop/abstractaudioplayerdialog.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class AudioPlayerDialog_p;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4661)
#endif

/**
 * @brief Default AbstractAudioPlayerDialog: an AudioPlayer in ordinary dialog chrome, without a
 *  button row.
 *
 * Built in construct() rather than the constructor, so a factory-substituted implementation
 * participates the same way.
 */
class UISE_DESKTOP_EXPORT AudioPlayerDialog : public Dialog<AbstractAudioPlayerDialog>
{
    Q_OBJECT

    public:

        using Base=Dialog<AbstractAudioPlayerDialog>;

        explicit AudioPlayerDialog(QWidget* parent=nullptr);

        ~AudioPlayerDialog();
        AudioPlayerDialog(const AudioPlayerDialog&)=delete;
        AudioPlayerDialog(AudioPlayerDialog&&)=delete;
        AudioPlayerDialog& operator=(const AudioPlayerDialog&)=delete;
        AudioPlayerDialog& operator=(AudioPlayerDialog&&)=delete;

        AbstractAudioPlayer* player() const override;

        void construct() override;

        //! A player has a natural size only; mouse resizing would just stretch it.
        bool isResizable() const override
        {
            return false;
        }

    protected:

        void changeEvent(QEvent* event) override;

    private:

        std::unique_ptr<AudioPlayerDialog_p> pimpl;
};

// Max width/height of 0 on purpose, as for the emoji gallery: FloatingDialog applies its caps only
// when they are positive, which leaves the player's own natural size in charge.
using FloatingAudioPlayerDialogType=FloatingDialog<AbstractAudioPlayerDialog,AudioPlayerDialog,0,0>;

/**
 * @brief Floating, draggable host for AudioPlayerDialog.
 *
 * A named class rather than a bare typedef so it can be forward-declared. Its window type is
 * Qt::Tool, as the emoji gallery's is: a player is a palette the user keeps beside the chat, and a
 * Qt::Tool window is kept above the application's ordinary windows by the platform, with no
 * raise() needed. The same trade-off applies -- on macOS it hides while the application is not
 * active.
 */
class UISE_DESKTOP_EXPORT FloatingAudioPlayerDialog : public FloatingAudioPlayerDialogType
{
    Q_OBJECT

    public:

        explicit FloatingAudioPlayerDialog(QWidget* parent=nullptr);
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}

#endif // UISE_DESKTOP_AUDIOPLAYERDIALOG_HPP
