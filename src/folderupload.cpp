/**
@copyright Evgeny Sidorov 2026

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/folderupload.cpp
*
*  Defines the folder drop helpers.
*
*/

/****************************************************************************/

#include <algorithm>

#include <QCollator>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>

#include <uise/desktop/utils/folderupload.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

QString mimeDataSingleLocalFolder(const QMimeData* mimeData)
{
    if (mimeData==nullptr || !mimeData->hasUrls())
    {
        return QString{};
    }

    const auto urls=mimeData->urls();
    if (urls.size()!=1 || !urls.front().isLocalFile())
    {
        return QString{};
    }

    const QFileInfo fi(urls.front().toLocalFile());
    if (!fi.isDir())
    {
        return QString{};
    }

    return QDir::cleanPath(fi.absoluteFilePath());
}

//--------------------------------------------------------------------------

FolderUploadScan scanFolderForUpload(const QString& folderPath, int maxFiles)
{
    FolderUploadScan scan;

    // No QDir::Hidden and no QDir::System: dot-files, hidden-attribute files and broken links
    // are not offered. A link to a folder is listed under Dirs, so it counts as a subfolder.
    QDirIterator it(folderPath,QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot);
    while (it.hasNext())
    {
        it.next();
        const auto fi=it.fileInfo();

        if (fi.isDir())
        {
            scan.hasSubfolders=true;
            break;
        }

        scan.files.push_back(fi.absoluteFilePath());
        if (maxFiles>0 && scan.files.size()>maxFiles)
        {
            scan.exceeded=true;
            break;
        }
    }

    // QDirIterator's order is whatever the file system returns. The "img2, img10" order a user
    // expects is a numeric one; every path here shares one directory, so comparing whole paths
    // compares names.
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(
        scan.files.begin(),
        scan.files.end(),
        [&collator](const QString& l, const QString& r)
        {
            return collator.compare(l,r)<0;
        }
    );

    return scan;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
