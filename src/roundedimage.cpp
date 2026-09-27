/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/roundedimagel.cpp
*
*  Defines round pixmap label.
*
*/

/****************************************************************************/

#include <QPainter>
#include <QStyle>
#include <QGuiApplication>
#include <QScreen>
#include <QEnterEvent>
#include <QEvent>

#include <iostream>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/datetime.hpp>
#include <uise/desktop/roundedimage.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

// COMPOSER-DEBUG temporary (Windows composer render glitch): only two groups of RoundedImages
// are traced, to keep the output readable:
//  - those inside a MessageEditor (the composer's attach/emoji/mic/send/expand icons, plus the
//    formatting toolbar's);
//  - the icon of an IconTextButton named "add" (the Add buttons beside the chats/contacts search
//    input), which show the same unpainted-icon symptom.
// Every other RoundedImage in the app (avatars, list icons, ...) stays silent. Returns an empty
// string for an untraced widget, otherwise its ancestor chain as
// "Class#objectName/Class#objectName/..." (innermost last), cut at the MessageEditor if any.
std::string composerDebugPath(const QWidget* w)
{
    QStringList parts;
    bool traced=false;
    for (auto* p=w; p!=nullptr; p=p->parentWidget())
    {
        parts.prepend(QStringLiteral("%1#%2").arg(QString::fromLatin1(p->metaObject()->className()),p->objectName()));
        if (p->inherits("uise::IconTextButton") && p->objectName()==QLatin1String("add"))
        {
            traced=true;
        }
        if (p->inherits("uise::MessageEditor"))
        {
            traced=true;
            break;
        }
    }
    if (!traced)
    {
        return std::string{};
    }
    return parts.join(QLatin1Char('/')).toStdString();
}

}


/************************** RoundedImage **********************************/

//--------------------------------------------------------------------------

RoundedImage::RoundedImage(QWidget *parent, Qt::WindowFlags f)
    : QFrame(parent,f),
      m_pixmapConsumer(nullptr),
      m_prevPixmapConsumer(nullptr),
      m_autoSize(true),
      m_hovered(false),
      m_parentHovered(false),
      m_selected(false),
      m_cacheSvgPixmap(true),
      m_autoFitEllipse(false),
      m_disableHover(false)
{}

//--------------------------------------------------------------------------

void RoundedImage::setImageSource(
        std::shared_ptr<RoundedImageSource> source
    )
{
    m_imageSource=std::move(source);
    createPixmapConsumer();
}

//--------------------------------------------------------------------------

void RoundedImage::setImageSource(
        std::shared_ptr<RoundedImageSource> source,
        WithPath path,
        const QSize& size
    )
{
    setImagePath(std::move(path));
    if (size.isValid())
    {
        setImageSize(size);
    }
    setImageSource(std::move(source));
}

//--------------------------------------------------------------------------

void RoundedImage::setImagePath(
        WithPath path
    )
{
    setPath(std::move(path));
    createPixmapConsumer();
}

//--------------------------------------------------------------------------

void RoundedImage::setImageSize(
        const QSize& size
    )
{
    // COMPOSER-DEBUG temporary (Windows composer render glitch): this is where an autoSize image
    // locks its size (setFixedSize) -- log what it locks to and what min/max it overrides.
    {
        auto path=composerDebugPath(this);
        if (!path.empty())
        {
            std::cerr << "COMPOSER-DEBUG icon " << static_cast<const void*>(this) << " setImageSize"
                      << " to=" << size.width() << "x" << size.height()
                      << " minBefore=" << minimumWidth() << "x" << minimumHeight()
                      << " maxBefore=" << maximumWidth() << "x" << maximumHeight()
                      << " path=" << path << std::endl;
        }
    }

    const qreal pixelRatio = qApp->primaryScreen()->devicePixelRatio();
    m_size=size * pixelRatio;
    setFixedSize(size);
    createPixmapConsumer();
}

