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

/** @file uise/desktop/chatobjectbrowser.hpp
*
*  Declares ChatObjectBrowserModel, ChatObjectRowHooks and AbstractChatObjectBrowser, the common
*  part of the chat object browsers. See docs/chat-object-browsers.md.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATOBJECTBROWSER_HPP
#define UISE_DESKTOP_CHATOBJECTBROWSER_HPP

#include <cstddef>
#include <functional>
#include <vector>

#include <QPoint>
#include <QPointer>
#include <QString>
#include <QVariant>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/framewithrefresh.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/chatobjectitem.hpp>

class QFrame;
class QLabel;
class QTimer;
class QVBoxLayout;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot
// expand a macro-opened namespace, so it records tr() calls in this file under an unqualified
// context that does not match what moc (a real preprocessor) resolves at runtime -- translations
// for every string here would silently stay in English. Do not revert to the macro form. See
// task-localization-framework.md.
namespace uise {

class SearchLineEdit;

/**
 * @brief The month of a date as one number, for grouping by month; -1 for an invalid date.
 *
 * The month in the local time zone, as the user sees the dates.
 */
UISE_DESKTOP_EXPORT int chatObjectMonthKey(const QDateTime& dateTime);

/**
 * @brief Title of the month group of a date, e.g. "October 2026", in the current locale; empty for
 *  an invalid date.
 */
UISE_DESKTOP_EXPORT QString chatObjectMonthTitle(const QDateTime& dateTime);

/**
 * @brief Names of the generic actions a row can ask the owner for, see
 *  AbstractChatObjectBrowser::itemActionRequested().
 */
namespace ChatObjectAction
{
    //! The load control of a file row was clicked (start, pause or retry a transfer).
    inline const QString LoadControl=QStringLiteral("loadControl");
    //! Pause was requested for a running transfer.
    inline const QString Pause=QStringLiteral("pause");
    //! Cancel was requested for a transfer.
    inline const QString Cancel=QStringLiteral("cancel");
    //! The built-in menu of a file row was used; the argument is an int, a ChatFileMenuAction.
    inline const QString FileMenu=QStringLiteral("fileMenu");
    //! Play a voice (or audio) object.
    inline const QString Play=QStringLiteral("play");
    //! Stop (pause) a playing voice object.
    inline const QString Stop=QStringLiteral("stop");
    //! Seek a voice object; the argument is a qreal fraction in [0,1].
    inline const QString Seek=QStringLiteral("seek");
    //! A row put a text (a link, the text of a message) to the clipboard; the argument is the text. The owner may
    //! tell the user.
    inline const QString Copied=QStringLiteral("copied");
}

/**
 * @brief The browsing state of one kind: the objects loaded so far and which ends are known.
 *
 * Plain data structure, no widgets, so it is tested on its own. Items are kept sorted by key,
 * ascending (older first), unique by key. "Older" and "newer" are in terms of keys; the widgets
 * show newest first.
 */
class UISE_DESKTOP_EXPORT ChatObjectBrowserModel
{
    public:

        struct MergeResult
        {
            //! Items whose key was not known, ascending.
            std::vector<ChatObjectItem> added;
            //! Items whose key was known and whose content was replaced, ascending.
            std::vector<ChatObjectItem> updated;
        };

        void clear();

        size_t size() const noexcept
        {
            return m_items.size();
        }

        bool empty() const noexcept
        {
            return m_items.empty();
        }

        //! All items, ascending (oldest first).
        const std::vector<ChatObjectItem>& items() const noexcept
        {
            return m_items;
        }

        //! The oldest item or nullptr.
        const ChatObjectItem* oldest() const noexcept;

        //! The newest item or nullptr.
        const ChatObjectItem* newest() const noexcept;

        //! The item with the key or nullptr.
        const ChatObjectItem* find(const ChatObjectKey& key) const;

        //! Index in items() of the first item whose key is not less than the given one.
        size_t lowerBound(const ChatObjectKey& key) const;

        //! Add new items and replace the content of the known ones. Duplicates inside the
        //! argument are dropped, the first one wins.
        MergeResult merge(std::vector<ChatObjectItem> incoming);

        //! Replace the content of the item with the same key. False if it is not there.
        bool replace(const ChatObjectItem& item);

        //! Remove the item. False if it is not there.
        bool remove(const ChatObjectKey& key);

