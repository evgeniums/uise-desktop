/**
@copyright Evgeny Sidorov 2025

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/directoryimagesviewer.hpp
*
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DIRECTORY_IMAGES_VIEWER_HPP
#define UISE_DESKTOP_DIRECTORY_IMAGES_VIEWER_HPP

#include <QFrame>

#include <filesystem>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/pixmapproducer.hpp>
#include <uise/desktop/frame.hpp>

class QLineEdit;

UISE_DESKTOP_NAMESPACE_BEGIN

class PushButton;
class AbstractImageViewer;

class UISE_DESKTOP_EXPORT DirectoryImagesSource : public PixmapSource
{
    public:

        using PixmapSource::PixmapSource;

    protected:

        void doLoadPixmap(const PixmapKey& key) override;
};

class UISE_DESKTOP_EXPORT DirectoryImagesViewer : public WidgetQFrame
{
    Q_OBJECT

    public:

        DirectoryImagesViewer(QWidget* parent);

        std::filesystem::path path() const;

        void setFileBrowserFrameVisible(bool enable);
        bool isFileBrowserFrameVisible() const;

        void setCustomFileDialog(bool enable) noexcept
        {
            m_customFileDialog=enable;
        }

        bool isCustomFileDialog() const noexcept
        {
            return m_customFileDialog;
        }

    public slots:

        void selectPath(std::filesystem::path filePath);

    private slots:

        void browseFile();
        void onCurrentImageIndexChanged(size_t index);

    private:

        std::shared_ptr<DirectoryImagesSource> m_imageSource;
        AbstractImageViewer* m_viewer;

        QFrame* m_fileBrowserFrame;
        QLineEdit* m_fileName;
        PushButton* m_browseButton;

        bool m_customFileDialog;
        mutable std::filesystem::path m_path;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_DIRECTORY_IMAGES_VIEWER_HPP
