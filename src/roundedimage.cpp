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
        std::shared_ptr<RoundedImageSource> source,
        WithPath path,
        QSize size
    )
{
    m_imageSource=std::move(source);
    createPixmapConsumer(std::move(path),size);
}

//--------------------------------------------------------------------------

void RoundedImage::createPixmapConsumer(
        WithPath path,
        QSize size
    )
{
    if (m_pixmapConsumer!=nullptr)
    {
        delete m_pixmapConsumer;
    }

    m_pixmapConsumer=new PixmapConsumer(std::move(path),size,this);
    m_pixmapConsumer->setPixmapSource(m_imageSource);

    if (!size.isNull())
    {
        setFixedSize(size);
    }
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
            createPixmapConsumer(m_pixmapConsumer->path(),size());
            px=m_pixmapConsumer->pixmapProducer()->pixmap();
        }

        px=px.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }

    QBrush brush(px);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(brush);
    painter.setPen(Qt::transparent);
    painter.drawRoundedRect(0, 0, width(), height(), m_imageSource->evalXRadius(width()), m_imageSource->evalYRadius(height()));
    QLabel::paintEvent(event);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
