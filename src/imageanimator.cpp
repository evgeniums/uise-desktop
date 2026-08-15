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

/** @file uise/desktop/src/imageanimator.cpp
*
*  Defines ImageAnimator.
*
*/

/****************************************************************************/

#include <QBuffer>
#include <QMovie>
#include <QImageReader>

#include <uise/desktop/imageanimator.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

ImageAnimator::ImageAnimator(QObject* parent)
    : QObject(parent),
      m_mode(DefaultAnimationMode),
      m_speed(100),
      m_animated(false),
      m_cacheFrames(false),
      m_pauseWhenHidden(true),
      m_pauseWhenInactive(false),
      m_playing(false),
      m_inSync(false),
      m_manual(ManualState::Stopped)
{}

//--------------------------------------------------------------------------

ImageAnimator::~ImageAnimator()
{
    // Disconnect first so no slot runs against a half-destroyed object while the movie is being
    // torn down, then stop before releasing -- see resetContent() for the same ordering used at
    // runtime and the member-order comment in the header for why m_movie must go before m_buffer.
    if (m_movie)
    {
        m_movie->disconnect(this);
        m_movie->stop();
    }
    m_movie.reset();
    m_buffer.reset();
}

//--------------------------------------------------------------------------

bool ImageAnimator::loadFile(const QString& fileName)
{
    // Captured before resetContent() below, which unconditionally clears m_animated -- see
    // doLoad()'s own doc on why this can't be read fresh inside it.
    auto wasAnimated=m_animated;
    resetContent();
    m_fileName=fileName;
    return doLoad(wasAnimated);
}

//--------------------------------------------------------------------------

bool ImageAnimator::loadData(const QByteArray& data, const QByteArray& format)
{
    auto wasAnimated=m_animated;
    resetContent();
    m_data=data;
    m_format=format;
    return doLoad(wasAnimated);
}

//--------------------------------------------------------------------------

bool ImageAnimator::loadContent(const AnimationContent& content)
{
    if (content.isNull())
    {
        return false;
    }
    if (!content.file.isEmpty())
    {
        return loadFile(content.file);
    }
    return loadData(content.data,content.format);
}

//--------------------------------------------------------------------------

void ImageAnimator::clear()
{
    resetContent();
}

//--------------------------------------------------------------------------

void ImageAnimator::resetContent()
{
    if (m_movie)
    {
        m_movie->disconnect(this);
        m_movie->stop();
    }
    m_movie.reset();
    m_buffer.reset();

    m_fileName.clear();
    m_data.clear();
    m_format.clear();

    m_naturalSize=QSize{};
    m_animated=false;
    m_manual=ManualState::Stopped;

    if (m_playing)
    {
        m_playing=false;
        emit playingChanged(false);
    }
}

//--------------------------------------------------------------------------

bool ImageAnimator::doLoad(bool wasAnimated)
{
    QImageReader reader;
    QBuffer sniffBuffer;

    if (!m_fileName.isEmpty())
    {
        reader.setFileName(m_fileName);
    }
    else
    {
        sniffBuffer.setBuffer(&m_data);
        sniffBuffer.open(QIODevice::ReadOnly);
        reader.setDevice(&sniffBuffer);
        if (!m_format.isEmpty())
        {
            reader.setFormat(m_format);
        }
        else
        {
            reader.setDecideFormatFromContent(true);
        }
    }
    reader.setAutoTransform(true);

    if (!reader.canRead())
    {
        auto error=reader.errorString();
        resetContent();
        emit loadFailed(error);
        return false;
    }

    m_naturalSize=reader.size();

    // Query animation support/frame count before read() -- the conventional order, and safer than
    // querying afterwards since some format handlers determine these from a quick block scan that
    // is not guaranteed to still be meaningful once the first frame has been decoded and the
    // device position has moved on. wasAnimated itself comes from the caller (see this method's
    // doc) rather than being read fresh here, since resetContent() already cleared m_animated by
    // this point.
    m_animated=reader.supportsAnimation() && reader.imageCount()!=1;

    auto first=reader.read();
    if (first.isNull())
    {
        auto error=reader.errorString();
        resetContent();
        emit loadFailed(error);
        return false;
    }
    if (!m_naturalSize.isValid())
    {
        m_naturalSize=first.size();
    }

    if (m_animated)
    {
        createMovie();
        if (m_movie && m_movie->frameCount()==1)
        {
            // A single-frame "animated" format (e.g. a static GIF) -- no point driving a timer for
            // it, fall back to the still path below.
            m_movie->disconnect(this);
            m_movie.reset();
            m_buffer.reset();
            m_animated=false;
        }
    }

    if (wasAnimated!=m_animated)
    {
        emit animatedChanged(m_animated);
    }
    emit loaded();

    sync();

    return true;
}

