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

/** @file uise/desktop/imageanimator.hpp
*
*  Declares ImageAnimator.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_IMAGEANIMATOR_HPP
#define UISE_DESKTOP_IMAGEANIMATOR_HPP

#include <memory>

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QImage>
#include <QSize>

#include <uise/desktop/uisedesktop.hpp>

class QBuffer;
class QMovie;

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Encoded animated-image content: either a file path (":/..." resource paths included) or
 *  bytes already in memory, never both -- mirrors ImageLabel::setImageFile()/setImageData()'s two
 *  input shapes, packaged so it can ride through PixmapProducer/PixmapSource/PixmapConsumer and
 *  AbstractImageViewer::Image/ChatImageViewer::ChatImage as a single value.
 */
struct UISE_DESKTOP_EXPORT AnimationContent
{
    QString    file;
    QByteArray data;
    QByteArray format;

    bool isNull() const noexcept
    {
        return file.isEmpty() && data.isEmpty();
    }

    bool operator==(const AnimationContent& other) const noexcept
    {
        return file==other.file && data==other.data && format==other.format;
    }

    bool operator!=(const AnimationContent& other) const noexcept
    {
        return !(*this==other);
    }
};

/**
 * @brief Widget-free QMovie-driven playback engine for animated images (GIF, APNG, animated WebP).
 *
 * Extracted from ImageLabel so a non-QLabel host (see ImageViewer) can drive the same decode/
 * playback logic without owning a QLabel. Unlike ImageLabel, this class does not render or scale
 * anything for painting -- currentFrame()/firstFrame() hand back plain QImage frames at their
 * decoded resolution (optionally pre-scaled via setScaledSize(), the same QMovie-level decode-time
 * scaling ImageLabel used internally) and the host is responsible for tiling/painting them.
 *
 * A host embeds this by subclassing and overriding isWidgetVisible()/isWindowActive()/isHovered()
 * to report its own visibility/activation/hover state (see ImageLabel's use of these), and connects
 * to frameChanged() to know when to repaint.
 *
 * Animated WebP requires the qtimageformats Qt plugin to be deployed alongside the application;
 * without it, files of that format simply fail to load (see loadFailed()). GIF playback works out
 * of the box, as QMovie's GIF handler is built into Qt.
 */
class UISE_DESKTOP_EXPORT ImageAnimator : public QObject
{
    Q_OBJECT

    public:

        //! When the animation of an animated image is allowed to run.
        enum class AnimationMode
        {
            Auto,       //!< Play as soon as animated content is loaded and the host is ready.
            Never,      //!< Never play, always rest on the first frame.
            OnHover,    //!< Rest at first frame, play while isHovered() is true.
            Manual      //!< Rest at first frame, playback driven only by play()/pause()/stop()/
                        //!< togglePlay().
        };
        Q_ENUM(AnimationMode)

    // Property declarations must follow the AnimationMode enum: moc parses the header top to
    // bottom and needs the enum (registered via Q_ENUM above) already visible when it reaches
    // the Q_PROPERTY line below that references it as a type.
    Q_PROPERTY(AnimationMode animationMode READ animationMode WRITE setAnimationMode)
    Q_PROPERTY(int animationSpeed READ animationSpeed WRITE setAnimationSpeed)

    public:

        constexpr static const AnimationMode DefaultAnimationMode=AnimationMode::Auto;

        explicit ImageAnimator(QObject* parent=nullptr);

        //! Dtor is defined in the .cpp because QMovie/QBuffer are incomplete types here.
        ~ImageAnimator() override;

        ImageAnimator(const ImageAnimator&)=delete;
        ImageAnimator(ImageAnimator&&)=delete;
        ImageAnimator& operator=(const ImageAnimator&)=delete;
        ImageAnimator& operator=(ImageAnimator&&)=delete;

        /**
         * @brief Load content from a file path (or a ":/..." resource path).
         * @return false if the file cannot be read or decoded, in which case loadFailed() is also
         *  emitted and any previously loaded content is dropped.
         */
        bool loadFile(const QString& fileName);

        /**
         * @brief Load content from bytes already in memory.
         * @param data Raw file content, e.g. "GIF89a...". Copied and retained for the lifetime of
         *  the content, so the caller's buffer can be discarded afterwards.
         * @param format Optional format hint, e.g. "gif", "webp", "png". When empty, the format
         *  is sniffed from the content itself.
         * @return false if the data cannot be decoded, in which case loadFailed() is also emitted
         *  and any previously loaded content is dropped.
         */
        bool loadData(const QByteArray& data, const QByteArray& format={});

        //! Dispatches to loadFile()/loadData() depending on which of AnimationContent::file/data
        //! is set; false (no-op, nothing emitted) if content.isNull().
        bool loadContent(const AnimationContent& content);

        //! Drop all loaded content (movie, buffer, cached decoder state).
        void clear();

        //! File name passed to loadFile(), empty when content came from loadData() or none is loaded.
        QString fileName() const
        {
            return m_fileName;
        }

        //! Bytes passed to loadData(), empty when content came from loadFile() or none is loaded.
        QByteArray data() const
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
        QSize naturalSize() const noexcept
        {
            return m_naturalSize;
        }

        //! Number of frames of the loaded content: 0 when unknown/nothing loaded, 1 for still content.
        int frameCount() const;

