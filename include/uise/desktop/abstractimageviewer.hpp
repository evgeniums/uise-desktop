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
#include <cstdint>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>
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

        //! How the navigation/toolbar controls are presented over the image.
        enum class ControlsMode : uint8_t
        {
            Static,   //!< Current behaviour: the bottom widget occupies layout space below the image.
            Overlay   //!< The bottom widget and prev/next buttons float over the image and fade out
                      //!< after a period of user inactivity, reappearing on mouse movement.
        };
        Q_ENUM(ControlsMode)

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

        QPixmap imagePixmap(size_t /*index*/) const
        {
            auto key=currentImageKey();
            return imagePixmap(key);
        }

        virtual void setImageSource(std::shared_ptr<PixmapSource> imageSource);

        std::shared_ptr<PixmapSource> imageSource() const
        {
            return m_imageSource;
        }

        size_t currentImageIndex() const noexcept
        {
            return m_currentImageIndex;
        }

        //! Switch between the embedded static toolbar and a floating, auto-hiding overlay.
        //! Default is ControlsMode::Static, so existing callers see no behavioural change.
        virtual void setControlsMode(ControlsMode mode)
        {
            m_controlsMode=mode;
        }

        ControlsMode controlsMode() const noexcept
        {
            return m_controlsMode;
        }

        /**
         * @brief Replace the embedded bottom toolbar with a caller-supplied widget.
         * @param widget New bottom widget; the viewer takes ownership (reparents it). Passing
         *  nullptr restores the viewer's own embedded toolbar.
         *
         * Base implementation is a no-op: only a concrete viewer with an actual widget surface
         * (see ImageViewer) can host one.
         */
        virtual void setBottomWidget(QWidget* widget)
        {
            std::ignore=widget;
        }

        //! Get the widget set via setBottomWidget(), or nullptr if none/the embedded toolbar is used.
        virtual QWidget* bottomWidget() const
        {
            return nullptr;
        }

        //! Idle delay, in ControlsMode::Overlay, before the controls fade out. Default 2500 ms.
        void setControlsAutoHideDelayMs(int ms) noexcept
        {
            m_controlsAutoHideDelayMs=ms;
        }

        int controlsAutoHideDelayMs() const noexcept
        {
            return m_controlsAutoHideDelayMs;
        }

    signals:

        void currentImageIndexChanged(size_t index);

        //! Emitted once when the user presses Escape (see ImageViewerWidget::keyPressEvent()).
        //! A host wrapping the viewer in a modal dialog is expected to close that dialog on this.
        void closeRequested();

    public slots:

        virtual void reset() {}

        virtual void zoomIn() {}
        virtual void zoomOut() {}
        virtual void flipVertical() {}
        virtual void flipHorizontal() {}
        virtual void rotate() {}
        virtual void rotateClockwise() {}

        //! Alias of rotate() (which rotates counterclockwise) under an unambiguous name; not
        //! itself virtual so a single definition always dispatches to whatever rotate() resolves to.
        void rotateCounterclockwise()
        {
            rotate();
        }

        virtual void fitImage() {}

        //! Not itself virtual -- a single definition always dispatches to whatever
        //! closeRequested() is connected to; see ImageViewerWidget::keyPressEvent()'s Escape case.
        void requestClose()
        {
            emit closeRequested();
        }

        void showNextImage();
        void showPrevImage();

        void selectImage(size_t index);
        void selectImage(const UISE_DESKTOP_NAMESPACE::PixmapKey& key);

        //! In ControlsMode::Overlay, show the controls and (re)arm the auto-hide timer. No-op in Static mode.
        virtual void showControls() {}

        //! In ControlsMode::Overlay, fade the controls out immediately. No-op in Static mode.
        virtual void hideControls() {}

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

        ControlsMode m_controlsMode=ControlsMode::Static;
        int m_controlsAutoHideDelayMs=2500;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACT_IMAGE_VIEWER_HPP
