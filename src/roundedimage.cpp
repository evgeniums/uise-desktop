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
    m_size=size;
    setFixedSize(m_size);
    createPixmapConsumer();
}

//--------------------------------------------------------------------------

void RoundedImage::createPixmapConsumer()
{
    if (m_pixmapConsumer!=nullptr)
    {
        delete m_pixmapConsumer;
        m_pixmapConsumer=nullptr;
    }
    if (!m_imageSource || path().empty())
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
    if (m_size!=size() && m_autoSize)
    {
        setImageSize(size());
    }

    QPainter painter(this);
    painter.setRenderHints(QPainter::TextAntialiasing | QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    if (m_svgIcon)
    {
        // paint svg icon
        m_svgIcon->paint(&painter,rect(),currentSvgIconMode(),QIcon::Off,m_cacheSvgPixmap);
    }
    else
    {
        // paint pixmap
        auto px=pixmap();
        bool hasPixmap=!px.isNull();
        if (!hasPixmap && m_pixmapConsumer!=nullptr)
        {
            px=m_pixmapConsumer->pixmapProducer()->pixmap();
        }
        if (!px.isNull() && px.size()!=m_size)
        {
            px=px.scaled(m_size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        }
        doFill(&painter,px);
    }

    // add extra painting in derived class
    doPaint(&painter);

    // done
    QLabel::paintEvent(event);
}

//--------------------------------------------------------------------------

void RoundedImage::doFill(QPainter* painter, const QPixmap& pixmap)
{
    QBrush brush(pixmap);
    painter->setBrush(brush);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(0, 0, m_size.width(), m_size.height(), xRadius(), yRadius());
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

UISE_DESKTOP_NAMESPACE_END
