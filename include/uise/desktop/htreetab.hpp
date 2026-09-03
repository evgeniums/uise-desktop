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

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

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

        /**
         * @brief How a successful openPath() affects the tab's navigation history.
         */
        enum class HistoryMode
        {
            /**
             * @brief Record the landing as a new history entry (default) -- an ordinary
             * navigation the user asked for.
             */
            Record,

            /**
             * @brief The navigation is an automatic redirect performed by the node being left,
             * not a step the user took: a node that opens one of its own children as soon as it
             * is filled (e.g. ShareMeController::reload(), AboutController::reload()).
             *
             * The entry the redirect started from is replaced by the landing instead of the
             * landing being stacked on top of it, so the transient intermediate node does not
             * show up in the history and Back skips straight past it. Falls back to Record if
             * the current entry is not an ancestor of the landing (i.e. the history cursor moved
             * elsewhere while the redirect was pending).
             */
            Redirect
        };

        bool openPath(HTreePath path, HistoryMode historyMode=HistoryMode::Record);

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

        //! Override the tab bar's icon/tooltip for this tab, independent of the last node's own
        //! icon/tooltip -- which continue to drive iconUpdated()/tooltipUpdated() as before,
        //! see HTreeTab_p::updateLastNode(). Without this, the tab bar always mirrors the last
        //! node's icon/tooltip, so an externally set value would be overwritten on the very next
        //! node append/truncate. An application uses this to give a tab an identity of its own
        //! (e.g. which account/character a tab belongs to) that survives navigation inside the
        //! tab. Passing a null QIcon()/empty QString() is a valid override -- "no icon/tooltip
        //! for this tab" -- distinct from never calling the setter at all, which is why
        //! hasTabIconOverride()/hasTabTooltipOverride() exist alongside the plain getters.
        void setTabIcon(const QIcon& icon);
        QIcon tabIcon() const;
        bool hasTabIconOverride() const noexcept;

        void setTabTooltip(const QString& tooltip);
        QString tabTooltip() const;
        bool hasTabTooltipOverride() const noexcept;

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

        constexpr static const size_t MaxHistoryDepth=50;

        /**
         * @brief Record the tab's current full path (see path()) as a navigation history entry.
         *
         * Called by openPath() itself on success, and by HTreeBranch::openNextNode() for
         * landings reached via a branch's own next-node slots (e.g. a list item clicked
         * directly), which never go through openPath(). Intermediate prefixes built while
         * openPath() descends into a subtree are never recorded -- only the final, settled path
         * of a completed navigation is. A no-op while goBack()/goForward() is itself replaying a
         * history entry, while the tab has no open node, or when the last node has
         * HTreeNode::isHistoryEnabled()==false. Consecutive identical entries are not duplicated,
         * and opening the entry Back points at walks the cursor back instead of appending -- so
         * bouncing between two nodes toggles within the existing chain rather than growing it.
         *
         * Pass HistoryMode::Redirect when the landing was an automatic redirect out of the node
         * that is currently the newest entry -- see HistoryMode.
         */
        void recordHistory(HistoryMode historyMode=HistoryMode::Record);

        //! Whether goBack() would currently do anything.
        bool canGoBack() const noexcept;
        //! Whether goForward() would currently do anything.
        bool canGoForward() const noexcept;

        /**
         * @brief Navigate to the previous history entry, or -- if the tab's current path is not
         * the history cursor's entry (e.g. after closing a node with its own close button) --
         * back to the cursor entry itself first, without moving the cursor.
         * @return true if navigation happened.
         */
        bool goBack();

        /**
         * @brief Navigate to the next history entry recorded after the current cursor.
         * @return true if navigation happened.
         */
        bool goForward();

        std::vector<HTreePath> history() const;
        //! Index of the current entry in history(), or -1 if history is empty.
        int historyPosition() const noexcept;
        void clearHistory();

    public slots:

        void activate();

    signals:

        void nameUpdated(const QString&);
        void tooltipUpdated(const QString&);
        void iconUpdated(const QIcon&);

        void nodeUpdated(const UISE_DESKTOP_NAMESPACE::HTreePath&);

        void nodesReconfigured();

        //! Emitted whenever history()/historyPosition()/canGoBack()/canGoForward() may have
        //! changed -- after recordHistory(), goBack(), goForward(), or clearHistory().
        void historyChanged();

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

}

#endif // UISE_DESKTOP_HTREE_TAB_HPP