        //! Remove all items of the message and return their keys, ascending.
        std::vector<ChatObjectKey> removeMessage(const QString& messageId);

        /**
         * @brief Whether the item is the first of its month in the widgets' order, i.e. the
         *  newest one of its month among the loaded items.
         *
         * True for the newest loaded item whatever its month: a newer one of the same month may
         * still arrive, and the browser then asks again. False if the key is not loaded.
         */
        bool startsMonth(const ChatObjectKey& key) const;

        /**
         * @brief Up to count items immediately older than the key, nearest first, i.e. in
         *  descending key order -- which is the order the widgets show them in.
         */
        std::vector<ChatObjectItem> olderThan(const ChatObjectKey& key, size_t count) const;

        /**
         * @brief Up to count items immediately newer than the key, nearest first, i.e. in
         *  ascending key order. Reverse it to get the widgets' order.
         */
        std::vector<ChatObjectItem> newerThan(const ChatObjectKey& key, size_t count) const;

        //! No older items exist anywhere, the owner said so.
        bool isEndOlder() const noexcept
        {
            return m_endOlder;
        }

        //! No newer items exist anywhere, the owner said so (or the request had a null anchor).
        bool isEndNewer() const noexcept
        {
            return m_endNewer;
        }

        void setEndOlder(bool enable) noexcept
        {
            m_endOlder=enable;
        }

        void setEndNewer(bool enable) noexcept
        {
            m_endNewer=enable;
        }

    private:

        std::vector<ChatObjectItem> m_items;
        bool m_endOlder=false;
        bool m_endNewer=false;
};

/**
 * @brief What a row widget may call back. Handed to the row builders by the browser.
 *
 * Every member may be empty; call through the std::function only after checking it.
 */
struct ChatObjectRowHooks
{
    //! The row was activated (clicked). The browser maps it to itemActivated() or, for kinds
    //! whose object IS the message (mentions), to goToMessageRequested().
    std::function<void (const ChatObjectItem&)> activated;

    //! The row asks to show its message in the chat.
    std::function<void (const ChatObjectItem&)> goToMessage;

    //! The row asks for the browser's context menu at a global position.
    std::function<void (const ChatObjectItem&, const QPoint&)> contextMenu;

    //! Anything else, see ChatObjectAction.
    std::function<void (const ChatObjectItem&, const QString&, const QVariant&)> action;
};

/**
 * @brief Base class of the browsers of the objects of one kind of one chat.
 *
 * Owns everything that does not depend on how the objects are laid out: the keyset paging state
 * machine, the model, the empty state, the context menu and the signals. A derived class owns the
 * view (a FlyweightListView) and implements the do*() hooks.
 *
 * The browser is data-driven. It never reads a database: when it needs objects it calls the
 * request callback with a ChatObjectsRequest, and the owner answers whenever it can -- later, from
 * another thread's result, after a network trip -- with applyPage(). Counts are not computed here.
 *
 * Paging is keyset: the browser asks for "N objects before/after this key", never for a page
 * number or an offset, and never learns a total. The owner says explicitly when there is nothing
 * more (ChatObjectsPage::endBefore/endAfter). At most one request per side is in flight. Items
 * that were loaded stay in the model for the lifetime of the browser, so scrolling back over
 * loaded ground is served locally and only an unexhausted edge asks the owner; the model is
 * therefore memory-resident (small structs, the widgets stay flyweight).
 *
 * The view shows the newest object first.
 */
class UISE_DESKTOP_EXPORT AbstractChatObjectBrowser : public FrameWithRefresh
{
    Q_OBJECT

    public:

        using RequestCb=std::function<void (const ChatObjectsRequest&)>;
        using ContextMenuBuilder=std::function<std::vector<MenuItem> (const ChatObjectItem&)>;

        //! Ids of the built-in context menu entries.
        constexpr static const int MenuOpen=1;
        constexpr static const int MenuShowInChat=2;

        //! Ids of the entries added by a ContextMenuBuilder must be not less than this.
        constexpr static const int MenuFirstCustomId=100;

        //! Default number of objects requested at a time.
        constexpr static const size_t DefaultPageCount=30;

        explicit AbstractChatObjectBrowser(QString kind, QWidget* parent=nullptr);

        ~AbstractChatObjectBrowser() override;

