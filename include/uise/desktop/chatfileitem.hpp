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

/** @file uise/desktop/chatfileitem.hpp
*
*  Declares ChatFileItem.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATFILEITEM_HPP
#define UISE_DESKTOP_CHATFILEITEM_HPP

#include <vector>

#include <QString>
#include <QImage>
#include <QSize>
#include <QUuid>
#include <QMetaType>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractloadcontrol.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Transfer state of one entry of a file/image chat message.
 *
 * Unlike FileUploadItem (always backed by a live file/image on this machine), a ChatFileItem
 * describes a message attachment that may not have any local content yet -- an incoming file
 * only known from its server-side descriptor, or an outgoing one still queued for upload.
 */
enum class ChatFileTransferState : uint8_t
{
    Ready,       //!< Content available locally (local path/preview valid); no load control shown.
    NotLoaded,   //!< Not transferred yet; load control offers download (incoming) / upload (outgoing).
    Pending,     //!< Queued, waiting for its turn.
    Running,     //!< Transfer in progress, see transferred()/size().
    Paused,      //!< Transfer paused by the user.
    Failed       //!< Transfer failed; load control offers a retry.
};

/**
 * @brief One entry of a file or image chat message.
 *
 * A plain, cheaply-copyable value type (QString/QImage are implicitly shared) -- no pimpl,
 * modelled after FileUploadItem. Unlike FileUploadItem, every field here is a plain stored
 * property rather than something derived from a live filesystem path: the data comes from
 * wherever the host's file service resolved it (a local descriptor, a server-side attachment
 * record), which this widget layer knows nothing about.
 *
 * id() is a locally-minted, stable identity for list bookkeeping (e.g. matching a widget row to
 * an item across a setItems()/updateItem() refresh) -- preserved across copies, distinct from
 * fileId(), the host's own opaque identifier for the underlying file.
 */
class UISE_DESKTOP_EXPORT ChatFileItem
{
    public:

        ChatFileItem()
            : m_id(QUuid::createUuid())
        {}

        QUuid id() const noexcept
        {
            return m_id;
        }

        QString fileId() const
        {
            return m_fileId;
        }

        void setFileId(QString fileId)
        {
            m_fileId=std::move(fileId);
        }

        QString fileName() const
        {
            return m_fileName;
        }

        void setFileName(QString name)
        {
            m_fileName=std::move(name);
        }

        QString suffix() const;

        /**
         * @brief Get the MIME type, e.g. "image/png".
         * @return The value set via setMimeType(), or, if never set, sniffed from fileName()'s
         *  suffix.
         */
        QString mimeType() const;

        void setMimeType(QString mimeType)
        {
            m_mimeType=std::move(mimeType);
        }

        /**
         * @brief Check if this item is an image, from mimeType().
         */
        bool isImage() const
        {
            return mimeType().startsWith(QStringLiteral("image/"));
        }

        qint64 size() const noexcept
        {
            return m_size;
        }

        void setSize(qint64 size) noexcept
        {
            m_size=size;
        }

        /**
         * @brief Get the image's pixel dimensions, known from the attachment's metadata even
         *  before any content is transferred locally.
         * @return An invalid QSize if not known / not an image.
         */
        QSize pixelSize() const noexcept
        {
            return m_pixelSize;
        }

        void setPixelSize(QSize size) noexcept
        {
            m_pixelSize=size;
        }

        /**
         * @brief Get the decoded preview image, if one has been supplied.
         * @return A possibly-null QImage -- previews are decoded off-thread by the host and
         *  handed in via setPreview(), never decoded from localPath() by this class itself.
         */
        QImage preview() const
        {
            return m_preview;
        }

        void setPreview(QImage preview)
        {
            m_preview=std::move(preview);
        }

        /**
         * @brief Get the local filesystem path of the transferred content.
         * @return Empty unless state() is Ready and the host has resolved a local path (e.g.
         *  via an export).
         */
        QString localPath() const
        {
            return m_localPath;
        }

        void setLocalPath(QString path)
        {
            m_localPath=std::move(path);
        }

        ChatFileTransferState state() const noexcept
        {
            return m_state;
        }

        void setState(ChatFileTransferState state) noexcept
        {
            m_state=state;
        }

        /**
         * @brief Get the number of bytes transferred so far.
         * @return Meaningful only while state() is Running; see size() for the total.
         */
        qint64 transferred() const noexcept
        {
            return m_transferred;
        }

        void setTransferred(qint64 transferred) noexcept
        {
            m_transferred=transferred;
        }

        /**
         * @brief Check whether "Show in folder" should be offered for this item.
         * @return false unless the host has a real filesystem location to reveal (the brief
         *  marks this menu entry optional).
         */
        bool isShowInFolderAvailable() const noexcept
        {
            return m_showInFolderAvailable;
        }

        void setShowInFolderAvailable(bool enable) noexcept
        {
            m_showInFolderAvailable=enable;
        }

    private:

        QUuid m_id;
        QString m_fileId;
        QString m_fileName;
        QString m_mimeType;
        qint64 m_size=0;
        QSize m_pixelSize;
        QImage m_preview;
        QString m_localPath;
        ChatFileTransferState m_state=ChatFileTransferState::Ready;
        qint64 m_transferred=0;
        bool m_showInFolderAvailable=false;
};

using ChatFileItems=std::vector<ChatFileItem>;

/**
 * @brief Ids of the drop-down menu entries offered by ChatMessageFileItem/ChatMessageImageItem,
 *  shared between both so a host can handle AbstractChatMessageFiles/AbstractChatMessageImages
 *  uniformly.
 */
enum class ChatFileMenuAction : int
{
    Open=1,
    SaveAs=2,
    Forward=3,
    ShowInFolder=4
};

/**
 * @brief Map a chat file item's transfer state to the state of the AbstractLoadControl shown
 *  for it, as the single place this mapping is defined.
 * @param state Current transfer state of the item.
 * @param incoming Direction of the owning chat message (AbstractChatMessage::isIncoming()) --
 *  distinguishes CanDownload from CanUpload for a NotLoaded item.
 * @return AbstractLoadControl::State::None is never returned by this mapping: Ready has no load
 *  control at all (the caller should hide it instead of mapping to None).
 */
UISE_DESKTOP_EXPORT AbstractLoadControl::State chatFileLoadControlState(ChatFileTransferState state, bool incoming);

UISE_DESKTOP_NAMESPACE_END

Q_DECLARE_METATYPE(UISE_DESKTOP_NAMESPACE::ChatFileItem)

#endif // UISE_DESKTOP_CHATFILEITEM_HPP
