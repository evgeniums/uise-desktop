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

/** @file uise/desktop/audioplaybackengine.hpp
*
*  Declares AudioPlaybackEngine, the seam between the audio player widget and whatever plays the sound.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_AUDIOPLAYBACKENGINE_HPP
#define UISE_DESKTOP_AUDIOPLAYBACKENGINE_HPP

#include <QObject>
#include <QString>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief What an audio player needs from something that plays sound.
 *
 * The player widgets (AbstractAudioPlayer, ChatVoiceFileItem, ...) never play anything: they show
 * a position and send requests. An engine is the other half. It can be Qt's media player
 * (QtAudioPlaybackEngine, for ordinary audio files) or anything else that can honour these calls --
 * the voice message engine over hatn `media` is supplied by hatnuise, not by this library.
 *
 * Threads: every method is called on the GUI thread, and every signal must be emitted on it. An
 * engine that decodes elsewhere marshals its own notifications.
 *
 * Volume is 0..1 and playback rate is a ratio (1.0 is normal). Position and duration are in
 * milliseconds.
 */
class UISE_DESKTOP_EXPORT AudioPlaybackEngine : public QObject
{
    Q_OBJECT

    public:

        enum class State
        {
            Stopped,
            Playing,
            Paused
        };
        Q_ENUM(State)

        using QObject::QObject;

        /**
         * @brief Load a source. Does not start playing.
         * @param source A local file path or a URL; what else an engine accepts is up to it.
         */
        virtual void open(const QString& source)=0;

        virtual void play()=0;
        virtual void pause()=0;

        //! Stop and go back to the start.
        virtual void stop()=0;

        virtual void seekMs(qint64 ms)=0;

        virtual void setVolume(qreal volume)=0;
        virtual void setMuted(bool muted)=0;
        virtual void setPlaybackRate(qreal rate)=0;

        virtual State state() const=0;
        virtual qint64 positionMs() const=0;
        virtual qint64 durationMs() const=0;

    signals:

        void positionChanged(qint64 ms);
        void durationChanged(qint64 ms);
        void stateChanged(UISE_DESKTOP_NAMESPACE::AudioPlaybackEngine::State state);

        //! Playback failed; `message` is for a log or a tooltip, not for translation.
        void errorOccurred(const QString& message);
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_AUDIOPLAYBACKENGINE_HPP
