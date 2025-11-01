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

/** @file uise/desktop/abstractimageditor.cpp
*
*  Defines AbstractImageViewer.
*
*/

/****************************************************************************/

#include <uise/desktop/widgetfactory.hpp>
#include <uise/desktop/abstractimageviewer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

void AbstractImageViewer::setImageSource(std::shared_ptr<PixmapSource> imageSource)
{
    m_imageSource=std::move(imageSource);
}

//--------------------------------------------------------------------------

void AbstractImageViewer::selectImage(size_t index)
{
    m_currentImageIndex=index;
    if (m_currentImageIndex>=m_imageKeys.size())
    {
        m_currentImageIndex=m_imageKeys.size()-1;
    }
    doSelectImage();
    emit currentImageIndexChanged(m_currentImageIndex);
}

//--------------------------------------------------------------------------

void AbstractImageViewer::showNextImage()
{
    if (m_imageKeys.empty())
    {
        return;
    }

    if (m_imageKeys.size()>(m_currentImageIndex+1))
    {
        selectImage(m_currentImageIndex+1);
    }
}

//--------------------------------------------------------------------------

void AbstractImageViewer::showPrevImage()
{
    if (m_imageKeys.empty())
    {
        return;
    }

    if (m_currentImageIndex>0)
    {
        selectImage(m_currentImageIndex-1);
    }
}

//--------------------------------------------------------------------------

void AbstractImageViewer::loadImages(std::vector<Image> images)
{
    m_imageKeys.clear();
    m_images.clear();
    m_currentImageIndex=0;
    insertImages(0,std::move(images));
}

//--------------------------------------------------------------------------

void AbstractImageViewer::insertImages(size_t index, std::vector<Image> images)
{
    std::vector<PixmapKey> keys;

    for (const auto& image : images)
    {
        keys.push_back(image.key);
        auto item=std::make_shared<ImageData>();
        item->content=image.content;
        if (m_imageSource)
        {
            item->consumer.setPixmapSource(m_imageSource);
            item->consumer.acquireProducer();
            connect(
                &item->consumer,
                &PixmapConsumer::pixmapUpdated,
                this,
                [this,key=image.key]()
                {
                    onPixmapUpdated(key);
                }
            );
        }
        m_images[image.key]=item;
    }

    m_imageKeys.insert(m_imageKeys.begin() + index,keys.begin(),keys.end());
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