//--------------------------------------------------------------------------

void RoundedImage::setPixmap(const QPixmap& pixmap)
{
    m_pixmap=pixmap;
    update();
}

//--------------------------------------------------------------------------

bool RoundedImage::isDeviceImageSizeEqual(const QSize& other) const
{
    const qreal pixelRatio = qApp->primaryScreen()->devicePixelRatio();
    auto sz=other*pixelRatio;
    return m_size.width()==sz.width() && m_size.height()==sz.height();
}

//--------------------------------------------------------------------------

void RoundedImage::createPixmapConsumer()
{
    auto minSize=minimumSize();
#if 0
    qDebug() << "RoundedImage::createPixmapConsumer() minSize="<<minSize << " m_size="<<m_size
                       << " path=" << toWithPath().toString()  << " " << printCurrentDateTime();
#endif
    if ((m_size.isNull() || !m_size.isValid()) && minSize.isValid())
    {
        const qreal pixelRatio = qApp->primaryScreen()->devicePixelRatio();
        m_size=minSize*pixelRatio;
    }

    if (m_pixmapConsumer!=nullptr)
    {
        if (m_size==m_pixmapConsumer->size() && path()==m_pixmapConsumer->path())
        {
#if 0
            qDebug() << "RoundedImage::createPixmapConsumer() use existing consumer m_size="<<m_size
                               << " path=" << toWithPath().toString() << " " << printCurrentDateTime();
#endif
            update();
            return;
        }
#if 0
        qDebug() << "RoundedImage::createPixmapConsumer() destroy existing consumer m_size="<<m_size
                           << " path=" << toWithPath().toString()
                           << " consumerSize=" << m_pixmapConsumer->size()
                           << " consumerPath="<<m_pixmapConsumer->toWithPath().toString()
                            << " " << printCurrentDateTime();
#endif
        m_prevPixmapConsumer=m_pixmapConsumer;
    }

    if (!m_imageSource || path().empty() || !m_size.isValid() || m_size.isNull())
    {
#if 0
        qDebug() << "RoundedImage::createPixmapConsumer() not ready" << " " << printCurrentDateTime();
#endif
        // m_prevPixmapConsumer may have just been aliased to m_pixmapConsumer above (same
        // pointer) when there is no legitimate new consumer to transition to here — clear the
        // alias first so it is not left dangling once m_pixmapConsumer is deleted below, and so
        // a later swap (or ~RoundedImage) does not double-delete it.
        if (m_prevPixmapConsumer==m_pixmapConsumer)
        {
            m_prevPixmapConsumer=nullptr;
        }
        delete m_pixmapConsumer;
        m_pixmapConsumer=nullptr;
        return;
    }
#if 0
    qDebug() << "RoundedImage::createPixmapConsumer() create new consumer m_size="<<m_size
        << " path=" << toWithPath().toString() << " " << printCurrentDateTime();
#endif
    m_pixmapConsumer=new PixmapConsumer(toWithPath(),m_size,this);
    m_pixmapConsumer->setPixmapSource(m_imageSource);
    connect(
        m_pixmapConsumer,
        &PixmapConsumer::pixmapUpdated,
        this,
        &RoundedImage::onPixmapUpdated
    );
    connect(
        m_pixmapConsumer,
        &PixmapConsumer::dataUpdated,
        this,
        [this]()
        {
            if (m_pixmapConsumer->pixmapProducer()!=nullptr)
            {
                emit producerDataUpdated(m_pixmapConsumer->pixmapProducer()->data());
            }
        }
    );
    if (m_pixmapConsumer->pixmapProducer()!=nullptr)
    {
        emit producerDataUpdated(m_pixmapConsumer->pixmapProducer()->data());

        if (!m_pixmapConsumer->pixmapProducer()->pixmap().isNull())
        {
            delete m_prevPixmapConsumer;
            m_prevPixmapConsumer=nullptr;
            update();
        }
    }
}

//--------------------------------------------------------------------------

int RoundedImage::xRadius() const
{
    if (m_xRadius)
    {
        return m_xRadius.value();
    }

    int val=0;
    if (m_imageSource)
    {
        val=m_imageSource->evalXRadius(width());
    }
    else if (m_autoFitEllipse)
    {
        val=width()/2;
    }
    return val;
}

//--------------------------------------------------------------------------

int RoundedImage::yRadius() const
{
    if (m_yRadius)
    {
        return m_yRadius.value();
    }

    int val=0;
    if (m_imageSource)
    {
        val=m_imageSource->evalYRadius(height());
    }
    else if (m_autoFitEllipse)
    {
        val=height()/2;
    }
    return val;
}

//--------------------------------------------------------------------------

IconMode RoundedImage::currentSvgIconMode() const
{
    if (!isEnabled())
    {
        return IconMode::Disabled;
    }

    auto mode=IconMode::Normal;
    if (m_hovered||m_parentHovered)
    {
        if (m_selected)
        {
            mode=IconMode::CheckedHovered;
        }
        else
        {
            mode=IconMode::Hovered;
        }
    }
    else if (m_selected)
    {
        mode=IconMode::Checked;
    }
    return mode;
}

//--------------------------------------------------------------------------

void RoundedImage::paintEvent(QPaintEvent* /*event*/)
{
    // update image size
    auto imageSizeMatch=isDeviceImageSizeEqual(size());
    if (!imageSizeMatch && m_autoSize)
    {
        setImageSize(size());
    }

    QPainter painter;
    painter.begin(this);
    painter.setRenderHints(QPainter::TextAntialiasing | QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    QPixmap px=pixmap();

    // See setSvgIconSize()'s own doc comment -- a valid size there means the icon is a small
    // fixed-size glyph to be centered, not a fill-the-whole-shape placeholder like Avatar's.
    bool svgIconCentered=false;
    if (px.isNull() && m_svgIcon)
    {
        if (m_svgIconSize.isValid())
        {
            svgIconCentered=true;
            const qreal pixelRatio=qApp->primaryScreen()->devicePixelRatio();
            px=m_svgIcon->pixmap(m_svgIconSize*pixelRatio,currentSvgIconMode());
            px.setDevicePixelRatio(pixelRatio);
        }
        else
        {
            // use svg icon, filling the whole shape
            px=m_svgIcon->pixmap(m_size,currentSvgIconMode());
        }
    }
    if (px.isNull())
    {
        if (m_pixmapConsumer!=nullptr)
        {
            // use pixmap from pixmap consumer
            px=m_pixmapConsumer->pixmapProducer()->pixmap();
        }
        if (px.isNull() && m_prevPixmapConsumer!=nullptr)
        {
            px=m_prevPixmapConsumer->pixmapProducer()->pixmap();
        }
    }

    // COMPOSER-DEBUG temporary (Windows composer render glitch): first paint of each traced icon,
    // and every paint that ends up drawing nothing (null pixmap). A property rather than a member
    // keeps this out of the class layout.
    {
        const bool firstPaint=!property("_composerDebugPainted").toBool();
        if (firstPaint || px.isNull())
        {
            auto path=composerDebugPath(this);
            if (!path.empty())
            {
                setProperty("_composerDebugPainted",true);
                std::cerr << "COMPOSER-DEBUG icon " << static_cast<const void*>(this)
                          << (firstPaint ? " firstPaint" : " paint")
                          << " size=" << width() << "x" << height()
                          << " imageSize=" << m_size.width() << "x" << m_size.height()
                          << " pixmapNull=" << int(px.isNull())
                          << " hasSvg=" << int(static_cast<bool>(m_svgIcon))
                          << " path=" << path << std::endl;
            }
        }
    }

    // draw pixmap
    if (!px.isNull() && svgIconCentered)
    {
        painter.setPen(Qt::NoPen);
        QRect target(QPoint(0,0),px.deviceIndependentSize().toSize());
        target.moveCenter(rect().center());
        painter.drawPixmap(target,px);
    }
    else if (!px.isNull())
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(px);
        painter.drawRoundedRect(0,0,size().width(),size().height(),xRadius(),yRadius());
    }
    else
    {
        // pixmap is null, try to fill the image in derived class
        fillIfNoPixmap(&painter);
    }

    // add extra painting in derived class
    doPaint(&painter);

    // done
    painter.end();
}

