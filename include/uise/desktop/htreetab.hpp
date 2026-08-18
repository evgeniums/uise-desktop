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

/** @file uise/desktop/htreetab.hpp
*
*  Declares horizontal tree tab widget.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_TAB_HPP
#define UISE_DESKTOP_HTREE_TAB_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/framewithrefresh.hpp>

#include <uise/desktop/htreepath.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class NavigationBar;

class HTree;
class HTreeNode;

class HTreeTab_p;

class UISE_DESKTOP_EXPORT HTreeTab : public QFrame
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        HTreeTab(HTree* tree=nullptr, QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~HTreeTab();

        HTreeTab(const HTreeTab&)=delete;
        HTreeTab(HTreeTab&&)=delete;
        HTreeTab& operator=(const HTreeTab&)=delete;
        HTreeTab& operator=(HTreeTab&&)=delete;

        bool openPath(HTreePath path);

        /**
         * @brief Reconstruct \p node in place for \p path instead of destroying it and
         * creating a fresh node -- see HTreeNode::reconstructFromPath(). \p node must already
         * be one of this tab's currently open nodes (as returned by nodes()); node type/
         * contentReloadable/isUnique() eligibility is the caller's responsibility (see
         * HTreeNode::canReconstructFromPath()) -- this method does not check it again.
         *
         * Any node open deeper than \p node in this tab is closed first, exactly as it would
         * be if \p node itself were being destroyed and replaced. Used both by openPath()'s own
         * reconstruction shortcut and by HTreeBranch::loadNextNode(), so that a node reached
         * via a branch's own openNextNode()/openNextNodeInNewTab() slots (e.g. a list item
         * clicked directly, without going through HTree::openPath()) gets the same in-place
         * reconstruction opportunity as a path opened through the tab.
         *
         * @return false if \p node is not currently open in this tab.
         */
        bool reconstructNode(HTreeNode* node, HTreePath path);

        HTreeNode* node() const;
        HTreeNode* node(const HTreePath& path, bool exact=true) const;

        HTreePath path() const;

        void appendNode(HTreeNode* node);
        void closeNode(HTreeNode* node);

        void setTree(HTree* tree);
        HTree* tree() const;

        NavigationBar* navbar() const;

        void truncate(int index);

        void scrollToNode(HTreeNode* node);

        bool isSingleCollapsePlaceholder() const noexcept;
        bool isCollapsePlaceholderHidden() const noexcept;

        std::vector<HTreeNode*> nodes() const;

    public slots:

        void activate();

    signals:

        void nameUpdated(const QString&);
        void tooltipUpdated(const QString&);
        void iconUpdated(const QIcon&);

        void nodeUpdated(const UISE_DESKTOP_NAMESPACE::HTreePath&);

        void nodesReconfigured();

    private slots:

        void nodeCloseHovered(UISE_DESKTOP_NAMESPACE::HTreeNode*, bool);

    private:

        void adjustWidthsAndPositions();
        void emitNodesReconfigured();

        friend class HTree;
        friend class HTreeNode;
        friend class HTreeTab_p;

        std::unique_ptr<HTreeTab_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_TAB_HPP
