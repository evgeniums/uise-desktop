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

/** @file uise/desktop/imagelabel.hpp
*
*  Declares ImageLabel.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_IMAGELABEL_HPP
#define UISE_DESKTOP_IMAGELABEL_HPP

#include <memory>

#include <QByteArray>
#include <QString>
#include <QPixmap>
#include <QSize>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/roundedimage.hpp>

class QBuffer;
class QMovie;
class QResizeEvent;
class QShowEvent;
class QHideEvent;
class QMouseEvent;

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Extended image label showing either a static image or an animated image.
 *
 * ImageLabel extends RoundedImage with a direct file/bytes content API and QMovie-driven
 * animation playback. Whether the content actually animates is decided from two independent
 * things: the content itself (a single-frame image never animates, regardless of mode) and
 * animationMode() (which can force a still image, gate playback on hover, or hand playback to
 * explicit play()/pause()/stop() calls).
 *
 * Static content still goes through the base RoundedImage rendering (QLabel::pixmap(), the
 * inherited setSvgIcon() fallback, or an inherited PixmapSource via setImageSource()) -- this
 * class only adds a second, self-contained path for animated content and never touches
 * PixmapSource/PixmapProducer.
 *
 * Animated WebP requires the qtimageformats Qt plugin to be deployed alongside the application;
 * without it, files of that format simply fail to load (see imageLoadFailed()). GIF playback
 * works out of the box, as QMovie's GIF handler is built into Qt.
 */
class UISE_DESKTOP_EXPORT ImageLabel : public RoundedImage
{
    Q_OBJECT

    public:

        //! When the animation of an animated image is allowed to run.
        enum class AnimationMode
        {
            Auto,       //!< Play as soon as animated content is loaded and the widget is visible.
            Never,      //!< Never play, always show the first frame as a still image.
            OnHover,    //!< Still at rest, play from the first frame while hovered.
            Manual      //!< Still at rest, playback driven only by play()/pause()/stop()/togglePlay();
                        //!< a click also toggles it when isClickable() is true.
        };
        Q_ENUM(AnimationMode)

    // Property declarations must follow the AnimationMode enum: moc parses the header top to
    // bottom and needs the enum (registered via Q_ENUM above) already visible when it reaches
    // the Q_PROPERTY line below that references it as a type.
    Q_PROPERTY(AnimationMode animationMode READ animationMode WRITE setAnimationMode)
    Q_PROPERTY(int animationSpeed READ animationSpeed WRITE setAnimationSpeed)

    public:

        constexpr static const AnimationMode DefaultAnimationMode=AnimationMode::Auto;
        constexpr static const Qt::AspectRatioMode DefaultAspectRatioMode=Qt::KeepAspectRatioByExpanding;

        //! Ctor, signature mirrors RoundedImage.
        explicit ImageLabel(QWidget* parent=nullptr, Qt::WindowFlags f=Qt::WindowFlags());

        //! Dtor is defined in the .cpp because QMovie/QBuffer are incomplete types here.
        ~ImageLabel() override;

        ImageLabel(const ImageLabel&)=delete;
        ImageLabel(ImageLabel&&)=delete;
        ImageLabel& operator=(const ImageLabel&)=delete;
        ImageLabel& operator=(ImageLabel&&)=delete;

        /**
         * @brief Load content from a file path (or a ":/..." resource path).
         * @param fileName Path to read from.
         * @return false if the file cannot be read or decoded, in which case imageLoadFailed()
         *  is also emitted and any previously loaded content is dropped.
         */
        bool setImageFile(const QString& fileName);

        /**
         * @brief Load content from bytes already in memory.
         * @param data Raw file content, e.g. "GIF89a...". Copied and retained for the lifetime of
         *  the content, so the caller's buffer can be discarded afterwards.
         * @param format Optional format hint, e.g. "gif", "webp", "png". When empty, the format
         *  is sniffed from the content itself.
         * @return false if the data cannot be decoded, in which case imageLoadFailed() is also
         *  emitted and any previously loaded content is dropped.
         */
        bool setImageData(const QByteArray& data, const QByteArray& format={});

        //! Drop all loaded content (movie, buffer, cached frames and the underlying QLabel pixmap).
        void clearImage();

        //! File name passed to setImageFile(), empty when content came from setImageData() or none is loaded.
        QString imageFile() const
        {
            return m_fileName;
        }

        //! Bytes passed to setImageData(), empty when content came from setImageFile() or none is loaded.
        QByteArray imageData() const
        {
            return m_data;
        }

        //! True when the loaded content is a multi-frame animation.
        bool isAnimated() const noexcept
        {
            return m_animated;
        }

        //! True when the underlying animation is currently running.
        bool isPlaying() const noexcept
        {
            return m_playing;
        }

        //! Unscaled pixel size of the loaded content, invalid when no content is loaded.
        QSize naturalImageSize() const noexcept
        {
            return m_naturalSize;
        }

        //! Number of frames of the loaded content: 0 when unknown, 1 for still content.
        int frameCount() const;

