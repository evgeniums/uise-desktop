/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/roundedimagelabel.hpp
*
*  Declares round label widget.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ROUNDED_IMAGE_HPP
#define UISE_DESKTOP_ROUNDED_IMAGE_HPP

#include <QLabel>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/pixmapproducer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT RoundedImageSource : public PixmapSource
{
    public:

        using PixmapSource::PixmapSource;

        void setXRadius(int value) noexcept
        {
            m_xRadius=value;
        }

        int xRadius() const noexcept
        {
            return m_xRadius;
        }

        void setYRadius(int value) noexcept
        {
            m_yRadius=value;
        }

        int yRadius() const noexcept
        {
            return m_yRadius;
        }

        int evalXRadius(int width) const noexcept
        {
            if (m_xRadius==0)
            {
                return width/2;
            }
            return m_xRadius;
        }

        int evalYRadius(int height) const noexcept
        {
            if (m_yRadius==0)
            {
                return height/2;
            }
            return m_yRadius;
        }

    private:

        int m_xRadius=0;
        int m_yRadius=0;
};

class UISE_DESKTOP_EXPORT RoundedImage : public QLabel,
                                         public WithPath
{
    Q_OBJECT

    public:

        explicit RoundedImage(QWidget *parent=nullptr, Qt::WindowFlags f=Qt::WindowFlags());

        void setImageSource(
            std::shared_ptr<RoundedImageSource> source
        );

        void setImageSource(
            std::shared_ptr<RoundedImageSource> source,
            WithPath path,
            const QSize& size={}
        );

        void setImagePath(
            WithPath path
        );

        void setImageSize(
            const QSize& size
        );

        void setText(const QString&)=delete;
        QString text() const=delete;

        //! @todo Implement configurable circle border

    protected:

        void paintEvent(QPaintEvent *event) override;

        virtual void doPaint(QPainter*)
        {}

    private:

        void createPixmapConsumer();

        PixmapConsumer* m_pixmapConsumer;
        std::shared_ptr<RoundedImageSource> m_imageSource;
        QSize m_size;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ROUNDED_IMAGE_HPP
