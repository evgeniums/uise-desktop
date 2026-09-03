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

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

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

    Q_PROPERTY(int maxBubbleWidth READ maxBubbleWidth WRITE setMaxBubbleWidth)

    public:

        constexpr static const int DefaultMaxBubbleWidth=600;

        using AbstractChatMessageBody::AbstractChatMessageBody;

        //! Hard cap on the width of this message's bubble -- same idea, and same default, as
        //! AbstractChatMessageText::maxBubbleWidth (see its own doc comment). A file row's width
        //! is otherwise driven by its file-name label, whose natural width tracks the FULL,
        //! unelided file name -- an arbitrarily long name would otherwise inflate the bubble just
        //! as an arbitrarily long line of text would. 0 disables the cap. Settable from QSS via
        //! qproperty-maxBubbleWidth (see chatmessagefiles.qss).
        void setMaxBubbleWidth(int width) noexcept
        {
            m_maxBubbleWidth=width;
        }

        int maxBubbleWidth() const noexcept
        {
            return m_maxBubbleWidth;
        }

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

        virtual void setComment(const QString& text, TextFormat format=TextFormat::Markdown) =0;
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
         * @param sourceTag Opaque identity of the chat this drag originates from, forwarded to
         *  startFileUrlDrag() -- empty means no source restriction.
         *
         * Called by the host once it has resolved urls for that item's dragStartRequested().
         * A no-op if no item with this id is present.
         */
        virtual void startItemDrag(const QUuid& id, const QList<QUrl>& urls, const QString& sourceTag) =0;

    protected:

        //! Clamp a negotiation budget by maxBubbleWidth(), pass-through when the cap is disabled.
        int clampToMaxBubbleWidth(int width) const noexcept
        {
            return (m_maxBubbleWidth>0 && width>m_maxBubbleWidth) ? m_maxBubbleWidth : width;
        }

    private:

        int m_maxBubbleWidth=DefaultMaxBubbleWidth;

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

        /**
         * @brief Emitted for ChatFileMenuAction::CopyImage -- put the item's own image bytes on
         *  the clipboard. Declared here (not only on AbstractChatMessageImages) so the app's
         *  connectFilesBodySignals<BodyT>() template, shared verbatim by both this and
         *  AbstractChatMessageImages, keeps connecting an identical signal set on either body --
         *  buildChatFileMenuItems()'s own imageItem gate still makes sure only an image tile
         *  actually offers the menu row that reaches this signal.
         */
        void copyImageRequested(const QUuid& id);
};

}

#endif // UISE_DESKTOP_ABSTRACTCHATMESSAGEFILES_HPP
