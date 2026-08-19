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
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

#include <uise/desktop/imagelabel.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

//! ImageAnimator subclass reporting ImageLabel's own visibility/window-activation/hover state --
//! kept local to this translation unit so imagelabel.hpp does not need to expose it.
class LabelAnimator : public ImageAnimator
{
    public:

        explicit LabelAnimator(ImageLabel* owner)
            // No QObject parent: ImageLabel owns this exclusively via its own unique_ptr (see the
            // member-order/ownership comment on ImageLabel::m_animator and ImageAnimator's own
            // m_movie for why -- parenting to owner would register it as a QObject child too, and
            // ~QObject would then try to delete it a second time on top of the unique_ptr).
            : ImageAnimator(nullptr),
              m_owner(owner)
        {}

    protected:

        bool isWidgetVisible() const override
        {
            return m_owner->isVisible();
        }

        bool isWindowActive() const override
        {
            return !m_owner->window() || m_owner->window()->isActiveWindow();
        }

        bool isHovered() const override
        {
            return m_owner->isEffectiveHovered();
        }

    private:

        ImageLabel* m_owner;
};

}

/************************** ImageLabel *************************************/

//--------------------------------------------------------------------------

ImageLabel::ImageLabel(QWidget* parent, Qt::WindowFlags f)
    : RoundedImage(parent,f),
      m_aspectMode(DefaultAspectRatioMode),
      m_clickable(false),
      m_frameDirty(false)
{
    m_animator=std::make_unique<LabelAnimator>(this);

    connect(
        m_animator.get(),
        &ImageAnimator::frameChanged,
        this,
        &ImageLabel::onAnimatorFrameChanged
    );
    connect(
        m_animator.get(),
        &ImageAnimator::loaded,
        this,
        &ImageLabel::onAnimatorLoaded
    );
    connect(
        m_animator.get(),
        &ImageAnimator::loadFailed,
        this,
        &ImageLabel::onAnimatorLoadFailed
    );
    connect(
        m_animator.get(),
        &ImageAnimator::animatedChanged,
        this,
        &ImageLabel::onAnimatorAnimatedChanged
    );
    connect(
        m_animator.get(),
        &ImageAnimator::playingChanged,
        this,
        &ImageLabel::onAnimatorPlayingChanged
    );
    connect(
        m_animator.get(),
        &ImageAnimator::finished,
        this,
        &ImageLabel::animationFinished
    );
}

//--------------------------------------------------------------------------

ImageLabel::~ImageLabel()
{
    m_animator.reset();
}

//--------------------------------------------------------------------------

bool ImageLabel::setImageFile(const QString& fileName)
{
    resetContent();
    return m_animator->loadFile(fileName);
}

//--------------------------------------------------------------------------

bool ImageLabel::setImageData(const QByteArray& data, const QByteArray& format)
{
    resetContent();
    return m_animator->loadData(data,format);
}

//--------------------------------------------------------------------------

void ImageLabel::clearImage()
{
    m_animator->clear();
    resetContent();
    update();
}

//--------------------------------------------------------------------------

void ImageLabel::resetContent()
{
    m_stillFrame=QPixmap{};
    m_paintFrame=QPixmap{};
    m_frameDirty=false;

    QLabel::setPixmap(QPixmap{});
}

//--------------------------------------------------------------------------

void ImageLabel::onAnimatorLoaded()
{
    applyScaledSize();
    rebuildStills();
    m_frameDirty=false;

    emit imageLoaded();
    update();
}

//--------------------------------------------------------------------------

void ImageLabel::onAnimatorLoadFailed(const QString& error)
{
    resetContent();
    emit imageLoadFailed(error);
    update();
}

//--------------------------------------------------------------------------

void ImageLabel::onAnimatorAnimatedChanged(bool animated)
{
    emit animatedChanged(animated);
}

//--------------------------------------------------------------------------

void ImageLabel::onAnimatorPlayingChanged(bool playing)
{
    emit playingChanged(playing);
}

//--------------------------------------------------------------------------

void ImageLabel::applyScaledSize()
{
    auto dev=imageSize();
    if (!dev.isValid() || dev.isNull())
    {
        // Not laid out yet -- resizeEvent() will call us again once a real size is known.
        return;
    }

    m_animator->setScaledSize(dev);
}

//--------------------------------------------------------------------------

