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
#include <QRegularExpression>

#include <uise/desktop/utils/mimedatautils.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

//! Reduce a candidate (a URL path or an <img src>) to a validated, sanitized bare filename
//! with a recognized image/* suffix, or an empty string if it doesn't qualify -- see
//! mimeDataImageFileNameHint()'s own doc comment for the full rationale.
QString sanitizeImageFileNameCandidate(const QString& candidate)
{
    if (candidate.isEmpty())
    {
        return QString();
    }

    // The URL-list branch already hands us a decoded QUrl::path(); the <img src> branch hands
    // us a raw src attribute that may itself be an absolute/relative URL (parse it) or, for a
    // data: URI (an inline-encoded image, no filename to recover), nothing usable at all.
    QUrl url(candidate,QUrl::TolerantMode);
    if (url.scheme()==QStringLiteral("data"))
    {
        return QString();
    }
    auto path=(url.isValid() && !url.path().isEmpty()) ? url.path() : candidate;

    auto name=QFileInfo(path).fileName();
    if (name.isEmpty() || name==QStringLiteral(".") || name==QStringLiteral(".."))
    {
        return QString();
    }

    static const QRegularExpression illegal(QStringLiteral("[\\x00-\\x1f\"*/:<>?\\\\|]"));
    name.remove(illegal);
    name=name.trimmed();
    if (name.isEmpty())
    {
        return QString();
    }

    QMimeDatabase db;
    auto type=db.mimeTypeForFile(name,QMimeDatabase::MatchExtension);
    if (!type.name().startsWith(QStringLiteral("image/")))
    {
        return QString();
    }

    constexpr int maxLength=128;
    if (name.size()>maxLength)
    {
        QFileInfo fi(name);
        auto suffix=fi.suffix();
        auto base=fi.completeBaseName();
        auto keep=maxLength-suffix.size()-1;
        if (keep>0)
        {
            base=base.left(keep);
            name=suffix.isEmpty() ? base : base+QLatin1Char('.')+suffix;
        }
        else
        {
            name=name.left(maxLength);
        }
    }

    return name;
}

//! First <img ... src="..."> (or src='...') found in an HTML fragment, or an empty string.
//! Non-greedy, case-insensitive, single match -- good enough for the clipboard HTML a browser's
//! own "Copy Image" puts alongside the bitmap; not a general HTML parser.
QString firstImgSrc(const QString& html)
{
    static const QRegularExpression re(
        QStringLiteral("<img\\b[^>]*\\bsrc\\s*=\\s*[\"']([^\"']+)[\"']"),
        QRegularExpression::CaseInsensitiveOption);
    auto match=re.match(html);
    if (match.hasMatch())
    {
        return match.captured(1);
    }
    return QString();
}

}

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

const QString& imageFileNameMimeFormat()
{
    static const QString format=QStringLiteral("application/x-uise-image-filename");
    return format;
}

//--------------------------------------------------------------------------

QString mimeDataImageFileNameHint(const QMimeData* mimeData)
{
    if (mimeData==nullptr)
    {
        return QString();
    }

    if (mimeData->hasFormat(imageFileNameMimeFormat()))
    {
        auto name=sanitizeImageFileNameCandidate(
            QString::fromUtf8(mimeData->data(imageFileNameMimeFormat())));
        if (!name.isEmpty())
        {
            return name;
        }
    }

    if (mimeData->hasUrls())
    {
        const auto urls=mimeData->urls();
        for (const auto& url : urls)
        {
            if (url.isLocalFile())
            {
                // Local files already carry their own real name via QFileInfo in the caller's
                // own addFiles() path -- not this helper's concern.
                continue;
            }
            auto name=sanitizeImageFileNameCandidate(url.path());
            if (!name.isEmpty())
            {
                return name;
            }
        }
    }

    if (mimeData->hasHtml())
    {
        auto src=firstImgSrc(mimeData->html());
        if (!src.isEmpty())
        {
            auto name=sanitizeImageFileNameCandidate(src);
            if (!name.isEmpty())
            {
                return name;
            }
        }
    }

    return QString();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
