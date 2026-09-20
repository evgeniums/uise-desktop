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

/** @file uise/desktop/abstractaudioplayer.hpp
*
*  Declares AbstractAudioPlayer, the interface of the generic audio player.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTAUDIOPLAYER_HPP
#define UISE_DESKTOP_ABSTRACTAUDIOPLAYER_HPP

#include <algorithm>
#include <utility>

#include <QByteArray>
#include <QPointer>
#include <QString>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/widget.hpp>
#include <uise/desktop/waveformbar.hpp>
#include <uise/desktop/audioplaybackengine.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

/**
 * @brief Interface of the generic audio player: a title, transport buttons, volume and speed
 *        controls and a seekable progress bar.
 *
 * One player serves both a voice message (title is the sender and time, progress is a Bars
 * waveform) and an ordinary audio file (title is the file name, progress is a Line). It can be
 * embedded as a panel or shown in a floating dialog, see Mode.
 *
 * BACKEND-FREE. The player does not play anything. The host pushes state in with the setters
 * below and hears about the user's input through the signals -- or hands over an
 * AudioPlaybackEngine with setEngine(), which does that wiring both ways.
 *
 * The setters are for the host: they update the display and never emit the matching signal, so a
 * host that mirrors its own state into the player does not get its own change echoed back.
 * The signals are for user input only.
 *
 * Like AbstractImageViewer this is a WidgetController: the actual widget is made by
 * doCreateActualWidget() of the concrete class, and reached through qWidget().
 */
class UISE_DESKTOP_EXPORT AbstractAudioPlayer : public WidgetController
{
    Q_OBJECT

    public:

        //! How the player is presented. Only decides whether a Stop button is shown: a player
        //! in a dialog is stopped by closing the dialog.
        enum class Mode
        {
            Panel,
            Dialog
        };
        Q_ENUM(Mode)

        explicit AbstractAudioPlayer(QObject* parent=nullptr)
            : WidgetController(parent)
        {}

        ~AbstractAudioPlayer() override;

        AbstractAudioPlayer(const AbstractAudioPlayer&)=delete;
        AbstractAudioPlayer(AbstractAudioPlayer&&)=delete;
        AbstractAudioPlayer& operator=(const AbstractAudioPlayer&)=delete;
        AbstractAudioPlayer& operator=(AbstractAudioPlayer&&)=delete;

        // ---- state pushed by the host ----------------------------------------------------

        void setMode(Mode mode)
        {
            m_mode=mode;
            updateMode();
        }

        Mode mode() const noexcept
        {
            return m_mode;
        }

        //! Text of the title button: a file name, or a message's sender and date.
        void setTitle(QString title)
        {
            m_title=std::move(title);
            updateTitle();
        }

        const QString& title() const noexcept
        {
            return m_title;
        }

        /**
         * @brief Whether the title acts as a button. Default true.
         *
         * Turn it off when there is nowhere for titleClicked() to go, for instance a file that
         * was not received in a chat.
         */
        void setTitleClickable(bool enable)
        {
            m_titleClickable=enable;
            updateTitleClickable();
        }

        bool isTitleClickable() const noexcept
        {
            return m_titleClickable;
        }

        void setDuration(qint64 ms)
        {
            m_duration=std::max<qint64>(0,ms);
            updateDuration();
        }

        qint64 duration() const noexcept
        {
            return m_duration;
        }

        void setPosition(qint64 ms)
        {
            m_position=std::max<qint64>(0,ms);
            updatePosition();
        }

        qint64 position() const noexcept
        {
            return m_position;
        }

        //! Show the Pause button (true) or the Play button (false).
        void setPlaying(bool playing)
        {
            m_playing=playing;
            updatePlaying();
        }

        bool isPlaying() const noexcept
        {
            return m_playing;
        }

        //! 0..1
        void setVolume(qreal volume)
        {
            m_volume=std::min<qreal>(1.0,std::max<qreal>(0.0,volume));
            updateVolume();
        }

        qreal volume() const noexcept
        {
            return m_volume;
        }

        void setMuted(bool muted)
        {
            m_muted=muted;
            updateMuted();
        }

        bool isMuted() const noexcept
        {
            return m_muted;
        }

        //! Playback speed as a ratio, 1.0 is normal.
        void setSpeed(qreal speed)
        {
            m_speed=speed;
            updateSpeed();
        }

        qreal speed() const noexcept
        {
            return m_speed;
        }

        //! Waveform for the progress bar, 100 bytes of 0..255 for a voice message. Ignored
        //! by the Line progress style.
        void setWaveform(QByteArray waveform)
        {
            m_waveform=std::move(waveform);
            updateWaveform();
        }

        const QByteArray& waveform() const noexcept
        {
            return m_waveform;
        }

        //! Bars for a voice message, Line for an audio file.
        void setProgressStyle(WaveformBar::Style style)
        {
            m_progressStyle=style;
            updateProgressStyle();
        }

        WaveformBar::Style progressStyle() const noexcept
        {
            return m_progressStyle;
        }

        // ---- engine ------------------------------------------------------------------------

        /**
         * @brief Let an engine play for this player.
         *
         * Connects both ways: the engine's position, duration and state drive the display, and the
         * user's play, pause, stop, seek, volume, mute and speed requests are forwarded to it. The
         * player's current volume, mute and speed are applied to the engine at once. The engine is
         * not owned. Pass nullptr to detach.
         */
        void setEngine(AudioPlaybackEngine* engine);

        AudioPlaybackEngine* engine() const noexcept
        {
            return m_engine.data();
        }

    signals:

        void playRequested();
        void pauseRequested();

        //! Only in Mode::Panel, there is no Stop button in a dialog.
        void stopRequested();

        //! The user let go of the progress bar; `ms` is where. Not sent while dragging.
        void seekRequested(qint64 ms);

        void volumeChanged(qreal volume);
        void mutedChanged(bool muted);
        void speedChanged(qreal speed);

        //! The user clicked the title.
        void titleClicked();

        //! The engine reported an error.
        void errorOccurred(const QString& message);

    protected:

        //! For the concrete class: the user changed the value, so remember it and tell the host.
        void userChangedVolume(qreal volume);
        void userChangedMuted(bool muted);
        void userChangedSpeed(qreal speed);

        virtual void updateMode() {}
        virtual void updateTitle() {}
        virtual void updateTitleClickable() {}
        virtual void updateDuration() {}
        virtual void updatePosition() {}
        virtual void updatePlaying() {}
        virtual void updateVolume() {}
        virtual void updateMuted() {}
        virtual void updateSpeed() {}
        virtual void updateWaveform() {}
        virtual void updateProgressStyle() {}

    private:

        Mode m_mode=Mode::Panel;
        QString m_title;
        bool m_titleClickable=true;
        qint64 m_duration=0;
        qint64 m_position=0;
        bool m_playing=false;
        qreal m_volume=1.0;
        bool m_muted=false;
        qreal m_speed=1.0;
        QByteArray m_waveform;
        WaveformBar::Style m_progressStyle=WaveformBar::Style::Line;

        QPointer<AudioPlaybackEngine> m_engine;
};

}

#endif // UISE_DESKTOP_ABSTRACTAUDIOPLAYER_HPP
