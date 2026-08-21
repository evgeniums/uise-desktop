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
         * @brief Check if this item's CONTENT is an image, regardless of how it will be
         *  presented (see presentAsImage() for that).
         * @return Always true for Type::ImageData; for Type::File, sniffed from mimeType().
         *  Unaffected by maxImageAspectRatio() -- an extreme-aspect-ratio image is still an
         *  image: still editable, still decodes a real thumbnail/pixelSize(), it just isn't
         *  presented as an inline image tile (see presentAsImage()).
         */
        bool isImage() const;

        /**
         * @brief Check whether this item should be PRESENTED as an inline image tile
         *  (FileUploadListItem::View::Image) rather than a plain document row (View::Row).
         * @return false whenever isImage() is false. Also false for an image whose pixelSize()
         *  aspect ratio exceeds maxImageAspectRatio() (too tall/narrow or too wide/short to
         *  usefully present as an image) -- content-wise it is still isImage()==true (editable,
         *  has a real thumbnail/dimensions), it is only routed to a document-style row instead.
         *  A degenerate/unreadable pixelSize() never trips the ratio check.
         */
        bool presentAsImage() const;

        /**
         * @brief Get the configured extreme-aspect-ratio limit, see setMaxImageAspectRatio().
         */
        uint32_t maxImageAspectRatio() const noexcept
        {
            return m_maxImageAspectRatio;
        }

        /**
         * @brief Set the max(w,h)/min(w,h) limit past which presentAsImage() returns false for
         *  an otherwise-image item.
         * @param ratio 0 disables the check -- presentAsImage() then equals isImage(), as before
         *  this setting existed. A bare FileUploadItem constructed directly (not via a widget)
         *  defaults to 0/disabled; FileUploadWidget stamps its own DefaultMaxImageAspectRatio
         *  onto every item it creates instead, see
         *  AbstractFileUploadWidget::setMaxImageAspectRatio().
         */
        void setMaxImageAspectRatio(uint32_t ratio) noexcept
        {
            m_maxImageAspectRatio=ratio;
        }

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
         * @return explicitMimeType() if set; otherwise, for Type::ImageData, derived from
         *  imageFormat(); for Type::File, QMimeDatabase's guess for filePath() -- extension
         *  match falling back to content sniffing, which is unreliable for a file under an
         *  application-private extension it has never seen before (see setExplicitMimeType()).
         */
        QString mimeType() const;

        /**
         * @brief Override mimeType()/isImage()'s own detection with a known-correct value.
         * @param mime A real MIME type string (e.g. "application/octet-stream"), or empty to
         *  go back to automatic detection.
         *
         * For content this class did not decode itself -- a Type::File item wrapping a caller-
         * fabricated payload (not a file the user picked/dropped/pasted) -- QMimeDatabase's
         * guess can be wrong: an unregistered extension falls back to sniffing the file's raw
         * bytes, which can coincidentally match an unrelated format's magic bytes. Call this
         * when the caller already knows the true type, so mimeType()/isImage()/presentAsImage()
         * stop guessing and use it directly.
         */
        void setExplicitMimeType(QString mime)
        {
            m_explicitMimeType=std::move(mime);
        }

        /**
         * @brief Get the override set via setExplicitMimeType(), empty if none.
         */
        const QString& explicitMimeType() const noexcept
        {
            return m_explicitMimeType;
        }

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
         *
         * FileUploadWidget calls this itself, as soon as a row is displayed in View::Row (i.e.
         * presented as a plain document rather than an image -- whether via the extreme-aspect-
         * ratio guard or "send as documents"), so a pasted/generated image with no name still
         * gets a sensible one before the user ever sees the row. Callers building a send request
         * from items() directly (bypassing the widget) still need to call this themselves.
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
        QString m_explicitMimeType;

        QImage m_image;
        mutable QByteArray m_encoded;
        QByteArray m_format=QByteArray("PNG");

        mutable QSize m_pixelSize;
        mutable qint64 m_size=-1;
        uint32_t m_maxImageAspectRatio=0;
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
