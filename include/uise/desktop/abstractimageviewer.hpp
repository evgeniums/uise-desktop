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

/** @file uise/desktop/abstractimageviewer.hpp
*
*  Declares AbstractImageViewer.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACT_IMAGE_VIEWER_HPP
#define UISE_DESKTOP_ABSTRACT_IMAGE_VIEWER_HPP

#include <vector>
#include <map>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/widget.hpp>
#include <uise/desktop/utils/withpathandsize.hpp>
#include <uise/desktop/pixmapproducer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT AbstractImageViewer : public WidgetController
{
    Q_OBJECT

    public:

        struct Image
        {
            PixmapKey key;
            QPixmap content;

            Image(PixmapKey key={}, QPixmap content={}) : key(std::move(key)), content(std::move(content))
            {}
        };

        using WidgetController::WidgetController;

        void loadImages(std::vector<Image> images);

        void insertImages(size_t index, std::vector<Image> images);

        void appendImages(std::vector<Image> images)
        {
            auto index=m_imageKeys.size();
            insertImages(index,std::move(images));
        }
        void prependImages(std::vector<Image> images)
        {
            insertImages(0,std::move(images));
        }

        size_t imageCount() const noexcept
        {
            return m_imageKeys.size();
        }

        QPixmap currentImage() const
        {
            auto key=currentImageKey();
            return imagePixmap(key);
        }

        PixmapKey currentImageKey() const
        {
            if (m_imageKeys.empty())
            {
                return PixmapKey{};
            }
            if (m_currentImageIndex>=m_images.size())
            {
                m_currentImageIndex=m_images.size()-1;
            }
            return m_imageKeys.at(m_currentImageIndex);
        }

        PixmapKey imageKey(size_t index) const
        {
            if (index>=m_images.size())
            {
                return PixmapKey{};
            }
            return m_imageKeys.at(index);
        }

        QPixmap imagePixmap(size_t index) const
        {
            auto key=currentImageKey();
            return imagePixmap(key);
        }

        void setImageSource(std::shared_ptr<PixmapSource> imageSource);

        std::shared_ptr<PixmapSource> imageSource() const
        {
            return m_imageSource;
        }

        size_t currentImageIndex() const noexcept
        {
            return m_currentImageIndex;
        }

        virtual void reset() {}

    signals:

        void currentImageIndexChanged(size_t index);

    public slots:

        virtual void zoomIn() {}
        virtual void zoomOut() {}
        virtual void flipVertical() {}
        virtual void flipHorizontal() {}
        virtual void rotate() {}
        virtual void rotateClockwise() {}

        void showNextImage();
        void showPrevImage();

        void selectImage(size_t index);
        void selectImage(const UISE_DESKTOP_NAMESPACE::PixmapKey& key);

    private slots:

        virtual void onPixmapUpdated(const UISE_DESKTOP_NAMESPACE::PixmapKey& key)
        {
            std::ignore=key;
        }

    protected:

        virtual void doSelectImage() =0;

    private:

        QPixmap imagePixmap(const PixmapKey& key) const
        {
            auto it=m_images.find(key);
            if (it!=m_images.end())
            {
                if (!it->second->content.isNull())
                {
                    return it->second->content;
                }
                if (it->second->consumer.pixmapProducer()!=nullptr)
                {
                    return it->second->consumer.pixmapProducer()->pixmap();
                }
            }
            return QPixmap{};
        }

        struct ImageData
        {
            QPixmap content;
            PixmapConsumer consumer;
        };

        mutable size_t m_currentImageIndex=0;
        std::vector<PixmapKey> m_imageKeys;

        std::map<PixmapKey,std::shared_ptr<ImageData>> m_images;
        std::shared_ptr<PixmapSource> m_imageSource;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACT_IMAGE_VIEWER_HPP
