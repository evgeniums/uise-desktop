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

/** @file uise/desktop/src/imagelabel.cpp
*
*  Defines ImageLabel.
*
*/

/****************************************************************************/

#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QEvent>
#include <QBuffer>
#include <QMovie>
#include <QImageReader>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

#include <uise/desktop/imagelabel.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************** ImageLabel *************************************/

//--------------------------------------------------------------------------

ImageLabel::ImageLabel(QWidget* parent, Qt::WindowFlags f)
    : RoundedImage(parent,f),
      m_mode(DefaultAnimationMode),
      m_aspectMode(DefaultAspectRatioMode),
      m_speed(100),
      m_animated(false),
      m_cacheFrames(false),
      m_pauseWhenHidden(true),
      m_pauseWhenInactive(false),
      m_clickable(false),
      m_playing(false),
      m_frameDirty(false),
      m_inSync(false),
      m_manual(ManualState::Stopped)
{}

//--------------------------------------------------------------------------

ImageLabel::~ImageLabel()
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

bool ImageLabel::setImageFile(const QString& fileName)
{
    resetContent();
    m_fileName=fileName;
    return loadContent();
}

//--------------------------------------------------------------------------

bool ImageLabel::setImageData(const QByteArray& data, const QByteArray& format)
{
    resetContent();
    m_data=data;
    m_format=format;
    return loadContent();
}

//--------------------------------------------------------------------------

void ImageLabel::clearImage()
{
    resetContent();
    update();
}

//--------------------------------------------------------------------------

void ImageLabel::resetContent()
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

    m_stillFrame=QPixmap{};
    m_paintFrame=QPixmap{};
    m_naturalSize=QSize{};

    m_animated=false;
    m_frameDirty=false;
    m_manual=ManualState::Stopped;

    if (m_playing)
    {
        m_playing=false;
        emit playingChanged(false);
    }

    QLabel::setPixmap(QPixmap{});
}

//--------------------------------------------------------------------------

bool ImageLabel::loadContent()
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
        emit imageLoadFailed(error);
        return false;
    }

    m_naturalSize=reader.size();

    // Query animation support/frame count before read() -- the conventional order, and safer
    // than querying afterwards since some format handlers determine these from a quick block
    // scan that is not guaranteed to still be meaningful once the first frame has been decoded
    // and the device position has moved on.
    auto wasAnimated=m_animated;
    m_animated=reader.supportsAnimation() && reader.imageCount()!=1;

    auto first=reader.read();
    if (first.isNull())
    {
        auto error=reader.errorString();
        resetContent();
        emit imageLoadFailed(error);
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
            // A single-frame "animated" format (e.g. a static GIF) -- no point driving a timer
            // for it, fall back to the still path below.
            m_movie->disconnect(this);
            m_movie.reset();
            m_buffer.reset();
            m_animated=false;
        }
    }

    if (m_animated)
    {
        QLabel::setPixmap(QPixmap{});
    }
    else
    {
        QLabel::setPixmap(renderTile(first));
    }

    m_stillFrame=renderTile(first);
    m_paintFrame=m_stillFrame;
    m_frameDirty=false;

    if (wasAnimated!=m_animated)
    {
        emit animatedChanged(m_animated);
    }
    emit imageLoaded();

    syncPlayback();
    update();

    return true;
}

//--------------------------------------------------------------------------

void ImageLabel::createMovie()
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
        &ImageLabel::onFrameChanged
    );
    connect(
        m_movie.get(),
        &QMovie::error,
        this,
        &ImageLabel::onMovieError
    );
    connect(
        m_movie.get(),
        &QMovie::finished,
        this,
        &ImageLabel::onMovieFinished
    );

    // Apply the current scaled size directly here rather than going through applyScaledSize():
    // that function's CacheAll branch calls back into createMovie() to rebuild a stale cache,
    // which would recurse into this constructor indefinitely for a CacheAll movie.
    auto dev=imageSize();
    if (dev.isValid() && !dev.isNull())
    {
        m_movie->setScaledSize(dev);
    }
}

//--------------------------------------------------------------------------

void ImageLabel::applyScaledSize()
{
    if (!m_movie)
    {
        return;
    }

    auto dev=imageSize();
    if (!dev.isValid() || dev.isNull())
    {
        // Not laid out yet -- resizeEvent() will call us again once a real size is known.
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
            syncPlayback();
        }
        return;
    }

    m_movie->setScaledSize(dev);
}

