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

/** @file uise/desktop/imageviewer.hpp
*
*  Declares ImageViewer.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_IMAGE_VIEWER_HPP
#define UISE_DESKTOP_IMAGE_VIEWER_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractimageviewer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class ImageViewerWidget;

class UISE_DESKTOP_EXPORT ImageViewer : public AbstractImageViewer
{
    Q_OBJECT

    public:

        using AbstractImageViewer::AbstractImageViewer;

        void reset() override;

    public slots:

        void zoomIn() override;
        void zoomOut() override;
        void flipVertical() override;
        void flipHorizontal() override;
        void rotate() override;
        void rotateClockwise() override;

    private slots:

        void onPixmapUpdated(const UISE_DESKTOP_NAMESPACE::PixmapKey& key) override;

    protected:

        void doReset();
        void fitImage();
        void doSelectImage() override;
        Widget* doCreateActualWidget(QWidget* parent) override;

        ImageViewerWidget* m_widget;
};

class ImageViewerWidget_p;
class UISE_DESKTOP_EXPORT ImageViewerWidget : public WidgetQFrame
{
    Q_OBJECT

    public:

        ImageViewerWidget(ImageViewer* ctrl, QWidget* parent=nullptr);

        ~ImageViewerWidget();
        ImageViewerWidget(const ImageViewerWidget&)=delete;
        ImageViewerWidget(ImageViewerWidget&&)=delete;
        ImageViewerWidget& operator=(const ImageViewerWidget&)=delete;
        ImageViewerWidget& operator=(ImageViewerWidget&&)=delete;

    private:

        std::unique_ptr<ImageViewerWidget_p> pimpl;

        friend class ImageViewer;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_IMAGE_VIEWER_HPP