        AbstractChatObjectBrowser(const AbstractChatObjectBrowser&)=delete;
        AbstractChatObjectBrowser(AbstractChatObjectBrowser&&)=delete;
        AbstractChatObjectBrowser& operator=(const AbstractChatObjectBrowser&)=delete;
        AbstractChatObjectBrowser& operator=(AbstractChatObjectBrowser&&)=delete;

        const QString& kind() const noexcept
        {
            return m_kind;
        }

        /**
         * @brief Set the callback that gets the browser's requests.
         *
         * Must be set before reload() or openAt(). The callback may answer synchronously
         * (applyPage() from inside it is supported) or later.
         */
        void setRequestCb(RequestCb cb);

        //! Entries to add to the built-in context menu. See MenuFirstCustomId.
        void setContextMenuBuilder(ContextMenuBuilder builder);

        //! Text shown instead of the view when the kind has no objects at all.
        void setEmptyText(const QString& text);

        QString emptyText() const;

        //! Number of objects requested at a time; 0 resets to DefaultPageCount.
        void setPageCount(size_t count) noexcept;

        size_t pageCount() const noexcept
        {
            return m_pageCount;
        }

        /**
         * @brief Make activation of an item mean "show the message".
         *
         * For kinds whose object IS the message (mentions): a click then emits
         * goToMessageRequested() instead of itemActivated(), and the context menu has "Go to message"
         * alone, without the redundant "Open".
         */
        void setActivationGoesToMessage(bool enable) noexcept;

        /**
         * @brief Group the objects by the month of their date.
         *
         * A heading with the month ("October 2026") is shown above the first object of every
         * month. On by default. The month comes from ChatObjectItem::dateTime in the local time
         * zone; an object without a valid date belongs to no month and gets no heading. The
         * grouping follows the order of the keys, so it assumes that keys and dates run the same
         * way.
         */
        void setGroupByMonth(bool enable);

        bool isGroupByMonth() const noexcept
        {
            return m_groupByMonth;
        }

        /**
         * @brief Show or hide the search line above the objects.
         *
         * Hiding it also drops the search: a text that is in effect is cleared and the browser
         * reloads without it.
         */
        void setSearchVisible(bool enable);

        bool isSearchVisible() const noexcept;

        /**
         * @brief Search for a text, as if it was typed into the line, but at once.
         *
         * An empty text ends the search. A change of the text reloads the browser; the owner
         * gets the text as ChatObjectsRequest::query and answers with the matching objects.
         * Typing in the line does the same after a short pause.
         */
        void setSearchText(const QString& text);

        //! The text of the search that is in effect.
        const QString& searchText() const noexcept
        {
            return m_query;
        }

        /**
         * @brief Ask for objects only while there is a search text (the "search results" kind).
         *
         * When on and the text is empty, reload() sends NO request and the browser shows its empty text (the hint, e.g.
         * "Type to search messages") instead of objects. Clearing the text returns to the hint at once and drops the requests
         * in flight; their answers are ignored. A text that finds nothing still shows "Nothing found". Off by default.
         * Changing it after the first load reloads.
         */
        void setQueryRequired(bool enable);

        bool isQueryRequired() const noexcept
        {
            return m_queryRequired;
        }

        void setSearchPlaceholderText(const QString& text);

        //! Put the cursor into the search line, if it is shown.
        void focusSearch();

        /**
         * @brief Drop everything and load from the newest end.
         *
         * Also what to call when the owner learns that the set of objects changed in a way the
         * browser cannot patch (new objects appeared, counters were updated). Requests in flight
         * are forgotten, their answers will be dropped.
         */
        void reload();

        /**
         * @brief Drop everything and load around the key: the key itself, older and newer ones.
         *
         * For "show this object in the list", e.g. the item that was just viewed.
         */
        void openAt(const ChatObjectKey& key);

        /**
         * @brief Answer to a request of this browser.
         *
         * An answer to an unknown request, including one superseded by reload() or openAt(), is
         * silently dropped.
         */
        void applyPage(const ChatObjectsPage& page);

        //! Remove all objects of the message, e.g. when the message was deleted.
        void removeMessageObjects(const QString& messageId);

        //! Replace the content of the loaded object with the same key and refresh its widget,
        //! e.g. a file's transfer state or a voice message being played. No-op if not loaded.
        void updateItem(const ChatObjectItem& item);

        const ChatObjectBrowserModel& model() const noexcept
        {
            return m_model;
        }

        //! Whether a request is in flight.
        bool isLoading() const noexcept;

    signals:

        //! An object was activated: a file or a thumbnail was clicked, "Open" was chosen.
        void itemActivated(const ChatObjectItem& item);

        //! The message of an object should be opened in the chat.
        void goToMessageRequested(const QString& messageId);

        //! An entry added with setContextMenuBuilder() was chosen.
        void contextMenuTriggered(const ChatObjectItem& item, int id);

        //! A row asked for something else, see ChatObjectAction.
        void itemActionRequested(const ChatObjectItem& item, const QString& action, const QVariant& argument);

        //! The search text in effect changed; the browser is reloading with it.
        void searchTextChanged(const QString& text);

    protected:

        //! Remove everything from the view.
        virtual void doReset()=0;

        /**
         * @brief Replace the content of the view.
         * @param items The model's items in the widgets' order, newest first.
         * @param focus Key to bring into view at the top, or a null key to stay at the newest end.
         */
        virtual void doLoad(const std::vector<ChatObjectItem>& items, const ChatObjectKey& focus)=0;

        //! Remove the objects from the view; they are already gone from the model.
        virtual void doRemove(const std::vector<ChatObjectKey>& keys)=0;

        //! Refresh the widget of the object; the model already holds the new content.
        virtual void doUpdate(const ChatObjectItem& item)=0;

        /**
         * @brief The model grew on one side because an owner's answer arrived.
         *
         * The view must now take from the model what it was waiting for, by calling serveEdge()
         * with its own edge item as the anchor, exactly as it does when its scrolling asks for
         * more. Never insert the new items directly: the model may hold items between the view's
         * edge and the new ones, and the view must stay gap-free.
         */
        virtual void doEdgeArrived(bool older)=0;

        /**
         * @brief isGroupByMonth() changed.
         *
         * The default reloads the view from the model at the newest end. A view that can do it
         * without losing the place overrides it.
         */
        virtual void doGroupingChanged();

        void doRefresh() override;

        //! Put the view widget into the browser. Call once from the derived constructor.
        void setView(QWidget* view);

        /**
         * @brief Items for the view's edge, in the widgets' order (newest first).
         * @param anchor Key of the view's outermost item on that side.
         * @param count How many are wanted.
         * @param older The older side (scrolling down) or the newer side (scrolling up).
         *
         * Takes the items from the model and, if the model has fewer than asked for and that side
         * is not known to be exhausted, requests more from the owner (once; the answer ends up in
         * doEdgeArrived()). May return fewer than count, even none.
         */
        std::vector<ChatObjectItem> serveEdge(const ChatObjectKey& anchor, size_t count, bool older);

        //! Hooks for the row builders of the derived class.
        ChatObjectRowHooks rowHooks();

        //! Open the built-in context menu for the item.
        void showContextMenu(const ChatObjectItem& item, const QPoint& globalPos);

        //! Show the empty text instead of the view if there is nothing and nothing to wait for.
        void updateEmptyState();

    private:

        struct Pending
        {
            quint64 id=0;
            bool nullAnchor=false;
            ChatObjectKey focus;
        };

        void resetState();
        void sendRequest(ChatObjectsRequest request, Pending& slot);
        void requestEdge(bool older);
        void handleResetPage(const ChatObjectsPage& page);
        void handleEdgePage(const ChatObjectsPage& page, bool older);
        void activate(const ChatObjectItem& item);
        void onMenuTriggered(int id);
        void applySearch();

        QString m_kind;
        ChatObjectBrowserModel m_model;
        RequestCb m_requestCb;
        ContextMenuBuilder m_menuBuilder;
        size_t m_pageCount;

        Pending m_pendingReset;
        Pending m_pendingOlder;
        Pending m_pendingNewer;
        bool m_started;
        bool m_activationGoesToMessage;
        bool m_groupByMonth;
        bool m_queryRequired;
        QString m_query;
        QString m_emptyText;

        QVBoxLayout* m_layout;
        //! Holds the search line and the room around it. The line has no margin of its own: LineEdit puts its icons by the
        //! whole rect of the widget, so a margin on the line puts them on the edge of the box, not inside it.
        QFrame* m_searchBar;
        SearchLineEdit* m_searchEdit;
        QTimer* m_searchTimer;
        QLabel* m_emptyLabel;
        QWidget* m_view;
        QPointer<DropdownMenu> m_menu;
        ChatObjectItem m_menuItem;
};

} // namespace uise

#endif // UISE_DESKTOP_CHATOBJECTBROWSER_HPP
