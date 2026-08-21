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

/** @file uise/desktop/htreenode.hpp
*
*  Declares horizontal tree node.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_NODE_HPP
#define UISE_DESKTOP_HTREE_NODE_HPP

#include <memory>

#include <QFrame>
#include <QResizeEvent>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/framewithrefresh.hpp>

#include <uise/desktop/htreepath.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class SvgIcon;

class HTreeTab;

class HTreeNode;
class HTreeNodeTitleBar_p;

class UISE_DESKTOP_EXPORT HTreeNodeTitleBar : public QFrame
{
    Q_OBJECT

    public:

        HTreeNodeTitleBar(HTreeNode* node);

        /**
         * @brief Destructor.
         */
        ~HTreeNodeTitleBar();

        HTreeNodeTitleBar(const HTreeNodeTitleBar&)=delete;
        HTreeNodeTitleBar(HTreeNodeTitleBar&&)=delete;
        HTreeNodeTitleBar& operator=(const HTreeNodeTitleBar&)=delete;
        HTreeNodeTitleBar& operator=(HTreeNodeTitleBar&&)=delete;

    signals:

        void refreshRequested();
        void closeRequested();
        void collapseRequested();
        void toParentRequested();
        void exclusiveRequested();

    private:

        std::unique_ptr<HTreeNodeTitleBar_p> pimpl;

        friend class HTreeNode;
};

class HTreeNodePlaceHolder_p;
class UISE_DESKTOP_EXPORT HTreeNodePlaceHolder : public QFrame
{
    Q_OBJECT

    public:

        HTreeNodePlaceHolder(HTreeNode* node);

        /**
             * @brief Destructor.
             */
        ~HTreeNodePlaceHolder();

        HTreeNodePlaceHolder(const HTreeNodePlaceHolder&)=delete;
        HTreeNodePlaceHolder(HTreeNodePlaceHolder&&)=delete;
        HTreeNodePlaceHolder& operator=(const HTreeNodePlaceHolder&)=delete;
        HTreeNodePlaceHolder& operator=(HTreeNodePlaceHolder&&)=delete;

    signals:

        void expandRequested();

    private:

        std::unique_ptr<HTreeNodePlaceHolder_p> pimpl;

        friend class HTreeNode;
        friend class HTreeNode_p;
};

class HTreeNodeLocator;
class HTreeNode_p;

class UISE_DESKTOP_EXPORT HTreeNode : public FrameWithRefresh
{
    Q_OBJECT

    public:

        static QString topIconName(const QString& name)
        {
            return QString("HTreeNodeTop::%1").arg(name);
        }

        static QString bottomIconName(const QString& name)
        {
            return QString("HTreeNodeBottom::%1").arg(name);
        }

