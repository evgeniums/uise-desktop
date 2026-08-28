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
#include <algorithm>

#include <QString>
#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QUuid>
#include <QMetaType>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractloadcontrol.hpp>
#include <uise/desktop/dropdownmenu.hpp>

class QWidget;

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
    Failed,      //!< Transfer failed; load control offers a retry.
    Complete,    //!< This item's own transfer just finished while sibling items in the same
                 //!< message have not -- once every item is done, the caller hides the load
                 //!< control entirely rather than leaving it in this state.
    Cancelled    //!< Transfer cancelled by the user -- deliberately distinct from Failed: this
                 //!< was on purpose, not an error, so no retry affordance is offered for it.
};

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
    ShowInFolder=4,
    OpenWith=5,       //!< labelled "Open in system app" -- unconditionally the OS default
                      //!< application, never a chooser (see chatfileitem.cpp's own comment)
    CopyFileName=6,
    Pause=7,
    Resume=8,
    Cancel=9,
    Download=10       //!< incoming, NotLoaded only -- start a fresh download from the menu,
                      //!< the same action clicking the load control triggers in that state.
                      //!< A distinct action from Resume (an existing paused/failed transfer)
                      //!< even though both end up calling the same underlying "start" path on
                      //!< the host side -- NotLoaded has no transfer to resume, so labelling
                      //!< it "Resume"/"Retry" would misdescribe it the same way those two
                      //!< already avoid misdescribing each other (see buildChatFileMenuItems()).
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
         * @brief Check whether preview() currently holds a low-resolution PLACEHOLDER rather than
         *  real, final-quality content.
         * @return True while preview() is a stand-in shown only until the real content resolves
         *  (e.g. a chat message's embedded ~128px thumbnail, before the `chat`/top rung arrives).
         *
         * Drives, together with sameAspect() (pixmapscale.hpp) comparing this placeholder's own
         * decoded size against pixelSize(): whether ChatMessageImageItem::updatePreview() crops
         * the preview to cover its content box (a framing mismatch -- a legacy/mobile-supplied
         * SQUARE thumbnail of a non-square original, see that function's own doc comment) or fits
         * it like real content, upscaled past its own resolution up to maxUpscale() times the
         * ORIGINAL's size -- blurry but recognisable beats a small stamp in a blank tile, or a
         * misrepresenting crop, while real content is never upscaled beyond that same bound, so a
         * genuinely small image still renders close to its own size. Also gates
         * ChatMessageImageItem::startDrag() (a placeholder is not worth using as a drag pixmap).
         * The host must clear this when it swaps in real content, otherwise that content is
         * treated as a placeholder too.
         */
        bool isPreviewPlaceholder() const noexcept
        {
            return m_previewPlaceholder;
        }

        void setPreviewPlaceholder(bool enable) noexcept
        {
            m_previewPlaceholder=enable;
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

        /**
         * @brief Get the encoded bytes of animated content for this item, when the host has them
         *  in memory.
         * @return Empty when the host has no animated content (the overwhelmingly common case) --
         *  the tile then renders preview()/localPath() exactly as if this were never called.
         *
         * The bytes-in-memory counterpart of localPath(): a host whose content lives in an
         * encrypted store, or that simply has no filesystem path to offer, can still drive
         * animation this way. Unlike localPath(), this is deliberately NOT gated on state() --
         * holding the bytes IS the availability proof. QByteArray is implicitly shared, so
         * carrying this on a value type copied per refresh costs only a refcount.
         */
        QByteArray animatedData() const
        {
            return m_animatedData;
        }

        //! Format hint for animatedData(), e.g. "gif"/"webp"; empty means "sniff from content".
        QByteArray animatedFormat() const
        {
            return m_animatedFormat;
        }

        /**
         * @brief Set the encoded bytes of animated content for this item.
         * @param data Raw encoded bytes, e.g. "GIF89a...". Copied and retained (QByteArray is
         *  implicitly shared) until cleared or replaced.
         * @param format Optional format hint forwarded to ImageLabel::setImageData().
         */
        void setAnimatedData(QByteArray data, QByteArray format={})
        {
            m_animatedData=std::move(data);
            m_animatedFormat=std::move(format);
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
         *
         * Only consulted by the default menu policy -- ignored once setMenuActions() has been
         * given a non-empty list, see its own docs.
         */
        bool isShowInFolderAvailable() const noexcept
        {
            return m_showInFolderAvailable;
        }

        void setShowInFolderAvailable(bool enable) noexcept
        {
            m_showInFolderAvailable=enable;
        }

        /**
         * @brief Set exactly which context-menu entries ChatMessageFileItem/ChatMessageImageItem
         *  should build for this item, and in what order.
         * @param actions Entries to show. ChatFileMenuAction::Pause/Resume/Cancel are still
         *  additionally filtered by state() -- see isChatFileCancellable() for Cancel's gate;
         *  listing an action that doesn't match state() is fine, it simply won't show.
         *
         * An empty list (the default) means "use the library's own default policy": Open, SaveAs,
         * Forward, plus ShowInFolder when isShowInFolderAvailable() -- i.e. today's menu, so every
         * existing caller is unaffected. A non-empty list is taken verbatim; the host is expected
         * to already know which entries make sense for this item -- e.g. whether an embedded
         * viewer/editor actually exists for this item's mime is the host's own concern, not this
         * class's, and gates whether OpenWith ("Open in system app") is listed at all. Listing
         * ChatFileMenuAction::Cancel is also what enables ChatMessageFileItem/ChatMessageImageItem's
         * always-visible Cancel control, not just the menu entry -- see isChatFileCancellable().
         */
        void setMenuActions(std::vector<ChatFileMenuAction> actions)
        {
            m_menuActions=std::move(actions);
        }

        const std::vector<ChatFileMenuAction>& menuActions() const noexcept
        {
            return m_menuActions;
        }

        /**
         * @brief Check if a given action is present in menuActions().
         */
        bool hasMenuAction(ChatFileMenuAction action) const
        {
            return std::find(m_menuActions.begin(),m_menuActions.end(),action)!=m_menuActions.end();
        }

    private:

        QUuid m_id;
        QString m_fileId;
        QString m_fileName;
        QString m_mimeType;
        qint64 m_size=0;
        QSize m_pixelSize;
        QImage m_preview;
        bool m_previewPlaceholder=false;
        QString m_localPath;
        QByteArray m_animatedData;
        QByteArray m_animatedFormat;
        ChatFileTransferState m_state=ChatFileTransferState::Ready;
        qint64 m_transferred=0;
        bool m_showInFolderAvailable=false;
        std::vector<ChatFileMenuAction> m_menuActions;
};

using ChatFileItems=std::vector<ChatFileItem>;

/**
 * @brief Map a chat file item's transfer state to the state of the AbstractLoadControl shown
 *  for it, as the single place this mapping is defined.
 * @param state Current transfer state of the item.
 * @param incoming Direction of the owning chat message (AbstractChatMessage::isIncoming()) --
 *  distinguishes Download from Upload for a NotLoaded item.
 * @return AbstractLoadControl::State::None is never returned by this mapping: Ready has no load
 *  control at all (the caller should hide it instead of mapping to None).
 */
UISE_DESKTOP_EXPORT AbstractLoadControl::State chatFileLoadControlState(ChatFileTransferState state, bool incoming);

/**
 * @brief Check whether a transfer in this state can be cancelled -- the single place this gate is
 *  defined, shared by buildChatFileMenuItems() (Cancel menu entry) and LoadControlMenu's own
 *  pause-or-cancel popup for the Running/Waiting states.
 * @param state Current transfer state of the item.
 * @return True for Pending/Running/Paused/Failed. Failed counts as cancellable alongside Resume's
 *  retry -- a permanently-failed item still needs a way to give up on it and remove it, not just
 *  retry it. False for Ready/NotLoaded/Complete/Cancelled.
 */
UISE_DESKTOP_EXPORT bool isChatFileCancellable(ChatFileTransferState state) noexcept;

/**
 * @brief Check whether the load control shown for an item in this state should react to clicks --
 *  the single place this gate is defined, applied by both ChatMessageFileItem::refresh() and
 *  ChatMessageImageItem::refresh() via LoadControl::setClickable().
 * @param state Current transfer state of the item.
 * @return False for Failed and Cancelled, where the control is a pure indicator that must not
 *  present itself as actionable: a permanently failed transfer is recovered through the menu's
 *  own Retry entry (see buildChatFileMenuItems()) rather than by clicking the control, and a
 *  cancelled one offers no click action at all. True otherwise.
 */
UISE_DESKTOP_EXPORT bool isChatFileLoadControlClickable(ChatFileTransferState state) noexcept;

/**
 * @brief Build the drop-down menu rows for one chat file/image item, the single place this is
 *  done so ChatMessageFileItem::rebuildMenu()/ChatMessageImageItem::rebuildMenu() cannot drift.
 * @param item Item the menu is being built for.
 * @param imageItem Reserved for a future per-kind divergence -- both item kinds currently share
 *  one default policy and one label/icon set.
 * @param incoming Direction of the owning message -- Pause/Resume/Cancel are labelled
 *  "...downloading"/"...sending" rather than a bare verb, mirroring LoadControlMenu's own
 *  pause-or-cancel popup, so pairing a verb with its icon can't later be misread as a
 *  media-playback control once the app grows an inline player for audio/video messages.
 * @param context Widget the icons will be painted in (for theme/mode resolution).
 * @return Rows built from item.menuActions() if non-empty (verbatim, in that order), else the
 *  default policy -- see ChatFileItem::setMenuActions(). ChatFileMenuAction::Pause/Resume are
 *  additionally filtered by item.state() so at most one of the pair is ever included.
 */
UISE_DESKTOP_EXPORT std::vector<MenuItem> buildChatFileMenuItems(const ChatFileItem& item, bool imageItem, bool incoming, QWidget* context);

UISE_DESKTOP_NAMESPACE_END

Q_DECLARE_METATYPE(UISE_DESKTOP_NAMESPACE::ChatFileItem)

#endif // UISE_DESKTOP_CHATFILEITEM_HPP
