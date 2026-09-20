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

/** @file uise/desktop/src/audioplayerdialog.cpp
*
*  Defines AudioPlayerDialog and FloatingAudioPlayerDialog.
*
*/

/****************************************************************************/

#include <QEvent>
#include <QPointer>

#include <uise/desktop/audioplayerdialog.hpp>
#include <uise/desktop/audioplayer.hpp>

#include <uise/desktop/ipp/dialog.ipp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

//--------------------------------------------------------------------------

class AudioPlayerDialog_p
{
    public:

        QPointer<AbstractAudioPlayer> player;
};

//--------------------------------------------------------------------------

AudioPlayerDialog::AudioPlayerDialog(QWidget* parent)
    : Base(parent),
      pimpl(std::make_unique<AudioPlayerDialog_p>())
{
}

//--------------------------------------------------------------------------

AudioPlayerDialog::~AudioPlayerDialog()
{
}

//--------------------------------------------------------------------------

AbstractAudioPlayer* AudioPlayerDialog::player() const
{
    return pimpl->player.data();
}

//--------------------------------------------------------------------------

void AudioPlayerDialog::construct()
{
    setObjectName("audioPlayerDialog");

    pimpl->player=makeWidget<AbstractAudioPlayer,AudioPlayer>(this);
    pimpl->player->setMode(AbstractAudioPlayer::Mode::Dialog);

    // Closing the dialog is what stops the playback here, there is no Stop button. It is sent
    // before the dialog goes so a host that mirrors it into an engine still has one.
    connect(this,&AbstractDialog::aboutToClose,this,
        [this]()
        {
            if (!pimpl->player.isNull())
            {
                emit pimpl->player->stopRequested();
            }
        }
    );

    setWidget(pimpl->player->qWidget());
    setTitle(tr("Audio player"));

    // No bottom button row: Dialog<>'s constructor installs a Close button by default, and the
    // title bar's own close button already does that.
    setButtons({});
}

//--------------------------------------------------------------------------

void AudioPlayerDialog::changeEvent(QEvent* event)
{
    Base::changeEvent(event);
    if (event->type()==QEvent::LanguageChange)
    {
        setTitle(tr("Audio player"));
    }
}

//--------------------------------------------------------------------------

FloatingAudioPlayerDialog::FloatingAudioPlayerDialog(QWidget* parent)
    : FloatingAudioPlayerDialogType(parent)
{
    // Re-typed from the base's Qt::Dialog, see the class doc comment. FramelessWindowHint stays:
    // the frame paints its own chrome and its content supplies the title bar it is dragged by.
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
}

//--------------------------------------------------------------------------

template class UISE_DESKTOP_EXPORT Dialog<AbstractAudioPlayerDialog>;

//--------------------------------------------------------------------------

}