        /**
         * @brief Constructor.
         * @param treeTab The tree tab this node belongs to.
         * @param parent Parent widget.
         */
        HTreeNode(HTreeTab* treeTab, QWidget* parent=nullptr);

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        HTreeNode(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~HTreeNode();

        HTreeNode(const HTreeNode&)=delete;
        HTreeNode(HTreeNode&&)=delete;
        HTreeNode& operator=(const HTreeNode&)=delete;
        HTreeNode& operator=(HTreeNode&&)=delete;

        void setTreeTab(HTreeTab* tree);
        HTreeTab* treeTab() const;

        void setPath(HTreePath path);
        const HTreePath& path() const;

        HTreePath& mutablePath();

        void setToParentPath(HTreePath path);
        const HTreePath& toParentPath() const;

        void setToParentAction(std::function<void()>);
        std::function<void()> toParentAction() const;

        void setParentNode(HTreeNode* node);
        HTreeNode* parentNode() const;

        QString id() const;
        QString name() const;
        QIcon icon() const;
        QString nodeTooltip() const;

        //! Leading/title icon shown in the navigation bar item for this node.
        //! Stored on the node itself (unlike titleIconUpdated(), which is fire-and-forget) so
        //! that HTreeTab_p::appendNode() can seed a freshly created navbar item with the
        //! current icon instead of waiting for the node to emit it after the item already
        //! exists with no icon slot.
        std::shared_ptr<SvgIcon> titleIcon() const;

        void setContentWidget(QWidget* widget);
        QWidget* contentWidget() const;

        bool isExpanded() const;

        void setNextNode(HTreeNode* node);
        HTreeNode* nextNode() const;

        void setCollapsible(bool enable);
        bool isCollapsible() const;

        void setClosable(bool enable);
        bool isClosable() const;

        void setCloseEnabled(bool enable);
        bool isCloseEnabled() const;

        void setRefreshable(bool enable);
        bool isRefreshable() const;

        void setTitleBarVisible(bool enable);
        bool isTitleBarVisible() const;

        int titleBarHeight() const;

        void setUnique(bool enable);
        bool isUnique() const;

        /**
         * @brief Enable/disable in-place reconstruction of this node when it is the last node
         * of a tab's path and the tab is asked to open a path whose last element differs from
         * this node's, but whose type matches (see canReconstructFromPath()).
         *
         * When enabled, HTreeTab::openPath() may call reconstructFromPath() on this node instead
         * of destroying it and creating a new node. Default is disabled (the traditional
         * destroy-and-recreate behaviour).
         */
        void setContentReloadable(bool enable);
        bool isContentReloadable() const;

        /**
         * @brief Check whether this node can be reconstructed in place for the given path
         * element instead of being destroyed and replaced by a freshly created node.
         *
         * Default implementation requires isContentReloadable(), !isUnique() (reconstructed
         * nodes are never registered in HTreeNodeLocator's unique-node map, so a unique node
         * changing its id in place would leave that map inconsistent), and that el.type()
         * matches this node's current path type. Override to add further constraints (e.g.
         * refuse reconstruction while some transient state is in progress).
         */
        virtual bool canReconstructFromPath(const HTreePathElement& el) const;

        /**
         * @brief Reconstruct this node in place for a new path, without destroying it.
         *
         * Fills content first if it was never filled, updates the node's path (see setPath()),
         * invokes doReconstructFromPath() for subclasses to switch their content to match the
         * new path, and re-emits nameUpdated()/tooltipUpdated() so dependent UI (tab title,
         * breadcrumb) follows.
         */
        void reconstructFromPath(HTreePath path);

        void setNextNodeLocator(HTreeNodeLocator* locator);
        HTreeNodeLocator* nextNodeLocator() const;

        void prepareForDestroy();

        bool isNodeVisible( ) const;

        void init();

        bool isAtListOneNodeExpanded() const;

        bool isExclusivelyExpandable() const;

        void informForDestroy();

        void setNavbarActivateAction(std::function<void ()>);
        std::function<void ()> navbarActivateAction() const;

        void setNodeControllerUi(QWidget* widget);
        QWidget* nodeControllerUi() const;

        template <typename T>
        T* nodeControllerUiT() const
        {
            return qobject_cast<T*>(nodeControllerUi());
        }

        void setHeaderVisible(bool enable);
        bool isHeaderVisible() const;

        bool isToParentVisible() const;
        QString toParentTooltip() const;

    public slots:

        void setNodeName(const QString& val);
        void setNodeTooltip(const QString& val);
        void setNodeIcon(const QIcon& val);

        //! Set the title icon and emit titleIconUpdated() -- storing and signalling can never
        //! diverge since they happen in the same call.
        void setTitleIcon(std::shared_ptr<UISE_DESKTOP_NAMESPACE::SvgIcon> icon);

        void closeNode();
        void collapseNode();
        void expandNode();

        void setExpanded(bool enable);

        virtual void setNextNodeId(const std::string& id);

        void activate();

        void expandParentNode();

        void expandExclusive(int depth=0);

        void activateToParent();

        /**
         * @brief Re-run this node's action because it was requested again while already the
         * currently open next node (e.g. the same list item or navbar button was clicked a
         * second time).
         *
         * For an ordinary content node this is a no-op by default -- the node is already open
         * and showing its content, so there is nothing to redo. Action-style nodes (a node
         * whose refresh() performs a one-shot action, e.g. a confirmation prompt, rather than
         * displaying persistent content) override doReopen() to repeat that action, since
         * HTreeBranch::loadNextNode() and HTreeTab::openPath() both short-circuit before
         * reaching refresh()/doRefresh() when the requested node is already open.
         */
        void reopen();

    signals:

        void nameUpdated(const QString&);
        void tooltipUpdated(const QString&);
        void iconUpdated(const QIcon&);

        void titleIconUpdated(std::shared_ptr<UISE_DESKTOP_NAMESPACE::SvgIcon> icon);
        void trailingIconUpdated(std::shared_ptr<UISE_DESKTOP_NAMESPACE::SvgIcon> icon);

        void toggleExpanded(bool enable);

        void initRequested();

        void reopenRequested();

        void aboutToDestroy(QObject* obj);

        void closeHovered(UISE_DESKTOP_NAMESPACE::HTreeNode* node, bool enable);

    protected:

        virtual QWidget* createContentWidget()=0;

        virtual void doInit()
        {}

        /**
         * @brief Reconstruct this node's content for a new path, invoked by
         * reconstructFromPath(). The base implementation does nothing; content-owning
         * subclasses (e.g. a node hosting a stack of pre-built pages) override this to switch
         * their displayed content to match the new path element. setPath() has already been
         * called with the new path when this is invoked.
         */
        virtual void doReconstructFromPath(const HTreePath&)
        {}

        /**
         * @brief Reimplement to redo this node's action when reopen() is invoked. See the
         * reopen() doc comment. Default implementation does nothing.
         */
        virtual void doReopen()
        {}

        void fillContent();

        virtual void onNodeExpanded()
        {}

    private slots:

        void nextNodeDestroyed(QObject*);
        void otherNodeExpanded(bool);

        void onPlaceHolderExpandRequest();
        void onToParentRequested();

    private:

        void updateExclusivelyExpandable();

        void setParentNodeTitle(const QString& title);

        bool updateCollapsePlaceholder();
        void updateCollapsePlaceholderTooltip();

        std::unique_ptr<HTreeNode_p> pimpl;

        friend class HTreeBranch;
        friend class HTreeTab;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_NODE_HPP
