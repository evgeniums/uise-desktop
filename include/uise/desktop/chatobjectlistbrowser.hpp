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

/** @file uise/desktop/chatobjectlistbrowser.hpp
*
*  Declares ChatObjectListBrowser, the browser that shows one row per chat object, and the
*  factories of the rows of the stock kinds: files, voice messages, links and mentions.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATOBJECTLISTBROWSER_HPP
#define UISE_DESKTOP_CHATOBJECTLISTBROWSER_HPP

#include <functional>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/chatobjectbrowser.hpp>

class QFrame;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot
// expand a macro-opened namespace, so it records tr() calls in this file under an unqualified
// context that does not match what moc (a real preprocessor) resolves at runtime -- translations
// for every string here would silently stay in English. Do not revert to the macro form. See
// task-localization-framework.md.
namespace uise {

//! Creates the widget of a row. The widget is parented to the list; keep a copy of the item in
//! it, and call the hooks with a COPY of that item (a hook may reload the browser).
using ChatObjectRowBuilder=std::function<QFrame* (const ChatObjectItem& item, QWidget* parent, const ChatObjectRowHooks& hooks)>;

//! Refreshes the widget of a row in place after ChatObjectListBrowser::updateItem().
using ChatObjectRowUpdater=std::function<void (QFrame* row, const ChatObjectItem& item)>;

/**
 * @brief How to build the rows of a kind. This is all a new kind needs, see
 *  ChatObjectBrowserRegistry.
 */
struct ChatObjectRowFactory
{
    ChatObjectRowBuilder build;

    //! Optional. Without it updateItem() replaces the row's widget by a new one, which can
    //! disturb the scroll position and is wasteful for something like a playback progress.
    ChatObjectRowUpdater update;
};

//! The widget behind the list; defined in the source file because it instantiates
//! FlyweightListView.
class ChatObjectListView;

/**
 * @brief Browser of the objects of one kind with a row per object, newest first.
 *
 * The rows are virtualised with a FlyweightListView: only the rows near the viewport exist as
 * widgets. What a row looks like is decided by the ChatObjectRowFactory.
 */
class UISE_DESKTOP_EXPORT ChatObjectListBrowser : public AbstractChatObjectBrowser
{
    Q_OBJECT

    public:

        ChatObjectListBrowser(QString kind, ChatObjectRowFactory factory, QWidget* parent=nullptr);

        /**
         * @brief The widget of the loaded object's row.
         * @return The row or nullptr if the object is not loaded or its row is not built at the
         *  moment (rows far from the viewport do not exist, and rows come and go as the list
         *  scrolls). For a frequent update of a row, e.g. a playback progress, look the row up on
         *  every tick instead of keeping the pointer.
         */
        QFrame* rowWidget(const ChatObjectKey& key) const;

    protected:

        void doReset() override;
        void doLoad(const std::vector<ChatObjectItem>& items, const ChatObjectKey& focus) override;
        void doRemove(const std::vector<ChatObjectKey>& keys) override;
        void doUpdate(const ChatObjectItem& item) override;
        void doEdgeArrived(bool older) override;
        void doGroupingChanged() override;

    private:

        void jumpToNewest();

        ChatObjectListView* m_listView;
};

//! Rows of the "file" kind: the usual file row of the chat with the size, date and sender.
UISE_DESKTOP_EXPORT ChatObjectRowFactory makeChatObjectFileRowFactory();

//! Rows of the "voice" kind: the voice message row with the sender and date as its title. Playing
//! is up to the host, see ChatObjectAction::Play and friends.
UISE_DESKTOP_EXPORT ChatObjectRowFactory makeChatObjectVoiceRowFactory();

//! Rows of the "link" kind: a link glyph, the title or url, the url, a snippet of the message.
UISE_DESKTOP_EXPORT ChatObjectRowFactory makeChatObjectLinkRowFactory();

//! Rows of the "mention" kind: sender avatar and name, date, a snippet of the message.
UISE_DESKTOP_EXPORT ChatObjectRowFactory makeChatObjectMentionRowFactory();

//! Browser of the "file" kind.
UISE_DESKTOP_EXPORT ChatObjectListBrowser* makeChatObjectFileBrowser(QWidget* parent=nullptr);

//! Browser of the "voice" kind.
UISE_DESKTOP_EXPORT ChatObjectListBrowser* makeChatObjectVoiceBrowser(QWidget* parent=nullptr);

//! Browser of the "link" kind.
UISE_DESKTOP_EXPORT ChatObjectListBrowser* makeChatObjectLinkBrowser(QWidget* parent=nullptr);

//! Browser of the "mention" kind; activating a row asks to show the message.
UISE_DESKTOP_EXPORT ChatObjectListBrowser* makeChatObjectMentionBrowser(QWidget* parent=nullptr);

} // namespace uise

#endif // UISE_DESKTOP_CHATOBJECTLISTBROWSER_HPP
