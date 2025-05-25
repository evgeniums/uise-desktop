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
#include <uise/desktop/flyweightlistview.hpp>

#include <uise/desktop/htreelistitem.hpp>
#include <uise/desktop/htreebranch.hpp>
#include <uise/desktop/htreenodebuilder.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class HTreeList;
class HTreeList_p;

template <typename ItemT, typename BaseT=QFrame>
class HTreeListView : public WithFlyweightListView<ItemT,BaseT>,
                      public WithRefreshRequested
{
    public:

        using WithFlyweightListView<ItemT,BaseT>::WithFlyweightListView;

        ~HTreeListView()
        {
            if (this->listView()!=nullptr)
            {
                this->listView()->resetCallbacks();
            }
        }

        void setHTreeList(HTreeList* list) noexcept
        {
            m_list=list;
        }

        HTreeList* hTreeList() const noexcept
        {
            return m_list;
        }

    private:

        HTreeList* m_list=nullptr;
};

class HTreeListWidget_p;
class UISE_DESKTOP_EXPORT HTreeListWidget : public QFrame
{
    Q_OBJECT

    public:

        constexpr static const int DefaultMaxItemWidth=170;
        constexpr static const int ItemExtraWidth=30;

        HTreeListWidget(HTreeList* node);

        /**
         * @brief Destructor.
         */
        ~HTreeListWidget();

        HTreeListWidget(const HTreeListWidget&)=delete;
        HTreeListWidget(HTreeListWidget&&)=delete;
        HTreeListWidget& operator=(const HTreeListWidget&)=delete;
        HTreeListWidget& operator=(HTreeListWidget&&)=delete;

        QSize sizeHint() const override;

        void setDefaultMaxItemWith(int val);

        int defaultMaxItemWidth() const noexcept;

        HTreeList* node() const
        {
            return m_node;
        }

        void setViewWidgets(QWidget* widget, QWidget* topWidget=nullptr, QWidget* bottomWidget=nullptr);

    public slots:

        void setNextNodeId(const std::string& id);

    private:

        void onItemInsert(HTreeListItem* item);
        void onItemRemove(HTreeListItem* item);        

        std::unique_ptr<HTreeListWidget_p> pimpl;
        HTreeList* m_node;

        friend class HTreeListViewBuilder;
};

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
        HTreeList(std::shared_ptr<HTreeListViewBuilder> builder, HTreeTab* treeTab, QWidget* parent=nullptr);

        std::shared_ptr<HTreeListViewBuilder> listViewBuilder() const
        {
            return m_builder;
        }

    public slots:

        void setNextNodeId(const std::string& id) override;

    protected:

        QWidget* createContentWidget() override;

    private:

        QWidget* doCreateContentWidget();

        QPointer<HTreeListWidget> m_widget;
        std::shared_ptr<HTreeListViewBuilder> m_builder;
};

struct makeNullWidgetT
{
    QWidget* operator()(QWidget* parent=nullptr) const
    {
        return nullptr;
    }
};
constexpr makeNullWidgetT makeNullWidget{};

class UISE_DESKTOP_EXPORT HTreeListViewBuilder
{
    public:

        HTreeListViewBuilder()=default;

        virtual ~HTreeListViewBuilder();

        HTreeListViewBuilder(const HTreeListViewBuilder&)=default;
        HTreeListViewBuilder(HTreeListViewBuilder&&)=default;
        HTreeListViewBuilder& operator=(const HTreeListViewBuilder&)=default;
        HTreeListViewBuilder& operator=(HTreeListViewBuilder&&)=default;

        virtual void createView(HTreeListWidget* listWidget) const=0;

        template <typename BuilderT>
        void createViewT(HTreeListWidget* listWidget, BuilderT&& builder) const
        {
            auto view=builder();
            setView(view,listWidget);
        }

        template <typename ViewBuilderT, typename TopBuilderT, typename BottomBuilderT>
        void createViewT(HTreeListWidget* listWidget, ViewBuilderT&& viewBuilder, TopBuilderT&& topBuilder, BottomBuilderT&& bottomBuilder) const
        {
            auto view=viewBuilder();
            QWidget* topWidget=topBuilder();
            QWidget* bottomWidget=bottomBuilder();
            setView(view,listWidget,topWidget,bottomWidget);
        }

        template <typename ItemT, typename BaseT>
        void setView(HTreeListView<ItemT,BaseT>* view, HTreeListWidget* listWidget, QWidget* topWidget=nullptr, QWidget* bottomWidget=nullptr) const
        {            
            view->setHTreeList(listWidget->node());

            view->listView()->setInsertItemCb(
                [listWidget](auto item)
                {
                    listWidget->onItemInsert(item);
                }
            );
            view->listView()->setRemoveItemCb(
                [listWidget](auto item)
                {
                    listWidget->onItemRemove(item);
                }
            );
            listWidget->setViewWidgets(view,topWidget,bottomWidget);

            view->setRefreshRequestedCb(
                [listWidget]()
                {
                    listWidget->node()->refresh();
                }
            );

            QObject::connect(
                listWidget->node(),
                &HTreeNode::refreshRequested,
                view,
                [view]()
                {
                    view->reload();
                }
            );
        }
};

template <typename T>
class HTreeListBuilder : public HTreeNodeBuilder
{
public:

    HTreeNode* makeNode(const HTreePathElement& pathElement, HTreeNode* parentNode=nullptr, HTreeTab* treeTab=nullptr) const override
    {
        HTreePath path;
        if (parentNode!=nullptr)
        {
            path=HTreePath{parentNode->path(),pathElement};
        }

        auto node=new HTreeList(std::make_shared<T>(),treeTab);
        node->setNodeTooltip(QString::fromStdString(pathElement.name()));
        return node;
    }
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_LIST_HPP
