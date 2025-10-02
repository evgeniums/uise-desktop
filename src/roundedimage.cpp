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

#include <uise/desktop/roundedimage.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************** RoundedImage **********************************/

//--------------------------------------------------------------------------

RoundedImage::RoundedImage(QWidget *parent, Qt::WindowFlags f)
    : QLabel(parent,f),
      m_pixmapConsumer(nullptr),
      m_xRadius(0),
      m_yRadius(0)
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
    if (m_xRadius!=0)
    {
        return m_xRadius;
    }

    int val=width()/2;
    if (m_imageSource)
    {
        val=m_imageSource->evalXRadius(width());
    }
    return val;
}

//--------------------------------------------------------------------------

int RoundedImage::yRadius() const
{
    if (m_yRadius!=0)
    {
        return m_yRadius;
    }

    int val=height()/2;
    if (m_imageSource)
    {
        val=m_imageSource->evalYRadius(height());
    }
    return val;
}

//--------------------------------------------------------------------------

void RoundedImage::paintEvent(QPaintEvent *event)
{
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

    QPainter painter(this);
    painter.setRenderHints(QPainter::TextAntialiasing | QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    doFill(&painter,px);

    doPaint(&painter);
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

UISE_DESKTOP_NAMESPACE_END