        //! Current movie frame, at whatever size setScaledSize() last selected (natural size if
        //! never called). Null when not animated/nothing loaded.
        QImage currentFrame() const;

        //! First frame, re-decoded on demand from the retained file/bytes -- deliberately not
        //! cached at full resolution, mirroring ImageLabel::rebuildStills()'s memory profile.
        //! Works for both animated and still content; null when nothing is loaded or decoding
        //! fails.
        QImage firstFrame() const;

        //! Set the mode governing when animated content is allowed to play; re-evaluates playback
        //! immediately via sync().
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

        //! Set whether playback pauses while isWidgetVisible() is false, default true.
        void setPauseWhenHidden(bool enable) noexcept
        {
            m_pauseWhenHidden=enable;
        }

        bool isPauseWhenHidden() const noexcept
        {
            return m_pauseWhenHidden;
        }

        //! Set whether playback pauses while isWindowActive() is false, default false.
        void setPauseWhenWindowInactive(bool enable) noexcept
        {
            m_pauseWhenInactive=enable;
        }

        bool isPauseWhenWindowInactive() const noexcept
        {
            return m_pauseWhenInactive;
        }

        /**
         * @brief Set the size QMovie decodes frames at, in device pixels.
         * @param size Invalid/null to decode at natural resolution (the default).
         *
         * Mirrors ImageLabel's internal applyScaledSize() -- letting QMovie downscale during
         * decode is cheaper than decoding at full resolution and downscaling every frame in
         * software. When isCacheFrames() is enabled and a movie is already loaded, changing this
         * rebuilds the movie from scratch (QMovie caches frames at the scaled size in effect when
         * they were cached).
         */
        void setScaledSize(const QSize& size);

        QSize scaledSize() const noexcept
        {
            return m_scaledSize;
        }

        //! Re-evaluate playback against the current mode/environment -- call after anything that
        //! might change isWidgetVisible()/isWindowActive()/isHovered()'s answer (show/hide/
        //! activation/hover transitions), mirroring ImageLabel's own event-handler calls to its
        //! (now-internal) syncPlayback().
        void sync();

    public slots:

        //! Request playback. Meaningful in AnimationMode::Manual; in other modes it only records
        //! intent for when the mode allows it.
        void play();

        //! Freeze playback on the current frame.
        void pause();

        //! Stop playback and rewind to the first frame.
        void stop();

        //! play() when not playing, pause() when playing.
        void togglePlay();

    signals:

        //! Emitted whenever the frame currentFrame() would return has changed -- including the
        //! rewind-to-first-frame that happens when playback rests (see AnimationMode's modes).
        //! The host should repaint in response.
        void frameChanged();

        //! Emitted whenever the effective running state of the animation changes.
        void playingChanged(bool playing);

        //! Emitted when the loaded content switches between still and animated.
        void animatedChanged(bool animated);

        //! Emitted after content has been successfully loaded.
        void loaded();

        //! Emitted when loading or decoding content failed, with a human readable reason.
        void loadFailed(const QString& error);

        //! Forwarded from QMovie::finished(), reached only by animations with a finite loop count.
        void finished();

    protected:

        //! True when the host is currently visible -- gates playback when isPauseWhenHidden() is
        //! true. Default true (never gated). Override with e.g. QWidget::isVisible().
        virtual bool isWidgetVisible() const
        {
            return true;
        }

        //! True when the host's top-level window is active, or the host has no meaningful notion
        //! of a window -- gates playback when isPauseWhenWindowInactive() is true. Default true
        //! (never gated). Override with e.g. `!window() || window()->isActiveWindow()`.
        virtual bool isWindowActive() const
        {
            return true;
        }

        //! True when the host is currently hovered -- only consulted in AnimationMode::OnHover.
        //! Default false. Override with e.g. RoundedImage::isEffectiveHovered().
        virtual bool isHovered() const
        {
            return false;
        }

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

        //! @param wasAnimated isAnimated()'s value before the caller (loadFile()/loadData())
        //!  called resetContent() -- must be captured by the caller before that call, since
        //!  resetContent() itself unconditionally clears m_animated, so reading it fresh in here
        //!  would always observe false and animatedChanged() would never fire on an animated ->
        //!  still transition.
        bool doLoad(bool wasAnimated);
        void createMovie();

        bool shouldPlay() const;
        bool restOnFirstFrame() const;

        // NOTE: declaration order matters. QMovie does not own its QIODevice, so m_movie must be
        // destroyed before m_buffer, and m_buffer (which wraps &m_data) before m_data. Members are
        // destroyed in reverse declaration order, hence m_data, m_buffer, m_movie in this order --
        // do not reorder these three, and do not parent either of them to `this` as a QObject
        // child (child destruction order is insertion order, not declaration order).
        QString                  m_fileName;
        QByteArray               m_data;
        QByteArray               m_format;
        std::unique_ptr<QBuffer> m_buffer;
        std::unique_ptr<QMovie>  m_movie;

        QSize m_naturalSize;
        QSize m_scaledSize;

        AnimationMode m_mode;
        int  m_speed;
        bool m_animated;
        bool m_cacheFrames;
        bool m_pauseWhenHidden;
        bool m_pauseWhenInactive;
        bool m_playing;
        bool m_inSync;
        ManualState m_manual;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_IMAGEANIMATOR_HPP
