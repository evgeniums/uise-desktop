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

/** @file uise/desktop/src/fileuploaditem.cpp
*
*  Defines FileUploadItem.
*
*/

/****************************************************************************/

#include <algorithm>

#include <QFile>
#include <QFileInfo>
#include <QBuffer>
#include <QImageReader>
#include <QMimeDatabase>
#include <QDateTime>
#include <QPainter>
#include <QtSvg/QSvgRenderer>

#include <uise/desktop/fileuploaditem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

QString& autoFileNamePrefix()
{
    static QString prefix=QStringLiteral("image");
    return prefix;
}

}

//--------------------------------------------------------------------------

FileUploadItem FileUploadItem::fromFile(QString filePath)
{
    FileUploadItem item;
    item.m_type=Type::File;
    item.m_filePath=std::move(filePath);
    item.m_fileName=QFileInfo(item.m_filePath).fileName();
    return item;
}

//--------------------------------------------------------------------------

FileUploadItem FileUploadItem::fromImage(QImage image, QString fileName, QByteArray format)
{
    FileUploadItem item;
    item.m_type=Type::ImageData;
    item.m_image=std::move(image);
    item.m_fileName=std::move(fileName);
    item.m_format=format.isEmpty() ? QByteArray("PNG") : std::move(format);
    return item;
}

//--------------------------------------------------------------------------

FileUploadItem FileUploadItem::fromImage(const QPixmap& image, QString fileName, QByteArray format)
{
    return fromImage(image.toImage(),std::move(fileName),std::move(format));
}

//--------------------------------------------------------------------------

