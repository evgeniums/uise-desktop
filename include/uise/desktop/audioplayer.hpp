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

/** @file uise/desktop/audioplayer.hpp
*
*  Declares AudioPlayer, the concrete audio player, and AudioPlayerWidget, the widget it shows.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_AUDIOPLAYER_HPP
#define UISE_DESKTOP_AUDIOPLAYER_HPP

#include <memory>

#include <QByteArray>
#include <QList>
#include <QPointer>
#include <QString>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/waveformbar.hpp>
#include <uise/desktop/abstractaudioplayer.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class AudioPlayerWidget_p;

/**
 * @brief The widget of the audio player: two rows of controls.
 *
 * Row 1 is the title (a button with elided text), then the volume and speed buttons. The volume
 * button opens a vertical slider when hovered and mutes or unmutes when clicked; the speed button
 * opens a menu of ratios. Row 2 is Stop-and-close (dialog mode only), Stop (panel mode only),
 * Play/Pause, the current time, the progress bar and the duration.
 *
 * It is a plain widget with setters and signals; AudioPlayer is what makes it an
 * AbstractAudioPlayer. Setters never emit, signals are for user input only.
 *
 * QSS: uise--AudioPlayerWidget, children by object name: #topRow, #bottomRow, #titleLabel,
 * #volumeButton, #speedButton, #stopCloseButton, #stopButton, #playButton, #positionLabel,
 * #progressBar, #durationLabel; the volume popup is #volumePopup with #volumeSlider. Icons are
 * the "AudioPlayer" context of audioplayer.json, and the "AudioPlayerDanger" context for the red
 * glyph of the stop-and-close button.
 */
class UISE_DESKTOP_EXPORT AudioPlayerWidget : public WidgetQFrame
{
    Q_OBJECT

    public:

        explicit AudioPlayerWidget(QWidget* parent=nullptr);
        ~AudioPlayerWidget();

        AudioPlayerWidget(const AudioPlayerWidget&)=delete;
        AudioPlayerWidget(AudioPlayerWidget&&)=delete;
        AudioPlayerWidget& operator=(const AudioPlayerWidget&)=delete;
        AudioPlayerWidget& operator=(AudioPlayerWidget&&)=delete;

        //! The playback speeds the speed menu offers.
        static QList<qreal> speeds();

        //! Panel mode shows the plain Stop button, dialog mode the red stop-and-close one.
        void setPanelMode(bool panel);

        void setTitle(const QString& title);
        void setTitleClickable(bool enable);

        void setDuration(qint64 ms);
        void setPosition(qint64 ms);
        void setPlaying(bool playing);

        void setVolume(qreal volume);
        void setMuted(bool muted);
        void setSpeed(qreal speed);

        void setWaveform(const QByteArray& waveform);
        void setProgressStyle(WaveformBar::Style style);

        //! The progress bar, for a host that wants to style or crop it.
        WaveformBar* waveformBar() const noexcept;

    signals:

        void playRequested();
        void pauseRequested();
        void stopRequested();

        //! The red button of dialog mode: stop the playback and close the player.
        void stopAndCloseRequested();
        void seekRequested(qint64 ms);
        void volumeChanged(qreal volume);
        void mutedChanged(bool muted);
        void speedChanged(qreal speed);
        void titleClicked();

    protected:

        bool eventFilter(QObject* watched, QEvent* event) override;
        void changeEvent(QEvent* event) override;
        void hideEvent(QHideEvent* event) override;

    private:

        void retranslate();

        //! Light the title, or put it out. A title that is not clickable never lights.
        void updateTitleHover(bool hovered);
        void updateVolumeButton();
        void updateSpeedButton();

        //! Put the ripple of the speed button, a circle, on the middle of its text, once the layout has settled.
        void scheduleSpeedRippleCentering();
        void updatePlayButton();
        void updateTimeLabels();

        //! Pin the position label to the widest the clock can get at the current duration, so
        //! counting up never resizes the player. See the definition for the full reasoning.
        void updateTimeLabelWidth();
        void updateProgress();

        void openVolumePopup();
        void closeVolumePopup();

        //! Restack the open slider above the player's own window, which a click on the player
        //! brings to the front of the window level they share. See the definition.
        void raiseVolumePopup();
        void onVolumeHoverPoll();

        std::unique_ptr<AudioPlayerWidget_p> pimpl;
};

/**
 * @brief The concrete audio player: an AbstractAudioPlayer made of an AudioPlayerWidget.
 *
 * Build it with makeWidget<AbstractAudioPlayer,AudioPlayer>(parent) from a WidgetBase, or
 * construct it and call initWidget(parent) from anywhere else. State set before the widget
 * exists is applied when it is created.
 */
class UISE_DESKTOP_EXPORT AudioPlayer : public AbstractAudioPlayer
{
    Q_OBJECT

    public:

        explicit AudioPlayer(QObject* parent=nullptr)
            : AbstractAudioPlayer(parent)
        {}

        //! The widget, null until it has been created.
        AudioPlayerWidget* playerWidget() const noexcept
        {
            return m_widget.data();
        }

    protected:

        Widget* doCreateActualWidget(QWidget* parent) override;

        void updateMode() override;
        void updateTitle() override;
        void updateTitleClickable() override;
        void updateDuration() override;
        void updatePosition() override;
        void updatePlaying() override;
        void updateVolume() override;
        void updateMuted() override;
        void updateSpeed() override;
        void updateWaveform() override;
        void updateProgressStyle() override;

    private:

        QPointer<AudioPlayerWidget> m_widget;
};

}

#endif // UISE_DESKTOP_AUDIOPLAYER_HPP