        //! Set the mode governing when animated content is allowed to play; re-evaluates playback immediately.
        void setAnimationMode(AnimationMode mode);

        AnimationMode animationMode() const noexcept
        {
            return m_mode;
        }

        //! Set playback speed as a percentage of the source frame delays, default 100.
        void setAnimationSpeed(int percent);

        int animationSpeed() const noexcept
        {
            return m_speed;
        }

        //! Set how frames are fitted into the widget rect, default KeepAspectRatioByExpanding.
        void setAspectRatioMode(Qt::AspectRatioMode mode);

        Qt::AspectRatioMode aspectRatioMode() const noexcept
        {
            return m_aspectMode;
        }

        /**
         * @brief Set whether all decoded frames are kept in memory (QMovie::CacheAll) instead of
         *  being decoded on the fly (QMovie::CacheNone), default false.
         *
         * Enabling this trades memory for CPU on short looping animations; changing it while
         * animated content is loaded rebuilds the underlying QMovie.
         */
        void setCacheFrames(bool enable);

        bool isCacheFrames() const noexcept
        {
            return m_cacheFrames;
        }

        //! Set whether playback pauses while the widget is hidden and resumes on show, default true.
        void setPauseWhenHidden(bool enable) noexcept
        {
            m_pauseWhenHidden=enable;
        }

        bool isPauseWhenHidden() const noexcept
        {
            return m_pauseWhenHidden;
        }

        //! Set whether playback pauses while the top-level window is inactive, default false.
        void setPauseWhenWindowInactive(bool enable) noexcept
        {
            m_pauseWhenInactive=enable;
        }

        bool isPauseWhenWindowInactive() const noexcept
        {
            return m_pauseWhenInactive;
        }

        //! Set whether clicked() is emitted on left mouse press and a click toggles playback in AnimationMode::Manual, default false.
        void setClickable(bool enable) noexcept
        {
            m_clickable=enable;
        }

        bool isClickable() const noexcept
        {
            return m_clickable;
        }

        //! Get the still (first) frame rendered for the current widget size, null when no content is loaded.
        QPixmap stillFrame() const
        {
            return m_stillFrame;
        }

    public slots:

        //! Request playback. Meaningful in AnimationMode::Manual; in other modes it only records intent for when the mode allows it.
        void play();

        //! Freeze playback on the current frame.
        void pause();

        //! Stop playback and rewind to the still first frame.
        void stop();

        //! play() when not playing, pause() when playing.
        void togglePlay();

    signals:

        //! Emitted on left mouse button press when isClickable() is true.
        void clicked();

        //! Emitted whenever the effective running state of the animation changes.
        void playingChanged(bool playing);

        //! Emitted when the loaded content switches between still and animated.
        void animatedChanged(bool animated);

        //! Emitted after content has been successfully loaded and its first frame is ready.
        void imageLoaded();

        //! Emitted when loading or decoding content failed, with a human readable reason.
        void imageLoadFailed(const QString& error);

        //! Forwarded from QMovie::finished(), reached only by animations with a finite loop count.
        void animationFinished();

    protected:

        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void changeEvent(QEvent* event) override;

    private slots:

        void onFrameChanged(int frameNumber);
        void onMovieError();
        void onMovieFinished();

    private:

        //! Playback intent selected through play()/pause()/stop(), meaningful only in AnimationMode::Manual.
        enum class ManualState
        {
            Stopped,
            Playing,
            Paused
        };

        void resetContent();
        bool loadContent();
        void createMovie();
        void applyScaledSize();
        void rebuildStills();
        QPixmap renderTile(const QImage& src) const;

        bool shouldPlay() const;
        bool restOnFirstFrame() const;
        void syncPlayback();

        // NOTE: declaration order matters. QMovie does not own its QIODevice, so m_movie must be
        // destroyed before m_buffer, and m_buffer (which wraps &m_data) before m_data. Members
        // are destroyed in reverse declaration order, hence m_data, m_buffer, m_movie in this
        // order -- do not reorder these three, and do not parent either of them to `this` as a
        // QObject child (child destruction order is insertion order, not declaration order).
        QString                  m_fileName;
        QByteArray               m_data;
        QByteArray               m_format;
        std::unique_ptr<QBuffer> m_buffer;
        std::unique_ptr<QMovie>  m_movie;

        QPixmap m_stillFrame;   // frame 0, rendered for the current widget size
        QPixmap m_paintFrame;   // what paintEvent() draws: m_stillFrame or the current movie frame
        QSize   m_naturalSize;

        AnimationMode        m_mode;
        Qt::AspectRatioMode  m_aspectMode;
        int  m_speed;
        bool m_animated;
        bool m_cacheFrames;
        bool m_pauseWhenHidden;
        bool m_pauseWhenInactive;
        bool m_clickable;
        bool m_playing;
        bool m_frameDirty;
        bool m_inSync;
        ManualState m_manual;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_IMAGELABEL_HPP