//--------------------------------------------------------------------------

void ImageAnimator::createMovie()
{
    m_buffer.reset();
    // No QObject parent: m_movie's lifetime is owned exclusively by the unique_ptr (see the
    // member-order comment in the header). Parenting it to `this` would register it as a QObject
    // child too, and ~QObject would then try to delete it a second time on top of the unique_ptr.
    m_movie.reset(new QMovie());

    if (!m_fileName.isEmpty())
    {
        m_movie->setFileName(m_fileName);
    }
    else
    {
        m_buffer.reset(new QBuffer(&m_data));
        m_buffer->open(QIODevice::ReadOnly);
        m_movie->setDevice(m_buffer.get());
        if (!m_format.isEmpty())
        {
            m_movie->setFormat(m_format);
        }
    }

    m_movie->setCacheMode(m_cacheFrames ? QMovie::CacheAll : QMovie::CacheNone);
    m_movie->setSpeed(m_speed);

    connect(
        m_movie.get(),
        &QMovie::frameChanged,
        this,
        &ImageAnimator::onFrameChanged
    );
    connect(
        m_movie.get(),
        &QMovie::error,
        this,
        &ImageAnimator::onMovieError
    );
    connect(
        m_movie.get(),
        &QMovie::finished,
        this,
        &ImageAnimator::onMovieFinished
    );

    if (m_scaledSize.isValid() && !m_scaledSize.isNull())
    {
        m_movie->setScaledSize(m_scaledSize);
    }
}

//--------------------------------------------------------------------------

void ImageAnimator::setScaledSize(const QSize& size)
{
    m_scaledSize=size;

    if (!m_movie)
    {
        return;
    }

    if (m_cacheFrames)
    {
        // QMovie caches frames at the scaled size in effect when they were cached, so changing
        // the scaled size of a CacheAll movie requires rebuilding it from scratch (createMovie()
        // applies the new scaled size itself) rather than calling setScaledSize() on the live
        // instance.
        auto wasPlaying=m_movie->state()==QMovie::Running;
        createMovie();
        if (wasPlaying)
        {
            sync();
        }
        return;
    }

    if (m_scaledSize.isValid() && !m_scaledSize.isNull())
    {
        m_movie->setScaledSize(m_scaledSize);
    }
}

//--------------------------------------------------------------------------

QImage ImageAnimator::currentFrame() const
{
    if (m_movie)
    {
        return m_movie->currentImage();
    }
    return QImage{};
}

//--------------------------------------------------------------------------

QImage ImageAnimator::firstFrame() const
{
    if (m_fileName.isEmpty() && m_data.isEmpty())
    {
        return QImage{};
    }

    QImageReader reader;
    QBuffer buf;
    QByteArray dataCopy;
    if (!m_fileName.isEmpty())
    {
        reader.setFileName(m_fileName);
    }
    else
    {
        dataCopy=m_data;
        buf.setBuffer(&dataCopy);
        buf.open(QIODevice::ReadOnly);
        reader.setDevice(&buf);
        if (!m_format.isEmpty())
        {
            reader.setFormat(m_format);
        }
        else
        {
            reader.setDecideFormatFromContent(true);
        }
    }
    reader.setAutoTransform(true);

    return reader.read();
}

//--------------------------------------------------------------------------

int ImageAnimator::frameCount() const
{
    if (m_movie)
    {
        return m_movie->frameCount();
    }
    return (m_fileName.isEmpty() && m_data.isEmpty()) ? 0 : 1;
}

//--------------------------------------------------------------------------

void ImageAnimator::setAnimationMode(AnimationMode mode)
{
    if (m_mode==mode)
    {
        return;
    }
    m_mode=mode;
    m_manual=ManualState::Stopped;
    sync();
}

