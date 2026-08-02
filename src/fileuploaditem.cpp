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

#include <QFile>
#include <QFileInfo>
#include <QBuffer>
#include <QImageReader>
#include <QMimeDatabase>
#include <QDateTime>

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
    if (m_type==Type::ImageData)
    {
        return true;
    }
    return mimeType().startsWith(QStringLiteral("image/"));
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

UISE_DESKTOP_NAMESPACE_END
