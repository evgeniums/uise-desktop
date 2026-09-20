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

/** @file uise/desktop/waveformbar.hpp
*
*  Declares WaveformBar – a seekable, optionally croppable audio waveform or linear progress bar.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_WAVEFORMBAR_HPP
#define UISE_DESKTOP_WAVEFORMBAR_HPP

#include <memory>

#include <QByteArray>
#include <QColor>
#include <QWidget>

#include <uise/desktop/uisedesktop.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class WaveformBar_p;

/**
 * @brief Waveform (or plain line) showing playback progress, that the user can click and drag to
 *        seek and, optionally, trim with two handles.
 *
 * One widget serves both forms the audio player needs: Style::Bars draws a stored waveform, as
 * in a voice message bubble; Style::Line draws a thin track with a knob, for an ordinary audio
 * file that has no waveform. Everything else -- progress, seeking, cropping -- behaves the same.
 *
 * The widget knows nothing about audio. The host pushes a waveform (setWaveform()) and the current
 * position (setProgress()), and hears about the user's input through signals. Positions are
 * fractions of the whole, 0..1, never times.
 *
 * The waveform is a sequence of bytes, 0..255 each, one per bar, as the voice message format
 * stores it. When there are more values than fit the width, neighbouring values are merged by
 * taking their maximum so the shape survives; when there are fewer bars than values it never
 * draws more than one bar per value.
 *
 * Seeking: press or drag inside the bar emits seekRequested(). While the button is held
 * isSeeking() is true, so a host can stop moving the bar from playback position updates and let
 * the finger drive it; seekFinished() reports where it was let go. When croppable, seeking is
 * clamped to the crop range.
 *
 * Cropping: setCroppable(true) shows two handles at cropStart()/cropEnd(). Dragging a handle emits
 * cropChanged(); audio outside the range is drawn dimmed. The handles cannot cross and keep at least
 * minimumCropFraction() between them. Pressing anywhere else still seeks.
 *
 * Colours and geometry are Q_PROPERTYs so a stylesheet can set them with qproperty-*; defaults
 * are in resources/style/waveformbar.qss and its light/dark variants.
 */
class UISE_DESKTOP_EXPORT WaveformBar : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(QColor barColor       READ barColor       WRITE setBarColor)
    Q_PROPERTY(QColor progressColor  READ progressColor  WRITE setProgressColor)
    Q_PROPERTY(QColor cropColor      READ cropColor      WRITE setCropColor)
    Q_PROPERTY(int    barWidth       READ barWidth       WRITE setBarWidth)
    Q_PROPERTY(int    barSpacing     READ barSpacing     WRITE setBarSpacing)
    Q_PROPERTY(int    minBarHeight   READ minBarHeight   WRITE setMinBarHeight)
    Q_PROPERTY(int    handleWidth    READ handleWidth    WRITE setHandleWidth)
    Q_PROPERTY(qreal  outsideOpacity READ outsideOpacity WRITE setOutsideOpacity)

    public:

        enum class Style
        {
            Bars,   //!< a bar per waveform value
            Line    //!< a thin track with a knob, no waveform
        };
        Q_ENUM(Style)

        explicit WaveformBar(QWidget* parent=nullptr);
        ~WaveformBar() override;

        WaveformBar(const WaveformBar&)=delete;
        WaveformBar(WaveformBar&&)=delete;
        WaveformBar& operator=(const WaveformBar&)=delete;
        WaveformBar& operator=(WaveformBar&&)=delete;

        // ---- content ----------------------------------------------------------------------

        void setStyle(Style style);
        Style style() const noexcept;

        /**
         * @brief Waveform to draw, one byte 0..255 per bar. Empty draws flat placeholder bars, so
         *        a bubble has its shape before the data arrives.
         */
        void setWaveform(const QByteArray& waveform);
        const QByteArray& waveform() const noexcept;

        /** @brief Playback position as a fraction of the whole, clamped to 0..1. */
        void setProgress(qreal fraction);
        qreal progress() const noexcept;

        // ---- interaction ------------------------------------------------------------------

        void setSeekable(bool enable);
        bool isSeekable() const noexcept;

        //! true from the press to the release of a seek gesture
        bool isSeeking() const noexcept;

        void setCroppable(bool enable);
        bool isCroppable() const noexcept;

        /**
         * @brief Set the crop range, as fractions with start < end. Clamped, and kept apart by
         *        minimumCropFraction(). Does not emit cropChanged(): that is for user input.
         */
        void setCropRange(qreal start, qreal end);
        qreal cropStart() const noexcept;
        qreal cropEnd() const noexcept;

        //! Smallest range the handles allow, as a fraction of the whole (default 0.05).
        void setMinimumCropFraction(qreal fraction);
        qreal minimumCropFraction() const noexcept;

        // ---- geometry and colour knobs ---------------------------------------------------

        void setBarColor(const QColor& color);
        QColor barColor() const noexcept;

        void setProgressColor(const QColor& color);
        QColor progressColor() const noexcept;

        void setCropColor(const QColor& color);
        QColor cropColor() const noexcept;

        void setBarWidth(int px);
        int barWidth() const noexcept;

        void setBarSpacing(int px);
        int barSpacing() const noexcept;

        //! Height of the shortest bar, so silence is still visible.
        void setMinBarHeight(int px);
        int minBarHeight() const noexcept;

        void setHandleWidth(int px);
        int handleWidth() const noexcept;

        //! Opacity of what lies outside the crop range, 0..1.
        void setOutsideOpacity(qreal opacity);
        qreal outsideOpacity() const noexcept;

        // ---- geometry helpers, public so a host (and a test) can reason about the same numbers ----

        //! Number of bars drawn at the current width and waveform.
        int visibleBarCount() const;

        //! Fraction of the whole that the widget x coordinate stands for, clamped to 0..1.
        qreal fractionAtX(qreal x) const;

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    signals:

        //! The user pressed or dragged in the bar; `fraction` is where, within the crop range if any.
        void seekRequested(qreal fraction);

        //! The seek gesture ended at `fraction`.
        void seekFinished(qreal fraction);

        //! The user dragged a crop handle.
        void cropChanged(qreal start, qreal end);

    protected:

        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:

        std::unique_ptr<WaveformBar_p> pimpl;
};

}

#endif // UISE_DESKTOP_WAVEFORMBAR_HPP
