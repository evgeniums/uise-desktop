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

/** @file uise/desktop/qtaudioplaybackengine.hpp
*
*  Declares QtAudioPlaybackEngine, an AudioPlaybackEngine over Qt Multimedia.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_QTAUDIOPLAYBACKENGINE_HPP
#define UISE_DESKTOP_QTAUDIOPLAYBACKENGINE_HPP

#include <memory>

#include <QIODevice>
#include <QString>
#include <QUrl>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/audioplaybackengine.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class QtAudioPlaybackEngine_p;

/**
 * @brief Plays audio files with QMediaPlayer and QAudioOutput.
 *
 * This is the engine for ordinary audio attachments (mp3, m4a, wav, flac, ogg, ...), whatever
 * formats the platform's Qt Multimedia backend can decode. It is compiled only when
 * UISE_DESKTOP_MULTIMEDIA is on. Voice messages do not go through it.
 *
 * open() takes a local path or a URL; openDevice() plays from a QIODevice instead, which is how a
 * file that is only available decrypted-on-read would be fed to the player. The device must be
 * open for reading, seekable, and must outlive the playback.
 */
class UISE_DESKTOP_EXPORT QtAudioPlaybackEngine : public AudioPlaybackEngine
{
    Q_OBJECT

    public:

        explicit QtAudioPlaybackEngine(QObject* parent=nullptr);
        ~QtAudioPlaybackEngine() override;

        QtAudioPlaybackEngine(const QtAudioPlaybackEngine&)=delete;
        QtAudioPlaybackEngine(QtAudioPlaybackEngine&&)=delete;
        QtAudioPlaybackEngine& operator=(const QtAudioPlaybackEngine&)=delete;
        QtAudioPlaybackEngine& operator=(QtAudioPlaybackEngine&&)=delete;

        void open(const QString& source) override;

        /**
         * @brief Play from a device.
         * @param device Readable and seekable; not owned, must outlive the playback.
         * @param hint Name or URL of the media, which some backends use to pick a decoder.
         */
        void openDevice(QIODevice* device, const QUrl& hint=QUrl());

        void play() override;
        void pause() override;
        void stop() override;
        void seekMs(qint64 ms) override;

        void setVolume(qreal volume) override;
        void setMuted(bool muted) override;
        void setPlaybackRate(qreal rate) override;

        State state() const override;
        qint64 positionMs() const override;
        qint64 durationMs() const override;

        /**
         * @brief Choose the output device by the id that QAudioDevice::id() reports.
         *
         * An empty id, or one that is not among QMediaDevices::audioOutputs() any more, means the
         * default output. Takes effect at once via QAudioOutput::setDevice().
         */
        void setOutputDevice(const QByteArray& id) override;

    private:

        std::unique_ptr<QtAudioPlaybackEngine_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_QTAUDIOPLAYBACKENGINE_HPP
