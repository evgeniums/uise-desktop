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

/** @file uise/desktop/htreelist.hpp
*
*  Declares branch node of list type for horizontal tree.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_LIST_HPP
#define UISE_DESKTOP_HTREE_LIST_HPP

#include <memory>

#include <QFrame>
#include <QPointer>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/makenullwidget.hpp>
#include <uise/desktop/widget.hpp>
#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/statusdialog.hpp>

#include <uise/desktop/htreebranch.hpp>
#include <uise/desktop/htreenodebuilder.hpp>

#include <uise/desktop/htreelistitem.hpp>
#include <uise/desktop/htreelistwidget.hpp>
#include <uise/desktop/htreelistflyweightview.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class HTreeList_p;
class HTreeListViewBuilder;

class UISE_DESKTOP_EXPORT HTreeList : public HTreeBranch
{
    Q_OBJECT

    public:

        constexpr static const int DefaultMaxItemWidth=170;
        constexpr static const int ItemExtraWidth=30;

        /**
         * @brief Constructor.
         * @param tree The tree this node belongs to.
         * @param parent Parent widget.
         */
        HTreeList(HTreeTab* treeTab, QWidget* parent=nullptr);

        HTreeListWidget* listContentWidget() const
        {
            return m_widget;
        }

    public slots:

        void setNextNodeId(const std::string& id) override;

    protected:

        QWidget* createContentWidget() override;

        virtual void setupContentWidget() =0;

    private:

        QWidget* doCreateContentWidget();

        QPointer<HTreeListWidget> m_widget;
};

class HTreeListUiHelper
{
    public:

        template <typename ListViewBuilderT>
        static auto createListView(HTreeList* node, HTreeListWidget* listWidget, ListViewBuilderT&& listViewBuilder)
        {
            auto listView=listViewBuilder();
            setupView(node,listWidget,listView);
            return listView;
        }

        template <typename ListViewBuilderT, typename TopBuilderT, typename BottomBuilderT>
        static auto createUi(HTreeList* node, HTreeListWidget* listWidget, ListViewBuilderT&& listViewBuilder, TopBuilderT&& topBuilder, BottomBuilderT&& bottomBuilder)
        {
            auto listView=listViewBuilder();
            auto topWidget=topBuilder();
            auto bottomWidget=bottomBuilder();
            setupView(node,listWidget,listView,topWidget,bottomWidget);
            return std::make_tuple();
        }

        // template <typename ItemT, typename BaseT>
        // static void setupView(HTreeList* node,  HTreeListWidget* listWidget, HTreeListFlyweightView<ItemT,BaseT>* view, QWidget* topWidget=nullptr, QWidget* bottomWidget=nullptr)

        template <typename ListViewT>
        static void setupView(HTreeList* node,  HTreeListWidget* listWidget, ListViewT* listView, QWidget* topWidget=nullptr, QWidget* bottomWidget=nullptr)
        {
            listView->setListNode(node);

            listView->listView()->setInsertItemCb(
                [listWidget,node](auto item)
                {
                    QObject::connect(
                        item,
                        &HTreeListItem::openRequested,
                        node,
                        &HTreeBranch::openNextNode
                    );
                    QObject::connect(
                        item,
                        &HTreeListItem::openInNewTabRequested,
                        node,
                        &HTreeBranch::openNextNodeInNewTab
                    );
                    QObject::connect(
                        item,
                        &HTreeListItem::openInNewTreeRequested,
                        node,
                        &HTreeBranch::openNextNodeInNewTree
                    );

                    listWidget->onItemInsert(item);
                }
            );

            listView->listView()->setRemoveItemCb(
                [listWidget,node](auto item)
                {
                    QObject::disconnect(
                        item,
                        &HTreeListItem::openRequested,
                        node,
                        &HTreeBranch::openNextNode
                        );
                    QObject::disconnect(
                        item,
                        &HTreeListItem::openInNewTabRequested,
                        node,
                        &HTreeBranch::openNextNodeInNewTab
                        );
                    QObject::disconnect(
                        item,
                        &HTreeListItem::openInNewTreeRequested,
                        node,
                        &HTreeBranch::openNextNodeInNewTree
                    );
                    listWidget->onItemRemove(item);
                }
            );
            listWidget->setContentWidgets(listView,topWidget,bottomWidget);

            listView->setRefreshRequestedCb(
                [node]()
                {
                    node->refresh();
                }
            );

            QObject::connect(
                node,
                &HTreeNode::refreshRequested,
                listView,
                [listView]()
                {
                    listView->reload();
                }
            );
        }
};

#if 0
class HTreeListBuilderPlaceholder
{
    public:

        template <typename T>
        void initListNode(T* node) const
        {
            std::ignore=node;
        }
};

template <typename ListViewBuilderT, typename ListNodeT=HTreeList, typename BaseBuilderT=HTreeListBuilderPlaceholder>
class HTreeListBuilder : public HTreeNodeBuilder,
                         public BaseBuilderT
{
    public:

        using BaseBuilderT::BaseBuilderT;

        HTreeNode* makeNode(const HTreePathElement& pathElement, HTreeNode* parentNode=nullptr, HTreeTab* treeTab=nullptr) const override
        {
            HTreePath path;
            if (parentNode!=nullptr)
            {
                path=HTreePath{parentNode->path(),pathElement};
            }

            auto node=new ListNodeT(treeTab,parentNode);
            this->initListNode(node);

            node->setNodeTooltip(QString::fromStdString(pathElement.name()));
            return node;
        }
};
#endif

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_LIST_HPP
