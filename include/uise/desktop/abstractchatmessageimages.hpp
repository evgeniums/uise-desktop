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

/** @file uise/desktop/abstractchatmessageimages.hpp
*
*  Declares AbstractChatMessageImages.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTCHATMESSAGEIMAGES_HPP
#define UISE_DESKTOP_ABSTRACTCHATMESSAGEIMAGES_HPP

#include <QUuid>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractchatmessage.hpp>
#include <uise/desktop/chatfileitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Interface of an image chat message body: an album grid of image tiles (see
 *  albumLayout()) plus an optional text comment, as described in
 *  tasks/chat-message-file-type.md.
 *
 * Mirrors AbstractChatMessageFiles exactly (same item-update/comment/menu-close shape, items
 * addressed by ChatFileItem::id()) so a host can treat both bodies uniformly; the only
 * difference is openWithRequested() in place of openRequested(), matching the brief's "Open
 * with" wording for image items versus plain "Open" for file items.
 */
class UISE_DESKTOP_EXPORT AbstractChatMessageImages : public AbstractChatMessageBody
{
    Q_OBJECT

    public:

        using AbstractChatMessageBody::AbstractChatMessageBody;

        virtual void setItems(ChatFileItems items) =0;
        virtual const ChatFileItems& items() const =0;

        /**
         * @brief Refresh one item in place, e.g. a progress tick, without rebuilding the grid.
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
         * See AbstractChatMessageFiles::closeMenus() -- same rationale.
         */
        virtual void closeMenus() =0;

    signals:

        /**
         * @brief Emitted when an item's preview is clicked.
         */
        void itemClicked(const QUuid& id);

        void loadControlClicked(const QUuid& id);

        void openWithRequested(const QUuid& id);
        void saveAsRequested(const QUuid& id);
        void forwardRequested(const QUuid& id);
        void showInFolderRequested(const QUuid& id);
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACTCHATMESSAGEIMAGES_HPP
