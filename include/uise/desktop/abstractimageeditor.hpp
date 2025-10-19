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

/** @file uise/desktop/abstractimageeditor.hpp
*
*  Declares AbstractImageEditor.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACT_IMAGE_EDITOR_HPP
#define UISE_DESKTOP_ABSTRACT_IMAGE_EDITOR_HPP

#include <QPixmap>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/widget.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT AbstractImageEditor : public WidgetController
{
    Q_OBJECT

    public:

        using WidgetController::WidgetController;

        void loadImage(const QPixmap& image)
        {
            m_originalImage=image;
            doLoadImage();
        }

        void loadImageFromFile(const QString& filename);

        void loadImageFromClipboard();

        QPixmap originalImage()
        {
            return m_originalImage;
        }

        virtual QPixmap editedImage() =0;

        QString filename() const
        {
            return m_filename;
        }

        void setFolder(QString folder)
        {
            m_folder=folder;
        }

        QString folder() const
        {
            return m_folder;
        }

        void setKeepAspectRatio(bool enable)
        {
            m_keepAspectRatio=enable;
            updateAspectRatio();
        }

        bool keepAspectRatio() const noexcept
        {
            return m_keepAspectRatio;
        }

        void setAspectRatioChangeable(bool enable)
        {
            m_aspectRatioChangeable=enable;
            updateAspectRatio();
        }

        bool aspectRatioChangeable() const noexcept
        {
            return m_aspectRatioChangeable;
        }

        bool isSquareCrop() const noexcept
        {
            return m_squareCrop;
        }

        QSize maximumImageSize() const noexcept
        {
            return m_maxImageSize;
        }

        QSize minimumImageSize() const noexcept
        {
            return m_minImageSize;
        }

        void setFilenameEditable(bool enable)
        {
            m_filenameEditable=enable;
            updateFilenameState();
        }

        bool isFilenameEditable() const noexcept
        {
            return m_filenameEditable;
        }

        void setFilenameVisible(bool enable)
        {
            m_filenameVisible=enable;
            updateFilenameState();
        }

        bool isFilenameVisible() const noexcept
        {
            return m_filenameVisible;
        }

        bool isEllipseCropPreview() const noexcept
        {
            return m_ellipseCropPreview;
        }

        bool isCropEnabled() const noexcept
        {
            return m_cropperEnabled;
        }

        void setEllipseCropPreview(bool enable)
        {
            m_ellipseCropPreview=enable;
            updateCropShape();
        }

        void setCropEnabled(bool enable);
        void setSquareCrop(bool enable);
        void setMaximumImageSize(const QSize& size);
        void setMinimumImageSize(const QSize& size);
        void setFixedImageSize(const QSize& size);

    public slots:

        virtual void zoomIn() {}
        virtual void zoomOut() {}
        virtual void flipVertical() {}
        virtual void flipHorizontal() {}
        virtual void rotate() {}
        virtual void rotateClockwise() {}

        virtual void setFreeHandDrawMode(bool enable) {}

    protected:

        virtual void updateCropShape()
        {}

        virtual void updateImageSizeLimits()
        {}

        virtual void updateAspectRatio()
        {}

        virtual void doLoadImage()
        {}

        virtual void updateFilenameState()
        {}

    private:

        QPixmap m_originalImage;

        bool m_squareCrop=true;
        QSize m_maxImageSize;
        QSize m_minImageSize=QSize{16,16};

        bool m_keepAspectRatio=false;

        bool m_aspectRatioChangeable=false;

        QString m_filename;
        bool m_filenameVisible=true;
        bool m_filenameEditable=true;
        bool m_ellipseCropPreview=false;
        bool m_cropperEnabled=true;

        QString m_folder;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACT_IMAGE_EDITOR_HPP
