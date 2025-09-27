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

        void openPath(HTreePath path, int tabIndex=CurrentTabIndex);

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

    signals:

        void newTreeRequested(const UISE_DESKTOP_NAMESPACE::HTreePath& path);

    public slots:

        void setCurrentTab(int tabIndex);
        void setCurrentTab(UISE_DESKTOP_NAMESPACE::HTreeTab* tab);
        void closeTab(int tabIndex);

        void activate();

    private:

        std::unique_ptr<HTree_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HORIZONTAL_TREE_HPP