FileUploadItem FileUploadItem::fromEncodedImage(QByteArray bytes, QString fileName, QByteArray format)
{
    FileUploadItem item;
    item.m_type=Type::ImageData;
    // keep the original bytes verbatim: encodedData() below returns them as-is until
    // setImage() invalidates the cache, so nothing gets re-encoded just because it passed
    // through this constructor
    item.m_encoded=bytes;

    QBuffer buffer(&bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    if (format.isEmpty())
    {
        format=reader.format();
    }
    item.m_image=reader.read();
    item.m_format=format.isEmpty() ? QByteArray("PNG") : std::move(format);
    item.m_fileName=std::move(fileName);
    return item;
}

//--------------------------------------------------------------------------

bool FileUploadItem::isImage() const
{
    return (m_type==Type::ImageData) || mimeType().startsWith(QStringLiteral("image/"));
}

//--------------------------------------------------------------------------

bool FileUploadItem::presentAsImage() const
{
    if (!isImage() || m_maxImageAspectRatio==0)
    {
        return isImage();
    }

    auto sz=pixelSize();
    if (!sz.isValid() || sz.width()<=0 || sz.height()<=0)
    {
        // Dimensions unknown -- the ratio check has nothing to act on, same
        // leniency as whitemclient's aspectRatioExceeds() probe-failure rule.
        return true;
    }

    auto hi=std::max(sz.width(),sz.height());
    auto lo=std::min(sz.width(),sz.height());
    return static_cast<qint64>(hi)<=static_cast<qint64>(lo)*m_maxImageAspectRatio;
}

//--------------------------------------------------------------------------

QString FileUploadItem::fileName() const
{
    if (!m_fileName.isEmpty())
    {
        return m_fileName;
    }
    if (m_type==Type::File)
    {
        return QFileInfo(m_filePath).fileName();
    }
    return QString();
}

//--------------------------------------------------------------------------

QString FileUploadItem::suffix() const
{
    return QFileInfo(fileName()).suffix();
}

//--------------------------------------------------------------------------

QString FileUploadItem::mimeType() const
{
    if (!m_explicitMimeType.isEmpty())
    {
        return m_explicitMimeType;
    }

    if (m_type==Type::ImageData)
    {
        auto fmt=QString::fromLatin1(m_format).toLower();
        if (fmt==QStringLiteral("jpg"))
        {
            fmt=QStringLiteral("jpeg");
        }
        return QStringLiteral("image/%1").arg(fmt);
    }

    QMimeDatabase db;
    return db.mimeTypeForFile(m_filePath).name();
}

//--------------------------------------------------------------------------

QImage FileUploadItem::image() const
{
    if (m_type==Type::ImageData)
    {
        return m_image;
    }

    if (mimeType()==QStringLiteral("image/svg+xml"))
    {
        // QImage(m_filePath) would decode via Qt's SVG plugin at the file's own declared
        // size -- for an icon-style SVG with only a viewBox and no explicit width/height
        // (e.g. a tabler.io icon, viewBox="0 0 24 24"), Qt correctly falls back to that
        // viewBox size, but that is a tiny raster (24x24). FileUploadListItem::updatePreview()
        // then scales THAT raster: the row/document chip's scaledAndCropped() deliberately
        // upscales to fill its fixed 40x40 box and looks fine, but the full image preview's
        // scaledToFit() deliberately never upscales past the source's own resolution (the
        // right call for a genuinely low-res photo) -- so a 24x24 source inside an up-to-
        // 220x260 box ends up rendered at a barely-visible ~24x24, not "poorly sized" so much
        // as nearly invisible. Render the vector content ourselves at an adequately large
        // target resolution instead, so every consumer downscales from a sharp source rather
        // than upscaling (or failing to scale) a tiny one. Harmless for an SVG that already
        // declares a large size -- QSvgRenderer::defaultSize() reports that, and scaling it to
        // fit within longSide only ever shrinks it, exactly like the previous decode did.
        QSvgRenderer renderer(m_filePath);
        if (renderer.isValid())
        {
            auto native=renderer.defaultSize();
            if (!native.isEmpty())
            {
                constexpr int longSide=512;
                auto target=native.scaled(longSide,longSide,Qt::KeepAspectRatio);
                QImage img(target,QImage::Format_ARGB32_Premultiplied);
                img.fill(Qt::transparent);
                QPainter painter(&img);
                painter.setRenderHint(QPainter::Antialiasing);
                renderer.render(&painter);
                return img;
            }
        }
    }

    return QImage(m_filePath);
}

//--------------------------------------------------------------------------

void FileUploadItem::setImage(QImage image)
{
    m_type=Type::ImageData;
    m_image=std::move(image);
    m_encoded.clear();
    m_pixelSize=QSize();
    m_size=-1;
    // m_fileName and m_filePath are left untouched: an edited File item keeps its display
    // name, and filePath() stays as provenance even though image()/encodedData() no longer
    // read from it once type() is ImageData
}

//--------------------------------------------------------------------------

QByteArray FileUploadItem::encodedData() const
{
    if (m_type==Type::File)
    {
        QFile f(m_filePath);
        if (f.open(QIODevice::ReadOnly))
        {
            return f.readAll();
        }
        return QByteArray();
    }

    if (m_encoded.isEmpty() && !m_image.isNull())
    {
        QBuffer buffer(&m_encoded);
        buffer.open(QIODevice::WriteOnly);
        m_image.save(&buffer,m_format.constData());
    }
    return m_encoded;
}

//--------------------------------------------------------------------------

qint64 FileUploadItem::size() const
{
    if (m_type==Type::File)
    {
        return QFileInfo(m_filePath).size();
    }
    if (m_size<0)
    {
        m_size=encodedData().size();
    }
    return m_size;
}

//--------------------------------------------------------------------------

QSize FileUploadItem::pixelSize() const
{
    if (m_type==Type::File)
    {
        if (!m_pixelSize.isValid())
        {
            QImageReader reader(m_filePath);
            m_pixelSize=reader.size();
        }
        return m_pixelSize;
    }
    return m_image.size();
}

//--------------------------------------------------------------------------

bool FileUploadItem::ensureFileName(int index)
{
    if (!m_fileName.isEmpty())
    {
        return false;
    }
    auto suf=m_format.isEmpty() ? QStringLiteral("png") : QString::fromLatin1(m_format).toLower();
    m_fileName=autoFileName(index,suf);
    return true;
}

//--------------------------------------------------------------------------

QString FileUploadItem::autoFileName(int index, const QString& suffix)
{
    auto stamp=QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"));
    auto suf=suffix.isEmpty() ? QStringLiteral("png") : suffix;
    return QStringLiteral("%1_%2_%3.%4").arg(autoFileNamePrefix(),stamp).arg(index+1).arg(suf);
}

//--------------------------------------------------------------------------

void FileUploadItem::setAutoFileNameTemplate(QString prefix)
{
    autoFileNamePrefix()=std::move(prefix);
}

//--------------------------------------------------------------------------

QString FileUploadItem::fileNameWithSuffix(QString fileName, const QString& suffix)
{
    if (fileName.isEmpty() || suffix.isEmpty())
    {
        return fileName;
    }

    // jpg/jpeg are the same format under two spellings -- don't rewrite a correct ".jpeg" name
    // just because the re-encoded suffix happens to be spelled "jpg" (or vice versa).
    auto normalize=[](QString s)
    {
        s=s.toLower();
        return s==QStringLiteral("jpeg") ? QStringLiteral("jpg") : s;
    };

    QFileInfo fi(fileName);
    auto existingSuffix=fi.suffix();
    if (!existingSuffix.isEmpty() && normalize(existingSuffix)==normalize(suffix))
    {
        return fileName;
    }

    auto base=fi.completeBaseName();
    if (base.isEmpty())
    {
        // fileName had no suffix at all (completeBaseName() == fileName in that case) --
        // keep it whole and just append the suffix.
        base=fileName;
    }
    return base+QLatin1Char('.')+suffix;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