//--------------------------------------------------------------------------

void ImageAnimator::setAnimationSpeed(int percent)
{
    m_speed=percent;
    if (m_movie)
    {
        m_movie->setSpeed(m_speed);
    }
}

//--------------------------------------------------------------------------

void ImageAnimator::setCacheFrames(bool enable)
{
    if (m_cacheFrames==enable)
    {
        return;
    }
    m_cacheFrames=enable;
    if (m_movie)
    {
        auto wasPlaying=m_movie->state()==QMovie::Running;
        createMovie();
        if (wasPlaying)
        {
            sync();
        }
    }
}

//--------------------------------------------------------------------------

void ImageAnimator::play()
{
    m_manual=ManualState::Playing;
    sync();
}

//--------------------------------------------------------------------------

void ImageAnimator::pause()
{
    m_manual=ManualState::Paused;
    sync();
}

//--------------------------------------------------------------------------

void ImageAnimator::stop()
{
    m_manual=ManualState::Stopped;
    sync();
}

//--------------------------------------------------------------------------

void ImageAnimator::togglePlay()
{
    if (isPlaying())
    {
        pause();
    }
    else
    {
        play();
    }
}

//--------------------------------------------------------------------------

bool ImageAnimator::shouldPlay() const
{
    if (!m_animated || !m_movie)
    {
        return false;
    }
    if (m_pauseWhenHidden && !isWidgetVisible())
    {
        return false;
    }
    if (m_pauseWhenInactive && !isWindowActive())
    {
        return false;
    }

    switch (m_mode)
    {
        case AnimationMode::Auto:
            return true;
        case AnimationMode::Never:
            return false;
        case AnimationMode::OnHover:
            return isHovered();
        case AnimationMode::Manual:
            return m_manual==ManualState::Playing;
    }
    return false;
}

//--------------------------------------------------------------------------

bool ImageAnimator::restOnFirstFrame() const
{
    if (m_mode==AnimationMode::Never)
    {
        return true;
    }
    if (m_mode==AnimationMode::OnHover)
    {
        return !isHovered();
    }
    if (m_mode==AnimationMode::Manual)
    {
        return m_manual==ManualState::Stopped;
    }
    // Auto only ever stops because of hide/deactivate -- freeze in place and resume from there.
    return false;
}

//--------------------------------------------------------------------------

void ImageAnimator::sync()
{
    if (m_inSync)
    {
        return;
    }
    m_inSync=true;

    if (m_animated && m_movie)
    {
        auto doPlay=shouldPlay();
        if (doPlay)
        {
            if (m_movie->state()==QMovie::Paused)
            {
                m_movie->setPaused(false);
            }
            else if (m_movie->state()!=QMovie::Running)
            {
                m_movie->start();
            }
        }
        else
        {
            if (restOnFirstFrame())
            {
                m_movie->stop();
                // QMovie::stop() does not guarantee the frame lands on 0 -- force it explicitly so
                // currentFrame() deterministically shows the first frame while resting, mirroring
                // ImageLabel's previous behaviour of painting a separately-cached still frame.
                m_movie->jumpToFrame(0);
                emit frameChanged();
            }
            else if (m_movie->state()==QMovie::Running)
            {
                m_movie->setPaused(true);
            }
        }

        auto running=m_movie->state()==QMovie::Running;
        if (m_playing!=running)
        {
            m_playing=running;
            emit playingChanged(m_playing);
        }
    }
    else if (m_playing)
    {
        m_playing=false;
        emit playingChanged(false);
    }

    m_inSync=false;
}

//--------------------------------------------------------------------------

void ImageAnimator::onFrameChanged(int frameNumber)
{
    std::ignore=frameNumber;
    emit frameChanged();
}

//--------------------------------------------------------------------------

void ImageAnimator::onMovieError()
{
    if (m_movie)
    {
        emit loadFailed(m_movie->lastErrorString());
    }
}

//--------------------------------------------------------------------------

void ImageAnimator::onMovieFinished()
{
    emit finished();

    auto running=m_movie && m_movie->state()==QMovie::Running;
    if (m_playing!=running)
    {
        m_playing=running;
        emit playingChanged(m_playing);
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
