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

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/roundedimage.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************** RoundedImage **********************************/

//--------------------------------------------------------------------------

RoundedImage::RoundedImage(QWidget *parent, Qt::WindowFlags f)
    : QLabel(parent,f),
      m_pixmapConsumer(nullptr),
      m_autoSize(true),
      m_hovered(false),
      m_parentHovered(false),
      m_selected(false),
      m_cacheSvgPixmap(true),
      m_autoFitEllipse(false)
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
    return m_size==sz;
}

//--------------------------------------------------------------------------

void RoundedImage::createPixmapConsumer()
{
    if (m_pixmapConsumer!=nullptr)
    {
        delete m_pixmapConsumer;
        m_pixmapConsumer=nullptr;
    }
    if (!m_imageSource || path().empty() || !m_size.isValid() || m_size.isNull())
    {
        return;
    }

    m_pixmapConsumer=new PixmapConsumer(path(),m_size,this);
    m_pixmapConsumer->setPixmapSource(m_imageSource);
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
    if (m_selected)
    {
        mode=IconMode::Checked;
    }
    else if (m_hovered||m_parentHovered)
    {
        mode=IconMode::Hovered;
    }
    return mode;
}

//--------------------------------------------------------------------------

void RoundedImage::paintEvent(QPaintEvent *event)
{
    // update image size
    if (!isDeviceImageSizeEqual(size()) && m_autoSize)
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
    if (px.isNull() && m_pixmapConsumer!=nullptr)
    {
        // use pixmap from pixmap consumer
        px=m_pixmapConsumer->pixmapProducer()->pixmap();
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
    m_hovered=true;
    QFrame::enterEvent(event);
    update();
}

//--------------------------------------------------------------------------

void RoundedImage::leaveEvent(QEvent* event)
{
    if (m_parentHovered)
    {
        return;
    }

    m_hovered=false;
    QFrame::leaveEvent(event);
    update();
}

//--------------------------------------------------------------------------

void RoundedImage::setParentHovered(bool enable)
{
    m_parentHovered=enable;
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
