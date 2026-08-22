/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/htree.hpp
*
*  Declares horizontal tree control with loadable nodes, navigation history and bookmarks of tree paths.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HORIZONTAL_TREE_HPP
#define UISE_DESKTOP_HORIZONTAL_TREE_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/framewithrefresh.hpp>

#include <uise/desktop/htreepath.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class Style;

class HTreeNode;
class HTreeTab;
class HTreeNodeLocator;
class HTreeSideBar;
class HTreeTabBar;
class HTreeTabBarItem;
class HTreeTabBarBuilder;
class HTree_p;

class UISE_DESKTOP_EXPORT HTree : public QFrame
{
    Q_OBJECT

    public:

        constexpr static const int CurrentTabIndex=-1;
        constexpr static const int NewTabIndex=-2;

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        HTree(HTreeNodeLocator* locator=nullptr, QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~HTree();

        HTree(const HTree&)=delete;
        HTree(HTree&&)=delete;
        HTree& operator=(const HTree&)=delete;
        HTree& operator=(HTree&&)=delete;

        void setNodeLocator(HTreeNodeLocator* locator) noexcept;
        HTreeNodeLocator* nodeLocator() const noexcept;

        //! Returns the tab \p path was opened in -- either an existing tab reused/switched to,
        //! or a freshly created one -- or nullptr if no tab could be resolved. Source-compatible
        //! with the previous void return: every existing caller discards the result.
        HTreeTab* openPath(HTreePath path, int tabIndex=CurrentTabIndex);

        void loadPaths(const std::vector<HTreePath>& paths);
        std::vector<HTreePath> paths() const;

        int tabCount() const;
        int currentTabIndex() const;

        HTreeTab* tab(int tabIndex=0) const;
        HTreeTab* currentTab() const;

        void closeAllTabs();

        HTreeNode* showNode(const HTreePath& path);
        HTreeNode* node(const HTreePath& path) const;

        HTreeSideBar* sidebar() const;

        void setSingleCollapsePlaceholder(bool enable) noexcept;
        bool isSingleCollapsePlaceholder() const noexcept;

        void setCollapsePlaceholderHidden(bool enable) noexcept;
        bool isCollapsePlacehodlerHidden() const noexcept;

        void setExlusivelyExpandableNode(bool enable) noexcept;
        bool isExlusivelyExpandableNode() const noexcept;

        void setExpandableLastDepthOnNodeOpen(int) noexcept;
        int expandableLastDepthOnNodeOpen() const noexcept;

        void setInternalNodeExpandable(bool enable) noexcept;
        bool isInternalNodeExpandable() const noexcept;

        void setInternalNodeClosable(bool enable) noexcept;
        bool isInternalNodeClosable() const noexcept;

        void setNodeHeaderVisible(bool enable);
        bool isNodeHeaderVisible() const noexcept;

        void setNavbarSingleVisibleMode(bool enable);
        bool isNavbarSingleVisibleMode() const noexcept;

        //! Install a builder that gives every tab its own composite tab-bar widget (see
        //! HTreeTabBar/HTreeTabBarItem) in place of the plain QTabBar label/icon. Existing
        //! tabs are fitted with an item immediately; every later tab gets one as it is
        //! created. Passing nullptr reverts to the plain QTabBar -- existing items are
        //! destroyed. With no builder ever set, HTree behaves exactly as it did before this
        //! API existed.
        void setTabBarBuilder(std::shared_ptr<UISE_DESKTOP_NAMESPACE::HTreeTabBarBuilder> builder);
        std::shared_ptr<UISE_DESKTOP_NAMESPACE::HTreeTabBarBuilder> tabBarBuilder() const;

        //! The custom tab bar, or nullptr if no builder has been set.
        UISE_DESKTOP_NAMESPACE::HTreeTabBar* htreeTabBar() const;

        //! The tab-bar item for \p tab, or nullptr if no builder has been set or \p tab is
        //! not one of this tree's tabs.
        UISE_DESKTOP_NAMESPACE::HTreeTabBarItem* tabBarItem(UISE_DESKTOP_NAMESPACE::HTreeTab* tab) const;

        //! Whether the tab bar (native or custom) hides itself while only one tab is open --
        //! on by default, matching QTabWidget::tabBarAutoHide()'s own default. Exposed here
        //! (rather than reaching for the QTabWidget directly, which HTree keeps private) so a
        //! custom-tab-bar application can flip it without another uise-desktop change if a
        //! bar control (e.g. a pin toggle) needs to stay reachable with a single tab open.
        void setTabBarAutoHide(bool enable);
        bool isTabBarAutoHide() const noexcept;

    signals:

        void newTreeRequested(const UISE_DESKTOP_NAMESPACE::HTreePath& path);

        //! Emitted whenever the current tab changes -- tab creation, tab close, openPath()
        //! switching tabs, and the user clicking the tab bar all go through the same internal
        //! QTabWidget::currentChanged() this forwards. \p tab is nullptr once the last tab is
        //! closed.
        void currentTabChanged(UISE_DESKTOP_NAMESPACE::HTreeTab* tab);

    public slots:

        void setCurrentTab(int tabIndex);
        void setCurrentTab(UISE_DESKTOP_NAMESPACE::HTreeTab* tab);
        void closeTab(int tabIndex);

        //! Resolves \p tab's current index itself (see HTree_p::addTab()'s own doc comment on
        //! why an index must never be cached across a tab close) and closes it. A no-op if
        //! \p tab is null or not one of this tree's tabs -- e.g. already closed.
        void closeTab(UISE_DESKTOP_NAMESPACE::HTreeTab* tab);

        void activate();

    private:

        std::unique_ptr<HTree_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HORIZONTAL_TREE_HPP
