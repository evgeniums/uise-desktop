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

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/datetime.hpp>
#include <uise/desktop/roundedimage.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************** RoundedImage **********************************/

//--------------------------------------------------------------------------

RoundedImage::RoundedImage(QWidget *parent, Qt::WindowFlags f)
    : QLabel(parent,f),
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
    const qreal pixelRatio = qApp->primaryScreen()->devicePixelRatio();
    m_size=size * pixelRatio;
    setFixedSize(size);
    createPixmapConsumer();
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

void RoundedImage::paintEvent(QPaintEvent *event)
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

    if (px.isNull() && m_svgIcon)
    {
        // use svg icon
        px=m_svgIcon->pixmap(m_size,currentSvgIconMode());
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

    // draw pixmap
    if (!px.isNull())
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
        QFrame::leaveEvent(event);
        event->accept();
        update();
        return;
    }

    QFrame::leaveEvent(event);
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
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
