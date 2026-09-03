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
#include <uise/desktop/frame.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class UISE_DESKTOP_EXPORT AbstractImageEditor : public WidgetController
{
    Q_OBJECT

    public:

        enum class CropMode
        {
            Off,
            Square,
            Rectangular
        };
        Q_ENUM(CropMode)

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

        CropMode cropMode() const noexcept
        {
            if (!m_cropperEnabled)
            {
                return CropMode::Off;
            }
            return m_squareCrop ? CropMode::Square : CropMode::Rectangular;
        }

        void setCropMode(CropMode mode);

        void setCropButtonVisible(bool enable)
        {
            m_cropButtonVisible=enable;
            updateCropButtonState();
        }

        bool isCropButtonVisible() const noexcept
        {
            return m_cropButtonVisible;
        }

        void setCropEnabled(bool enable);
        void setSquareCrop(bool enable);
        void setMaximumImageSize(const QSize& size);
        void setMinimumImageSize(const QSize& size);
        void setFixedImageSize(const QSize& size);

        void setNativeFileDialog(bool enable) noexcept
        {
            m_nativeFileDialog=enable;
        }

        bool isNativeFileDialog() const noexcept
        {
            return m_nativeFileDialog;
        }

    signals:

        void cropModeChanged(CropMode mode);

    public slots:

        virtual void zoomIn() {}
        virtual void zoomOut() {}
        virtual void flipVertical() {}
        virtual void flipHorizontal() {}
        virtual void rotate() {}
        virtual void rotateClockwise() {}

        virtual void setFreeHandDrawMode(bool /*enable*/) {}

    protected:

        virtual void updateCropShape()
        {}

        virtual void updateCropEnabled()
        {}

        virtual void updateCropButtonState()
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

        void applyCropMode();

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
        bool m_cropButtonVisible=false;

        QString m_folder;
        bool m_nativeFileDialog=true;
};

}

#endif // UISE_DESKTOP_ABSTRACT_IMAGE_EDITOR_HPP
