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

/** @file uise/desktop/src/mimedatautils.cpp
*
*  Defines helpers for inspecting QMimeData.
*
*/

/****************************************************************************/

#include <QMimeData>
#include <QMimeDatabase>
#include <QFileInfo>
#include <QUrl>

#include <uise/desktop/utils/mimedatautils.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

const QStringList& acceptedImageMimeFormats()
{
    static const QStringList formats={
        QStringLiteral("image/png"),
        QStringLiteral("image/jpeg"),
        QStringLiteral("image/webp")
    };
    return formats;
}

//--------------------------------------------------------------------------

QStringList mimeDataLocalFilePaths(const QMimeData* mimeData)
{
    QStringList paths;

    if (mimeData==nullptr || !mimeData->hasUrls())
    {
        return paths;
    }

    const auto urls=mimeData->urls();
    for (const auto& url : urls)
    {
        if (url.isLocalFile())
        {
            QFileInfo fi(url.toLocalFile());
            if (fi.isFile())
            {
                paths.push_back(fi.filePath());
            }
        }
    }

    return paths;
}

//--------------------------------------------------------------------------

bool mimeDataHasImages(const QMimeData* mimeData)
{
    if (mimeData==nullptr)
    {
        return false;
    }

    if (mimeData->hasImage())
    {
        return true;
    }

    for (const auto& fmt : acceptedImageMimeFormats())
    {
        if (mimeData->hasFormat(fmt))
        {
            return true;
        }
    }

    // An image FILE dragged from a file manager carries only text/uri-list, no "image"
    // format at all -- sniff the dropped paths' own extensions rather than miss this, by far
    // the most common way a user actually drags an image. MatchExtension only, deliberately:
    // content matching would open every dragged file synchronously inside a drag handler, and
    // a dragged file need not even exist locally yet (e.g. macOS promised files).
    QMimeDatabase db;
    const auto paths=mimeDataLocalFilePaths(mimeData);
    for (const auto& path : paths)
    {
        auto type=db.mimeTypeForFile(path,QMimeDatabase::MatchExtension);
        if (type.name().startsWith(QStringLiteral("image/")))
        {
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------

bool mimeDataHasAttachments(const QMimeData* mimeData)
{
    if (mimeData==nullptr)
    {
        return false;
    }

    return !mimeDataLocalFilePaths(mimeData).isEmpty() || mimeDataHasImages(mimeData);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
