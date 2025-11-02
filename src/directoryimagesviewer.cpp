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

/** @file uise/desktop/src/directoryimagesviewer.cpp
*
*
*/

/****************************************************************************/

// #include <iostream>

#include <QLineEdit>
#include <QFileDialog>
#include <QPixmap>
#include <QPointer>
#include <QStandardPaths>
#include <QMimeDatabase>
#include <QMimeType>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/imageviewer.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/directoryimagesviewer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

void DirectoryImagesSource::doLoadPixmap(const PixmapKey& key)
{
    auto path=key.toFilePath();
    QPixmap px;
    px.load(QString::fromStdString(path.string()));

    for (auto& producer : producers(key))
    {
        producer->setPixmap(px);
    }
}

//--------------------------------------------------------------------------

DirectoryImagesViewer::DirectoryImagesViewer(QWidget *parent)
    : WidgetQFrame(parent),
      m_customFileDialog(false)
{
    auto m_imageSource=std::make_shared<DirectoryImagesSource>();

    auto l=Layout::vertical(this);

    m_viewer=makeWidget<AbstractImageViewer,ImageViewer>(this);
    Q_ASSERT(m_viewer);
    l->addWidget(m_viewer->qWidget(),1);
    m_viewer->setImageSource(m_imageSource);
    connect(
        m_viewer,
        &AbstractImageViewer::currentImageIndexChanged,
        this,
        &DirectoryImagesViewer::onCurrentImageIndexChanged
    );

    m_fileBrowserFrame=new QFrame(this);
    m_fileBrowserFrame->setObjectName("fileBrowserFrame");
    l->addWidget(m_fileBrowserFrame);
    auto fl=Layout::horizontal(m_fileBrowserFrame);
    m_fileName=new QLineEdit(m_fileBrowserFrame);
    m_fileName->setObjectName("fileName");
    fl->addWidget(m_fileName,1);
    m_browseButton=new PushButton(m_fileBrowserFrame);
    m_browseButton->setObjectName("browseButton");
    m_browseButton->setText(tr("Browse..."));
    fl->addWidget(m_browseButton);
    connect(
        m_browseButton,
        &PushButton::clicked,
        this,
        &DirectoryImagesViewer::browseFile
    );
}

//--------------------------------------------------------------------------

void DirectoryImagesViewer::browseFile()
{
    QPointer<QFrame> guard(this);

    QFileDialog::Options options;
    if (m_customFileDialog)
    {
        options=QFileDialog::DontUseNativeDialog;
    }
    QString filter{tr("All Files (*.*); Image Files (*.png *.jpg *.jpeg *.bmp *.tiff) ")};
    auto fileName=QFileDialog::getOpenFileName(this,tr("Select image file"),QString::fromStdString(path()),filter,nullptr,options);
    if (!guard || fileName.isEmpty())
    {
        return;
    }
    selectPath(fileName.toStdString());
}

//--------------------------------------------------------------------------

std::filesystem::path DirectoryImagesViewer::path() const
{
    if (m_path.empty())
    {
        QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        m_path=homePath.toStdString();
    }
    return m_path;
}

//--------------------------------------------------------------------------

void DirectoryImagesViewer::selectPath(std::filesystem::path filePath)
{
    m_path=std::move(filePath);

    std::vector<AbstractImageViewer::Image> images;

    std::filesystem::path selectFile=m_path;
    std::filesystem::path dir=m_path;
    if (!std::filesystem::is_directory(dir))
    {
        dir=dir.parent_path();
    }
    else
    {
        selectFile.clear();
    }

    std::vector<std::filesystem::path> files;

    QMimeDatabase mimeDatabase;
    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(dir))
        {
            if (entry.is_regular_file())
            {
                auto fileName=entry.path().string();
                QMimeType mimeType = mimeDatabase.mimeTypeForFile(QString::fromStdString(fileName));
                if (mimeType.name().startsWith("image/"))
                {
                    files.push_back(entry.path());
                }
            }
        }
    }
    catch (const std::filesystem::filesystem_error&)
    {
    }

    std::sort(
        files.begin(),
        files.end(),
        [](const std::filesystem::path& l, const std::filesystem::path& r)
        {
            return l<r;
        }
    );
    for (const auto& file : files)
    {
        PixmapKey key{file};
        key.setAnySize(true);
        images.push_back(AbstractImageViewer::Image{std::move(key)});
    }
    m_viewer->loadImages(std::move(images));

    if (selectFile.empty())
    {
        m_viewer->selectImage(0);
    }
    else
    {
        m_viewer->selectImage(selectFile);
    }
}

//--------------------------------------------------------------------------

void DirectoryImagesViewer::onCurrentImageIndexChanged(size_t)
{
    auto key=m_viewer->currentImageKey();
    m_fileName->setText(QString::fromStdString(key.toFilePath().string()));
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