void ImageLabel::rebuildStills()
{
    auto first=m_animator->firstFrame();
    if (first.isNull())
    {
        return;
    }

    m_stillFrame=renderTile(first);
    if (!m_animator->isAnimated())
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

void ImageLabel::setAnimationMode(AnimationMode mode)
{
    m_animator->setAnimationMode(mode);
    update();
}

//--------------------------------------------------------------------------

void ImageLabel::setAnimationSpeed(int percent)
{
    m_animator->setAnimationSpeed(percent);
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
    m_animator->setCacheFrames(enable);
}

//--------------------------------------------------------------------------

void ImageLabel::play()
{
    m_animator->play();
}

//--------------------------------------------------------------------------

void ImageLabel::pause()
{
    m_animator->pause();
}

//--------------------------------------------------------------------------

void ImageLabel::stop()
{
    m_animator->stop();
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

void ImageLabel::onAnimatorFrameChanged()
{
    m_frameDirty=true;
    if (!isVisible() || visibleRegion().isEmpty())
    {
        return;
    }
    update();
}

//--------------------------------------------------------------------------

void ImageLabel::paintEvent(QPaintEvent* event)
{
    m_animator->sync();

    if (!m_animator->isAnimated())
    {
        RoundedImage::paintEvent(event);
        return;
    }

    if (autoSize() && !isDeviceImageSizeEqual(size()))
    {
        // setImageSize() (base class) only updates imageSize()/m_size -- it does not know about
        // our animator, so re-apply the scaled size here too. Without this, the movie would keep
        // decoding at whatever size was last applied (possibly none, i.e. the natural size) until
        // the *next* resize, one extra unnecessarily-large/blurry decode per resize. renderTile()
        // below still produces a correctly-sized tile regardless -- this call is a performance
        // improvement, not a correctness requirement.
        setImageSize(size());
        applyScaledSize();
    }

    if (m_frameDirty)
    {
        m_paintFrame=renderTile(m_animator->currentFrame());
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
    painter.setOpacity(m_contentOpacity);
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
    m_animator->sync();
}

//--------------------------------------------------------------------------

void ImageLabel::hideEvent(QHideEvent* event)
{
    RoundedImage::hideEvent(event);
    m_animator->sync();
}

//--------------------------------------------------------------------------

void ImageLabel::enterEvent(QEnterEvent* event)
{
    RoundedImage::enterEvent(event);
    m_animator->sync();
}

//--------------------------------------------------------------------------

void ImageLabel::leaveEvent(QEvent* event)
{
    RoundedImage::leaveEvent(event);
    m_animator->sync();
}

//--------------------------------------------------------------------------

void ImageLabel::mousePressEvent(QMouseEvent* event)
{
    if (m_clickable && event->button()==Qt::LeftButton)
    {
        if (m_dragEnabled)
        {
            // Accept the press instead of emitting clicked() right away -- a release only
            // reaches the widget that accepted the press, so this is what makes
            // mouseMoveEvent()/mouseReleaseEvent() below see the rest of the gesture.
            m_dragGesture.press(event->pos());
            emit dragPrepareRequested();
            event->accept();
            return;
        }

        emit clicked();
        if (m_animator->animationMode()==AnimationMode::Manual)
        {
            togglePlay();
        }
    }
    RoundedImage::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void ImageLabel::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragEnabled && m_dragGesture.isArmed() && (event->buttons() & Qt::LeftButton))
    {
        if (m_dragGesture.movedPastThreshold(event->pos()))
        {
            emit dragStartRequested();
        }
        event->accept();
        return;
    }
    RoundedImage::mouseMoveEvent(event);
}

//--------------------------------------------------------------------------

void ImageLabel::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_dragEnabled && m_clickable && event->button()==Qt::LeftButton && m_dragGesture.isArmed())
    {
        if (m_dragGesture.releaseIsClick())
        {
            emit clicked();
            if (m_animator->animationMode()==AnimationMode::Manual)
            {
                togglePlay();
            }
        }
        m_dragGesture.reset();
        event->accept();
        return;
    }
    RoundedImage::mouseReleaseEvent(event);
}

//--------------------------------------------------------------------------

void ImageLabel::changeEvent(QEvent* event)
{
    RoundedImage::changeEvent(event);

    // Window-activation events (WindowActivate/WindowDeactivate/ActivationChange) are not
    // reliably delivered to every descendant widget across Qt versions/platforms, so rather than
    // special-case those types here, just re-evaluate on every change event -- sync() is
    // idempotent and cheap, and shouldPlay() already re-checks isWindowActive() itself.
    m_animator->sync();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
