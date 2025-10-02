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
      m_pixmapConsumer(nullptr)
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

void RoundedImage::paintEvent(QPaintEvent *event)
{
    auto px=pixmap();
    bool hasPixmap=!px.isNull();

    if (!hasPixmap && m_pixmapConsumer!=nullptr)
    {
        px=m_pixmapConsumer->pixmapProducer()->pixmap();
    }

    if (!px.isNull() && px.size()!=size())
    {
        if (m_pixmapConsumer!=nullptr)
        {
            setImageSize(size());
            createPixmapConsumer();
            px=m_pixmapConsumer->pixmapProducer()->pixmap();
        }

        px=px.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }

    int xRadius=width()/2;
    int yRadius=height()/2;
    if (m_imageSource)
    {
        xRadius=m_imageSource->evalXRadius(width());
        yRadius=m_imageSource->evalYRadius(height());
    }

    QBrush brush(px);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(brush);
    painter.setPen(Qt::transparent);
    painter.drawRoundedRect(0, 0, width(), height(), xRadius, yRadius);
    doPaint(&painter);
    QLabel::paintEvent(event);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
