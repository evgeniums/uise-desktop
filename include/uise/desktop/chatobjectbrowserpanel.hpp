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

/** @file uise/desktop/chatobjectbrowserpanel.hpp
*
*  Declares ChatObjectBrowserRegistry and ChatObjectBrowserPanel: the kind to browser registry and
*  the container that shows one browser at a time under a back/title bar.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATOBJECTBROWSERPANEL_HPP
#define UISE_DESKTOP_CHATOBJECTBROWSERPANEL_HPP

#include <functional>
#include <utility>
#include <vector>

#include <QPointer>
#include <QString>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/framewithrefresh.hpp>
#include <uise/desktop/chatobjectbrowser.hpp>

class QBoxLayout;
class QLabel;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot
// expand a macro-opened namespace, so it records tr() calls in this file under an unqualified
// context that does not match what moc (a real preprocessor) resolves at runtime -- translations
// for every string here would silently stay in English. Do not revert to the macro form. See
// task-localization-framework.md.
namespace uise {

class PushButton;
class ElidedLabel;

/**
 * @brief What the registry knows about one kind of chat objects.
 *
 * The texts are functions, not strings, so that they are translated when asked for and follow a
 * change of the language.
 */
struct ChatObjectBrowserKindInfo
{
    //! Title of the panel while the browser of the kind is shown, e.g. "Images".
    std::function<QString ()> title;

    //! Text of the browser when there are no objects of the kind in the chat.
    std::function<QString ()> emptyText;

    //! Creates the browser of the kind.
    std::function<AbstractChatObjectBrowser* (QWidget* parent)> factory;
};

/**
 * @brief Maps a kind string to the browser that shows its objects.
 *
 * The set of kinds is open: a new kind (e.g. "video") is registered, not wired in. Usually the
 * factory creates a ChatObjectListBrowser with a ChatObjectRowFactory of its own, or a
 * ChatObjectGalleryBrowser.
 */
class UISE_DESKTOP_EXPORT ChatObjectBrowserRegistry
{
    public:

        //! Register a kind; a kind that is registered already is replaced.
        void registerKind(const QString& kind, ChatObjectBrowserKindInfo info);

        //! Remove a kind. False if it was not registered.
        bool unregisterKind(const QString& kind);

        //! Info of the kind or nullptr.
        const ChatObjectBrowserKindInfo* find(const QString& kind) const;

        //! Registered kinds in the order of registration.
        std::vector<QString> kinds() const;

        /**
         * @brief Registry with the stock kinds: image (the thumbnail grid), file, voice, link
         *  and mention (lists).
         */
        static ChatObjectBrowserRegistry defaults();

    private:

        std::vector<std::pair<QString,ChatObjectBrowserKindInfo>> m_kinds;
};

/**
 * @brief Shows the browser of one kind at a time under a bar with a back button and a title.
 *
 * The browser of a kind is created by the registry's factory when open() is called and destroyed
 * when another one is opened or the panel is closed with closeBrowser(). The panel does not
 * navigate: backRequested() tells the owner that the user wants to leave, and the owner decides
 * what is shown next (typically it goes back to the counters of the chat info). It is a
 * FrameWithRefresh, so it can be a page of a StackWithNavigationBar; in that case the owner
 * connects backRequested() to the stack's truncate and may hide the back button with
 * setBackButtonVisible().
 *
 * The signals of the current browser are forwarded, so the owner connects once to the panel.
 */
class UISE_DESKTOP_EXPORT ChatObjectBrowserPanel : public FrameWithRefresh
{
    Q_OBJECT

    public:

        explicit ChatObjectBrowserPanel(QWidget* parent=nullptr);

        ~ChatObjectBrowserPanel() override;

        ChatObjectBrowserPanel(const ChatObjectBrowserPanel&)=delete;
        ChatObjectBrowserPanel(ChatObjectBrowserPanel&&)=delete;
        ChatObjectBrowserPanel& operator=(const ChatObjectBrowserPanel&)=delete;
        ChatObjectBrowserPanel& operator=(ChatObjectBrowserPanel&&)=delete;

        //! The registry is ChatObjectBrowserRegistry::defaults() until replaced.
        void setRegistry(ChatObjectBrowserRegistry registry);

        const ChatObjectBrowserRegistry& registry() const noexcept
        {
            return m_registry;
        }

        //! Request callback given to every browser the panel creates.
        void setRequestCb(AbstractChatObjectBrowser::RequestCb cb);

        //! Context menu builder given to every browser the panel creates.
        void setContextMenuBuilder(AbstractChatObjectBrowser::ContextMenuBuilder builder);

        /**
         * @brief Show the browser of the kind and load it.
         * @param kind Registered kind.
         * @param anchor Object to open around, or a null key to start from the newest.
         * @return The new browser, or nullptr for a kind that is not registered (the panel then
         *  says so instead).
         */
        AbstractChatObjectBrowser* open(const QString& kind, const ChatObjectKey& anchor=ChatObjectKey{});

        //! Destroy the current browser.
        void closeBrowser();

        //! The current browser or nullptr.
        AbstractChatObjectBrowser* currentBrowser() const noexcept;

        //! Kind of the current browser, empty if there is none.
        const QString& currentKind() const noexcept
        {
            return m_kind;
        }

        /**
         * @brief Replace the title of the bar until the next open().
         *
         * For a title with a count, "Images (123)", which the panel does not know.
         */
        void setTitle(const QString& title);

        QString title() const;

        void setBackButtonVisible(bool enable);

        //! Put a widget at the right end of the bar.
        void addTrailingWidget(QWidget* widget);

    signals:

        //! The user wants to leave the browser.
        void backRequested();

        void itemActivated(const ChatObjectItem& item);
        void goToMessageRequested(const QString& messageId);
        void contextMenuTriggered(const ChatObjectItem& item, int id);
        void itemActionRequested(const ChatObjectItem& item, const QString& action, const QVariant& argument);

        //! The search text of the current browser changed, see
        //! AbstractChatObjectBrowser::searchTextChanged().
        void searchTextChanged(const QString& text);

    protected:

        void changeEvent(QEvent* event) override;

    private:

        void applyKindTexts();

        ChatObjectBrowserRegistry m_registry;
        AbstractChatObjectBrowser::RequestCb m_requestCb;
        AbstractChatObjectBrowser::ContextMenuBuilder m_menuBuilder;

        QString m_kind;
        QPointer<AbstractChatObjectBrowser> m_browser;

        QBoxLayout* m_headerLayout;
        QBoxLayout* m_bodyLayout;
        PushButton* m_backButton;
        ElidedLabel* m_titleLabel;
        QLabel* m_unknownLabel;
};

} // namespace uise

#endif // UISE_DESKTOP_CHATOBJECTBROWSERPANEL_HPP
