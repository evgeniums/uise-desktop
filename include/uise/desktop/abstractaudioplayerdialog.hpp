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

/** @file uise/desktop/abstractaudioplayerdialog.hpp
*
*  Declares AbstractAudioPlayerDialog, the interface of the audio player in a dialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTAUDIOPLAYERDIALOG_HPP
#define UISE_DESKTOP_ABSTRACTAUDIOPLAYERDIALOG_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractdialog.hpp>
#include <uise/desktop/abstractaudioplayer.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

/**
 * @brief The audio player presented as a dialog, for hosting in a FloatingDialog.
 *
 * Only the dialog chrome lives here; the player itself is an AbstractAudioPlayer that the host
 * reaches through player() and drives exactly as it would an embedded one. The player runs in
 * AbstractAudioPlayer::Mode::Dialog, so it has no Stop button: closing the dialog is what stops
 * it, and closing sends AbstractAudioPlayer::stopRequested() first.
 */
class UISE_DESKTOP_EXPORT AbstractAudioPlayerDialog : public AbstractDialog
{
    Q_OBJECT

    public:

        using AbstractDialog::AbstractDialog;

        //! The player inside the dialog. Null until the dialog has been constructed.
        virtual AbstractAudioPlayer* player() const =0;
};

}

#endif // UISE_DESKTOP_ABSTRACTAUDIOPLAYERDIALOG_HPP
