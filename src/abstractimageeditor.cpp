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
*  Defines AbstractImageEditor.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QFileInfo>

#include <uise/desktop/widgetfactory.hpp>
#include <uise/desktop/abstractimageeditor.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

void AbstractImageEditor::setSquareCrop(bool enable)
{
    m_squareCrop=enable;
    updateCropShape();
}

//--------------------------------------------------------------------------

void AbstractImageEditor::setMaximumImageSize(const QSize& size)
{
    m_maxImageSize=size;
    updateImageSizeLimits();
}

//--------------------------------------------------------------------------

void AbstractImageEditor::setMinimumImageSize(const QSize& size)
{
    m_minImageSize=size;
    updateImageSizeLimits();
}

//--------------------------------------------------------------------------

void AbstractImageEditor::setFixedImageSize(const QSize& size)
{
    m_maxImageSize=size;
    m_minImageSize=size;
    updateImageSizeLimits();
}

//--------------------------------------------------------------------------

void AbstractImageEditor::loadImageFromFile(const QString& filename)
{
    if (filename.isEmpty())
    {
        return;
    }

    QFileInfo finf{filename};
    setFolder(finf.canonicalPath());

    QPixmap pixmap(filename);

    m_filename=filename;
    updateFilenameState();

    loadImage(pixmap);
}

//--------------------------------------------------------------------------

void AbstractImageEditor::loadImageFromClipboard()
{
    QClipboard *clipboard = QApplication::clipboard();
    if (clipboard->mimeData()->hasImage())
    {
        QPixmap pixmap = clipboard->pixmap();
        loadImage(pixmap);

        m_filename.clear();
        updateFilenameState();
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