//--------------------------------------------------------------------------

void ImageLabel::rebuildStills()
{
    if (m_fileName.isEmpty() && m_data.isEmpty())
    {
        return;
    }

    QImageReader reader;
    QBuffer buf;
    if (!m_fileName.isEmpty())
    {
        reader.setFileName(m_fileName);
    }
    else
    {
        buf.setBuffer(&m_data);
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

    auto first=reader.read();
    if (first.isNull())
    {
        return;
    }

    m_stillFrame=renderTile(first);
    if (!m_animated)
    {
        m_paintFrame=m_stillFrame;
        QLabel::setPixmap(m_stillFrame);
    }
}

//--------------------------------------------------------------------------

QPixmap ImageLabel::renderTile(const QImage& src) const
{
    if (src.isNull())
    {
        return QPixmap{};
    }

    const qreal dpr=qApp->primaryScreen()->devicePixelRatio();
    QSize dev=imageSize();
    if (!dev.isValid() || dev.isNull())
    {
        dev = (size().isValid() && !size().isEmpty()) ? size()*dpr : src.size();
    }
    if (!dev.isValid() || dev.isEmpty())
    {
        dev=src.size();
    }

    QImage scaled = (src.size()==dev) ? src : src.scaled(dev,m_aspectMode,Qt::SmoothTransformation);

    QImage tile(dev,QImage::Format_ARGB32_Premultiplied);
    tile.fill(Qt::transparent);
    {
        QPainter p(&tile);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawImage(
            QPoint((dev.width()-scaled.width())/2,(dev.height()-scaled.height())/2),
            scaled
        );
    }

    auto px=QPixmap::fromImage(tile);
    px.setDevicePixelRatio(dpr);
    return px;
}

//--------------------------------------------------------------------------

int ImageLabel::frameCount() const
{
    if (m_movie)
    {
        return m_movie->frameCount();
    }
    return m_stillFrame.isNull() ? 0 : 1;
}

//--------------------------------------------------------------------------

void ImageLabel::setAnimationMode(AnimationMode mode)
{
    if (m_mode==mode)
    {
        return;
    }
    m_mode=mode;
    m_manual=ManualState::Stopped;
    syncPlayback();
    update();
}

//--------------------------------------------------------------------------

void ImageLabel::setAnimationSpeed(int percent)
{
    m_speed=percent;
    if (m_movie)
    {
        m_movie->setSpeed(m_speed);
    }
}

//--------------------------------------------------------------------------

void ImageLabel::setAspectRatioMode(Qt::AspectRatioMode mode)
{
    if (m_aspectMode==mode)
    {
        return;
    }
    m_aspectMode=mode;
    rebuildStills();
    applyScaledSize();
    m_frameDirty=true;
    update();
}

//--------------------------------------------------------------------------

void ImageLabel::setCacheFrames(bool enable)
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
            syncPlayback();
        }
    }
}

//--------------------------------------------------------------------------

void ImageLabel::play()
{
    m_manual=ManualState::Playing;
    syncPlayback();
}

//--------------------------------------------------------------------------

void ImageLabel::pause()
{
    m_manual=ManualState::Paused;
    syncPlayback();
}

//--------------------------------------------------------------------------

void ImageLabel::stop()
{
    m_manual=ManualState::Stopped;
    syncPlayback();
}

//--------------------------------------------------------------------------

void ImageLabel::togglePlay()
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

bool ImageLabel::shouldPlay() const
{
    if (!m_animated || !m_movie)
    {
        return false;
    }
    if (m_pauseWhenHidden && !isVisible())
    {
        return false;
    }
    if (m_pauseWhenInactive && window() && !window()->isActiveWindow())
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
            return isEffectiveHovered();
        case AnimationMode::Manual:
            return m_manual==ManualState::Playing;
    }
    return false;
}

//--------------------------------------------------------------------------

bool ImageLabel::restOnFirstFrame() const
{
    if (m_mode==AnimationMode::Never)
    {
        return true;
    }
    if (m_mode==AnimationMode::OnHover)
    {
        return !isEffectiveHovered();
    }
    if (m_mode==AnimationMode::Manual)
    {
        return m_manual==ManualState::Stopped;
    }
    // Auto only ever stops because of hide/deactivate -- freeze in place and resume from there.
    return false;
}

//--------------------------------------------------------------------------

void ImageLabel::syncPlayback()
{
    if (m_inSync)
    {
        return;
    }
    m_inSync=true;

    if (m_animated && m_movie)
    {
        auto play=shouldPlay();
        if (play)
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
                m_paintFrame=m_stillFrame;
                m_frameDirty=false;
                update();
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

void ImageLabel::onFrameChanged(int frameNumber)
{
    std::ignore=frameNumber;

    m_frameDirty=true;
    if (!isVisible() || visibleRegion().isEmpty())
    {
        return;
    }
    update();
}

//--------------------------------------------------------------------------

void ImageLabel::onMovieError()
{
    if (m_movie)
    {
        emit imageLoadFailed(m_movie->lastErrorString());
    }
}

//--------------------------------------------------------------------------

void ImageLabel::onMovieFinished()
{
    emit animationFinished();

    auto running=m_movie && m_movie->state()==QMovie::Running;
    if (m_playing!=running)
    {
        m_playing=running;
        emit playingChanged(m_playing);
    }
}

//--------------------------------------------------------------------------

void ImageLabel::paintEvent(QPaintEvent* event)
{
    syncPlayback();

    if (!m_animated || !m_movie)
    {
        RoundedImage::paintEvent(event);
        return;
    }

    if (autoSize() && !isDeviceImageSizeEqual(size()))
    {
        // setImageSize() (base class) only updates imageSize()/m_size -- it does not know about
        // our QMovie, so re-apply the scaled size here too. Without this, the movie would keep
        // decoding at whatever size was last applied (possibly none, i.e. the natural size) until
        // the *next* resize, one extra unnecessarily-large/blurry decode per resize. renderTile()
        // below still produces a correctly-sized tile regardless -- this call is a performance
        // improvement, not a correctness requirement.
        setImageSize(size());
        applyScaledSize();
    }

    if (m_frameDirty)
    {
        m_paintFrame=renderTile(m_movie->currentImage());
        m_frameDirty=false;
    }

    auto px = m_paintFrame.isNull() ? m_stillFrame : m_paintFrame;
    if (px.isNull())
    {
        RoundedImage::paintEvent(event);
        return;
    }

    QPainter painter;
    painter.begin(this);
    painter.setRenderHints(QPainter::TextAntialiasing | QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    painter.setPen(Qt::NoPen);
    painter.setBrush(px);
    painter.drawRoundedRect(0,0,size().width(),size().height(),xRadius(),yRadius());
    doPaint(&painter);
    painter.end();
}

//--------------------------------------------------------------------------

void ImageLabel::resizeEvent(QResizeEvent* event)
{
    RoundedImage::resizeEvent(event);

    applyScaledSize();
    rebuildStills();
    m_frameDirty=true;
    update();
}

//--------------------------------------------------------------------------

void ImageLabel::showEvent(QShowEvent* event)
{
    RoundedImage::showEvent(event);
    syncPlayback();
}

//--------------------------------------------------------------------------

void ImageLabel::hideEvent(QHideEvent* event)
{
    RoundedImage::hideEvent(event);
    syncPlayback();
}

//--------------------------------------------------------------------------

void ImageLabel::enterEvent(QEnterEvent* event)
{
    RoundedImage::enterEvent(event);
    syncPlayback();
}

//--------------------------------------------------------------------------

void ImageLabel::leaveEvent(QEvent* event)
{
    RoundedImage::leaveEvent(event);
    syncPlayback();
}

//--------------------------------------------------------------------------

void ImageLabel::mousePressEvent(QMouseEvent* event)
{
    if (m_clickable && event->button()==Qt::LeftButton)
    {
        emit clicked();
        if (m_mode==AnimationMode::Manual)
        {
            togglePlay();
        }
    }
    RoundedImage::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void ImageLabel::changeEvent(QEvent* event)
{
    RoundedImage::changeEvent(event);

    // Window-activation events (WindowActivate/WindowDeactivate/ActivationChange) are not
    // reliably delivered to every descendant widget across Qt versions/platforms, so rather than
    // special-case those types here, just re-evaluate on every change event -- syncPlayback() is
    // idempotent and cheap, and shouldPlay() already re-checks window()->isActiveWindow() itself.
    syncPlayback();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
