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

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/framewithrefresh.hpp>

#include <uise/desktop/htreepath.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

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
};

class HTreeNodeLocator;
class HTreeNode_p;

class UISE_DESKTOP_EXPORT HTreeNode : public FrameWithRefresh
{
    Q_OBJECT

    public:

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

        void setParentNode(HTreeNode* node);
        HTreeNode* parentNode() const;

        QString id() const;
        QString name() const;
        QIcon icon() const;
        QString nodeTooltip() const;

        void setNodeName(const QString& val);
        void setNodeTooltip(const QString& val);
        void setNodeIcon(const QIcon& val);

        void setContentWidget(QWidget* widget);
        QWidget* contentWidget() const;

        bool isExpanded() const;

        void setNextNode(HTreeNode* node);
        HTreeNode* nextNode() const;

        void setCollapsible(bool enable);
        bool isCollapsible() const;

        void setClosable(bool enable);
        bool isClosable() const;

        void setTitleBarVisible(bool enable);
        bool isTitleBarVisible() const;

        void setUnique(bool enable);
        bool isUnique() const;

        void setNextNodeLocator(HTreeNodeLocator* locator);
        HTreeNodeLocator* nextNodeLocator() const;

    public slots:

        void closeNode();
        void collapseNode();
        void expandNode();

        void setExpanded(bool enable);

        virtual void setNextNodeId(const std::string& id);

        void activate();

    signals:

        void nameUpdated(const QString&);
        void tooltipUpdated(const QString&);
        void iconUpdated(const QIcon&);

        void toggleExpanded(bool enable);

    protected:

        virtual QWidget* createContentWidget()=0;

    private slots:

        void nextNodeDestroyed(QObject*);
        void otherNodeExpanded(bool);

    private:

        std::unique_ptr<HTreeNode_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_NODE_HPP
