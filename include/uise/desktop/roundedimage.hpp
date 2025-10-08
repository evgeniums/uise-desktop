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
            return m_xRadius.value_or(0);
        }

        void resetXRadius()
        {
            m_xRadius.reset();
        }

        void setYRadius(int value) noexcept
        {
            m_yRadius=value;
        }

        int yRadius() const noexcept
        {
            return m_yRadius.value_or(0);
        }

        void resetYRadius()
        {
            m_yRadius.reset();
        }

        int evalXRadius(int width) const noexcept
        {
            if (!m_xRadius)
            {
                if (m_radiusRatio)
                {
                    return qRound(m_radiusRatio.value()*width);
                }

                return width/2;
            }
            return m_xRadius.value();
        }

        int evalYRadius(int height) const noexcept
        {
            if (!m_yRadius)
            {
                if (m_radiusRatio)
                {
                    return qRound(m_radiusRatio.value()*height);
                }

                return height/2;
            }
            return m_yRadius.value();
        }

        void setRadiusRatio(double ratio) noexcept
        {
            m_radiusRatio=ratio;
        }

        void resetRadiusRatio() noexcept
        {
            m_radiusRatio.reset();
        }

    private:

        std::optional<int> m_xRadius;
        std::optional<int> m_yRadius;

        std::optional<double> m_radiusRatio;
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

        QSize imageSize() const
        {
            return m_size;
        }

        void setAutoSize(bool enable) noexcept
        {
            m_autoSize=enable;
        }

        bool autoSize() const noexcept
        {
            return m_autoSize;
        }

        void setText(const QString&)=delete;
        QString text() const=delete;

        //! @todo Implement configurable circle border

        int xRadius() const;
        int yRadius() const;

        void setCornersRadius(int x, int y)
        {
            m_xRadius=x;
            m_yRadius=y;
            update();
        }

        void resetCornersRadius()
        {
            m_xRadius.reset();
            m_yRadius.reset();
        }

        void setSvgIcon(std::shared_ptr<SvgIcon> svgIcon)
        {
            m_svgIcon=std::move(svgIcon);
            update();
        }

        std::shared_ptr<SvgIcon> svgIcon() const
        {
            return m_svgIcon;
        }

        void setParentHovered(bool enable);

        bool isParentHovered() const noexcept
        {
            return m_parentHovered;
        }

        void setSelected(bool enable)
        {
            m_selected=enable;
            update();
        }

        bool isSelected() const noexcept
        {
            return m_selected;
        }

        void setCacheSvgPixmap(bool enable) noexcept
        {
            m_cacheSvgPixmap=enable;
        }

        bool isCacheSvgPixmap() const noexcept
        {
            return m_cacheSvgPixmap;
        }

        void setAutoFitEllipse(bool enable)
        {
            m_autoFitEllipse=enable;
            update();
        }

        bool isAutoFitEllipse() const noexcept
        {
            return m_autoFitEllipse;
        }

        bool isEffectiveHovered() const
        {
            return m_parentHovered || m_hovered;
        }

        IconMode currentSvgIconMode() const;

    protected:

        void paintEvent(QPaintEvent *event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

        virtual void doPaint(QPainter*)
        {}

        virtual void doFill(QPainter* painter, const QPixmap& pixmap);

    private:

        void createPixmapConsumer();

        PixmapConsumer* m_pixmapConsumer;
        std::shared_ptr<RoundedImageSource> m_imageSource;
        QSize m_size;

        std::optional<int> m_xRadius;
        std::optional<int> m_yRadius;

        bool m_autoSize;

        std::shared_ptr<SvgIcon> m_svgIcon;
        bool m_hovered;
        bool m_parentHovered;
        bool m_selected;
        bool m_cacheSvgPixmap;
        bool m_autoFitEllipse;
};

class UISE_DESKTOP_EXPORT WithRoundedImage : public QFrame
{
    Q_OBJECT

    public:

        explicit WithRoundedImage(QWidget *parent=nullptr);

        RoundedImage* image() const noexcept
        {
            return m_img;
        }

    private:

        RoundedImage* m_img;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ROUNDED_IMAGE_HPP
