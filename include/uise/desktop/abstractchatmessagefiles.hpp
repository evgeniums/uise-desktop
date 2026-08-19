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

/** @file uise/desktop/abstractchatmessagefiles.hpp
*
*  Declares AbstractChatMessageFiles.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTCHATMESSAGEFILES_HPP
#define UISE_DESKTOP_ABSTRACTCHATMESSAGEFILES_HPP

#include <QUuid>
#include <QList>
#include <QUrl>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractchatmessage.hpp>
#include <uise/desktop/chatfileitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Interface of a file chat message body: a vertical list of file items plus an optional
 *  text comment, as described in tasks/chat-message-file-type.md.
 *
 * Items are addressed by ChatFileItem::id() (a locally-minted, stable QUuid), not by index --
 * the host is expected to hold its own file/transfer state and just push updates through
 * updateItem() as transfers progress, without having to track list positions.
 */
class UISE_DESKTOP_EXPORT AbstractChatMessageFiles : public AbstractChatMessageBody
{
    Q_OBJECT

    public:

        using AbstractChatMessageBody::AbstractChatMessageBody;

        virtual void setItems(ChatFileItems items) =0;
        virtual const ChatFileItems& items() const =0;

        /**
         * @brief Refresh one item in place, e.g. a progress tick, without rebuilding the list.
         * @param id Id of the item to update (see ChatFileItem::id()).
         * @param item New content for that item.
         *
         * A no-op if no item with this id is present.
         */
        virtual void updateItem(const QUuid& id, const ChatFileItem& item) =0;

        virtual void setComment(const QString& text, bool markdown=true) =0;
        virtual void clearComment() =0;
        virtual QString comment() const =0;

        /**
         * @brief Close every open per-item drop-down menu, without animation.
         *
         * Meant for a host embedding this body in a scrolling list to call whenever the list
         * scrolls -- see ChatMessageFileItem::closeMenu().
         */
        virtual void closeMenus() =0;

        /**
         * @brief Set how every row's file-name/size two-line block is aligned within its height.
         * @param alignment Forwarded to ChatMessageFileItem::setTextVerticalAlignment() -- see
         *  its docs for the accepted values and their meaning.
         */
        virtual void setTextVerticalAlignment(Qt::Alignment alignment) =0;

        virtual Qt::Alignment textVerticalAlignment() const =0;

        /**
         * @brief Start an outgoing QDrag for one item, carrying urls.
         * @param id Id of the item to drag (see ChatFileItem::id()).
         * @param urls Local file URLs already resolved by the host -- this call does not
         *  resolve/export/decrypt anything itself, see dragStartRequested().
         *
         * Called by the host once it has resolved urls for that item's dragStartRequested().
         * A no-op if no item with this id is present.
         */
        virtual void startItemDrag(const QUuid& id, const QList<QUrl>& urls) =0;

    signals:

        /**
         * @brief Emitted when an item's icon/preview or its file name is clicked.
         */
        void itemClicked(const QUuid& id);

        void loadControlClicked(const QUuid& id);

        /**
         * @brief Emitted on press, before it is known whether the gesture turns into a drag or
         *  a click -- the host's cue to start resolving/exporting this item's content so it is
         *  ready by the time dragStartRequested() (if any) arrives.
         */
        void dragPrepareRequested(const QUuid& id);

        /**
         * @brief Emitted once the press has moved past the drag threshold. The host is expected
         *  to call startItemDrag() with whatever urls dragPrepareRequested() resolved, or do
         *  nothing if they are not ready yet.
         */
        void dragStartRequested(const QUuid& id);

        void openRequested(const QUuid& id);
        void openWithRequested(const QUuid& id);
        void saveAsRequested(const QUuid& id);
        void forwardRequested(const QUuid& id);
        void showInFolderRequested(const QUuid& id);
        void copyFileNameRequested(const QUuid& id);
        void pauseRequested(const QUuid& id);
        void resumeRequested(const QUuid& id);
        void cancelRequested(const QUuid& id);

        /**
         * @brief Emitted for ChatFileMenuAction::Download -- start a fresh download for an
         *  item that has never been attempted (state() NotLoaded). Distinct from
         *  resumeRequested(), which addresses an existing paused/failed transfer.
         */
        void downloadRequested(const QUuid& id);
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACTCHATMESSAGEFILES_HPP
