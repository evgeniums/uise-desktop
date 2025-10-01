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
    : QLabel(parent,f)
{}

//--------------------------------------------------------------------------

void RoundedImage::setPixmapSource(
        std::shared_ptr<PixmapSource> source,
        QString name,
        QSize size
    )
{
    m_pixmapConsumer=new PixmapConsumer(std::move(name),size,this);
    m_pixmapConsumer->setPixmapSource(std::move(source));
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
            auto prevConsumer=m_pixmapConsumer;
            auto source=prevConsumer->pixmapSource();
            setPixmapSource(std::move(source),prevConsumer->name(),size());
            delete prevConsumer;

            px=m_pixmapConsumer->pixmapProducer()->pixmap();
        }

        px=px.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }

    QBrush brush(px);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(brush);
    painter.drawRoundedRect(0, 0, width(), height(), width()/2, height()/2);
    QLabel::paintEvent(event);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

#if 0

// name generation

#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QLabel>
#include <QPixmap>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // 1. Create a QImage
    int width = 300;
    int height = 100;
    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::lightGray); // Fill with a background color

    // 2. Draw the name on the image
    QPainter painter(&image);
    painter.setPen(Qt::darkBlue); // Set text color
    QFont font("Verdana", 24, QFont::Bold); // Set font, size, and weight
    painter.setFont(font);

    QString nameToDisplay = "Qt C++ Image";
    painter.drawText(image.rect(), Qt::AlignCenter, nameToDisplay); // Draw the name centered
    painter.end(); // End painting

    // 3. Display the image in a QLabel
    QLabel *label = new QLabel();
    label->setPixmap(QPixmap::fromImage(image));
    label->setWindowTitle("Generated Image");
    label->show();

    // Optionally, save the image
    image.save("generated_name_image.png", "PNG");

    return a.exec();
}
#endif
