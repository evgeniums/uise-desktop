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

/** @file uise/desktop/fileuploaditem.hpp
*
*  Declares FileUploadItem and FileUploadOptions.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FILEUPLOADITEM_HPP
#define UISE_DESKTOP_FILEUPLOADITEM_HPP

#include <vector>

#include <QString>
#include <QByteArray>
#include <QImage>
#include <QPixmap>
#include <QSize>
#include <QUuid>
#include <QMetaType>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractmessageeditor.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief One entry of a file upload list: either a filesystem path or an in-memory image.
 *
 * A plain, cheaply-copyable value type (QString/QImage/QByteArray are all implicitly shared) --
 * no pimpl. Copies keep the same id() and refer to the same logical item; only fromFile()/
 * fromImage()/fromEncodedImage() and the default constructor mint a fresh id.
 *
 * The in-memory master is QImage rather than QPixmap: QPixmap is GUI-thread-only (the reason
 * PixmapProducer exists), while QImage keeps preview scaling/encoding free to move off-thread.
 * Round-tripping through AbstractImageEditor (which is QPixmap-only) costs one
 * QPixmap::fromImage()/toImage() pair, on the GUI thread, only when the editor is actually
 * opened -- see setImage()/image().
 *
 * encodedData() is kept separate from image() because "content size" is an ENCODED byte count:
 * fromEncodedImage() keeps dropped/pasted bytes verbatim so nothing is re-encoded until the
 * user actually edits the image; only setImage() invalidates that cache, and re-encoding then
 * happens lazily on the first size()/encodedData() call after the edit. Encoding a large image
 * can cost tens of milliseconds -- callers pasting/dropping many images at once and seeing a
 * stutter should move that first size() query off the GUI thread (e.g. via QtConcurrent).
 */
class UISE_DESKTOP_EXPORT FileUploadItem
{
    public:

        enum class Type : uint8_t
        {
            File,       //!< Backed by a filesystem path.
            ImageData   //!< Backed by an in-memory QImage.
        };

        FileUploadItem()
            : m_id(QUuid::createUuid())
        {}

        static FileUploadItem fromFile(QString filePath);
        static FileUploadItem fromImage(QImage image, QString fileName={}, QByteArray format="PNG");
        static FileUploadItem fromImage(const QPixmap& image, QString fileName={}, QByteArray format="PNG");

        /**
         * @brief Construct from already-encoded image bytes (e.g. a clipboard or drop payload).
         * @param bytes Encoded image data, kept verbatim as encodedData() until setImage() is
         *  called.
         * @param fileName Optional file name; left empty to be auto-generated later, see
         *  ensureFileName().
         * @param format Encoding format (e.g. "PNG", "JPEG"); sniffed from bytes if empty.
         */
        static FileUploadItem fromEncodedImage(QByteArray bytes, QString fileName={}, QByteArray format={});

        Type type() const noexcept
        {
            return m_type;
        }

        /**
         * @brief Check if this item is an image.
         * @return Always true for Type::ImageData; for Type::File, sniffed from mimeType().
         */
        bool isImage() const;

        /**
         * @brief Stable identity for list bookkeeping (e.g. matching a widget row to an item
         *  across a refresh). Preserved across copies.
         */
        QUuid id() const noexcept
        {
            return m_id;
        }

        /**
         * @brief Get the filesystem path.
         * @return Operation result, empty for Type::ImageData.
         */
        QString filePath() const
        {
            return m_filePath;
        }

        /**
         * @brief Get the file name (basename with extension).
         * @return The name set via fromFile()/fromImage()/setFileName(), or, for Type::File
         *  with no override, the basename of filePath(). May be empty for Type::ImageData
         *  until ensureFileName() is called.
         */
        QString fileName() const;

        /**
         * @brief Set/rename the file name. Metadata only -- never touches the filesystem.
         */
        void setFileName(QString name)
        {
            m_fileName=std::move(name);
        }

        QString suffix() const;

        /**
         * @brief Get the MIME type, e.g. "image/png".
         */
        QString mimeType() const;

        /**
         * @brief Get the decoded image.
         * @return For Type::ImageData, the in-memory master. For Type::File, decoded from disk
         *  on demand (a null QImage if filePath() is not an image or cannot be read).
         *
         * This is the read side of the external-editor round trip.
         */
        QImage image() const;

        /**
         * @brief Overwrite the image content.
         * @param image New image content.
         *
         * This is the write side of the external-editor round trip. Switches type() to
         * ImageData (a File item that gets edited becomes in-memory content) and invalidates
         * the cached encodedData()/size(); fileName() and filePath() are left untouched, so a
         * File item that started as "photo.jpg" and gets edited keeps showing "photo.jpg".
         */
        void setImage(QImage image);

        QByteArray imageFormat() const noexcept
        {
            return m_format;
        }

        void setImageFormat(QByteArray format)
        {
            m_format=std::move(format);
        }

        /**
         * @brief Get the encoded byte content.
         * @return For Type::File, the raw file bytes, read from disk on demand. For
         *  Type::ImageData, the bytes passed to fromEncodedImage() verbatim, or -- once
         *  setImage() has invalidated that cache -- image() encoded to imageFormat(), computed
         *  once and cached.
         */
        QByteArray encodedData() const;

        /**
         * @brief Get the content size in bytes.
         * @return For Type::File, the file's size on disk. For Type::ImageData, encodedData().size().
         */
        qint64 size() const;

        /**
         * @brief Get the image's pixel dimensions.
         * @return For Type::File, read from the file header without decoding the whole image
         *  (cheap). For Type::ImageData, image().size(). An invalid QSize if not an image.
         */
        QSize pixelSize() const;

        /**
         * @brief Generate a file name if none is set yet.
         * @param index Zero-based position of this item in its list, used by autoFileName().
         * @return true if a name was generated (fileName() was empty), false if it already had one.
         */
        bool ensureFileName(int index);

        /**
         * @brief Generate a default file name for an unnamed image item.
         * @param index Zero-based position of the item in its list.
         * @param suffix File extension without the dot, e.g. "png".
         * @return A name of the form "<prefix>_yyyyMMdd_hhmmss_<index+1>.<suffix>".
         */
        static QString autoFileName(int index, const QString& suffix);

        /**
         * @brief Set the prefix used by autoFileName() (default "image").
         */
        static void setAutoFileNameTemplate(QString prefix);

    private:

        Type m_type=Type::File;
        QUuid m_id;
        QString m_filePath;
        QString m_fileName;

        QImage m_image;
        mutable QByteArray m_encoded;
        QByteArray m_format=QByteArray("PNG");

        mutable QSize m_pixelSize;
        mutable qint64 m_size=-1;
};

using FileUploadItems=std::vector<FileUploadItem>;

/**
 * @brief Send-time options collected from a file upload widget alongside its items().
 */
struct UISE_DESKTOP_EXPORT FileUploadOptions
{
    bool highQuality=false;
    bool sendAsDocuments=false;
    bool groupItems=false;
    bool rememberChoice=false;

    QString comment;
    TextFormat commentFormat=TextFormat::Markdown;
};

UISE_DESKTOP_NAMESPACE_END

Q_DECLARE_METATYPE(UISE_DESKTOP_NAMESPACE::FileUploadItem)

#endif // UISE_DESKTOP_FILEUPLOADITEM_HPP