//--------------------------------------------------------------------------

void RoundedImage::enterEvent(QEnterEvent* event)
{
    if (m_disableHover)
    {
        QFrame::enterEvent(event);
        return;
    }

    if (!m_parentHovered)
    {
        m_hovered=true;
        event->accept();
        update();
        return;
    }

    QFrame::enterEvent(event);
}

//--------------------------------------------------------------------------

void RoundedImage::leaveEvent(QEvent* event)
{
    if (m_disableHover)
    {
        QFrame::leaveEvent(event);
        return;
    }

    if (!m_parentHovered)
    {
        m_hovered=false;
        QFrame::leaveEvent(event);
        event->accept();
        update();
        return;
    }

    QFrame::leaveEvent(event);
}

//--------------------------------------------------------------------------

void RoundedImage::changeEvent(QEvent* event)
{
    QFrame::changeEvent(event);

    // When the effective style changes (e.g. a QSS repolish applied min/max-width
    // after this widget was inserted into the tree), the size derived from
    // minimumSize() in createPixmapConsumer() may now differ. Rebuild the pixmap
    // consumer so the icon is rendered at the freshly-applied size. The call is
    // idempotent: it reuses the existing consumer when size and path are unchanged.
    if (event->type()==QEvent::StyleChange)
    {
        createPixmapConsumer();
    }
}

//--------------------------------------------------------------------------

bool RoundedImage::event(QEvent* event)
{
    // COMPOSER-DEBUG temporary (Windows composer render glitch). Logged AFTER the base handler,
    // so a Polish line already shows the min/max the stylesheet applied (or failed to apply).
    auto result=QFrame::event(event);

    const char* name=nullptr;
    switch (event->type())
    {
        case QEvent::Polish: name="Polish"; break;
        case QEvent::StyleChange: name="StyleChange"; break;
        case QEvent::ParentChange: name="ParentChange"; break;
        case QEvent::Resize: name="Resize"; break;
        case QEvent::Show: name="Show"; break;
        case QEvent::Hide: name="Hide"; break;
        default: break;
    }
    if (name!=nullptr)
    {
        auto path=composerDebugPath(this);
        if (!path.empty())
        {
            std::cerr << "COMPOSER-DEBUG icon " << static_cast<const void*>(this) << " " << name
                      << " size=" << width() << "x" << height()
                      << " min=" << minimumWidth() << "x" << minimumHeight()
                      << " max=" << maximumWidth() << "x" << maximumHeight()
                      << " imageSize=" << m_size.width() << "x" << m_size.height()
                      << " polished=" << int(testAttribute(Qt::WA_WState_Polished))
                      << " visible=" << int(isVisible())
                      << " path=" << path << std::endl;
        }
    }
    return result;
}

//--------------------------------------------------------------------------

void RoundedImage::setParentHovered(bool enable)
{
    m_parentHovered=enable;
    update();
}

//--------------------------------------------------------------------------

void RoundedImage::onPixmapUpdated()
{
    if (m_prevPixmapConsumer!=nullptr)
    {
        delete m_prevPixmapConsumer;
        m_prevPixmapConsumer=nullptr;
    }

    update();
}

//--------------------------------------------------------------------------

WithRoundedImage::WithRoundedImage(QWidget *parent)
    : QFrame(parent)
{
    m_img=new RoundedImage(this);
    auto l=Layout::vertical(this);
    l->addWidget(m_img);
    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
