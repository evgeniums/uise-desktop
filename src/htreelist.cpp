/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/htreelist.cpp
*
*  Defines HTreeList.
*
*/

/****************************************************************************/

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>

#include <uise/desktop/htreelist.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************* HTreeListWidget ***********************************/

class HTreeListWidget_p
{
    public:

        QBoxLayout* layout=nullptr;

        QWidget* topWidget=nullptr;
        QWidget* bottomWidget=nullptr;

        QWidget* view=nullptr;

        std::map<std::string,HTreeListItem*> items;

        int maxItemWidth=HTreeList::DefaultMaxItemWidth;

        void updateMaxWidth();
};

//--------------------------------------------------------------------------

void HTreeListWidget_p::updateMaxWidth()
{
    for (auto& it:items)
    {
        maxItemWidth=std::max(maxItemWidth,it.second->sizeHint().width());
    }

    for (auto& it:items)
    {
        it.second->setMaximumWidth(maxItemWidth);
    }
}

//--------------------------------------------------------------------------

HTreeListWidget::HTreeListWidget(HTreeList* node)
    : QFrame(node),
      pimpl(std::make_unique<HTreeListWidget_p>()),
      m_node(node)
{
    pimpl->layout=Layout::vertical(this);
}

//--------------------------------------------------------------------------

HTreeListWidget::~HTreeListWidget()
{}

//--------------------------------------------------------------------------

void HTreeListWidget::onItemInsert(HTreeListItem* item)
{
    connect(
        item,
        &HTreeListItem::openRequested,
        m_node,
        &HTreeList::openNextNode
    );
    connect(
        item,
        &HTreeListItem::openInNewTabRequested,
        m_node,
        &HTreeList::openNextNodeInNewTab
    );
    connect(
        item,
        &HTreeListItem::openInNewTreeRequested,
        m_node,
        &HTreeList::openNextNodeInNewTree
    );

    for (auto& otherItem: pimpl->items)
    {
        connect(
            item,
            &HTreeListItem::selectionChanged,
            otherItem.second,
            [other=otherItem.second](bool selected)
            {
                if (selected)
                {
                    other->setSelected(false);
                }
            }
        );
        connect(
            otherItem.second,
            &HTreeListItem::selectionChanged,
            item,
            [item](bool selected)
            {
                if (selected)
                {
                    item->setSelected(false);
                }
            }
        );
    }

    pimpl->items[item->pathElement().id()]=item;

    pimpl->maxItemWidth=std::max(pimpl->maxItemWidth,item->sizeHint().width()+ItemExtraWidth);

    for (auto& it:pimpl->items)
    {
        it.second->setMaximumWidth(pimpl->maxItemWidth);
    }
}

//--------------------------------------------------------------------------

void HTreeListWidget::onItemRemove(HTreeListItem* item)
{
    disconnect(
        item,
        &HTreeListItem::openRequested,
        m_node,
        &HTreeList::openNextNode
    );
    disconnect(
        item,
        &HTreeListItem::openInNewTabRequested,
        m_node,
        &HTreeList::openNextNodeInNewTab
    );
    disconnect(
        item,
        &HTreeListItem::openInNewTreeRequested,
        m_node,
        &HTreeList::openNextNodeInNewTree
    );

    if (item!=nullptr)
    {
        auto id=item->pathElement().id();
        pimpl->items.erase(id);
    }
}

//--------------------------------------------------------------------------

void HTreeListWidget::setViewWidgets(QWidget* widget, QWidget* topWidget, QWidget* bottomWidget)
{
    int minWidth=0;

    if (topWidget!=nullptr)
    {
        if (pimpl->topWidget!=nullptr)
        {
            destroyWidget(pimpl->topWidget);
        }

        pimpl->topWidget=topWidget;
        pimpl->layout->addWidget(topWidget);
        minWidth=topWidget->minimumWidth();
    }

    if (pimpl->view!=nullptr)
    {
        destroyWidget(pimpl->view);
    }

    pimpl->view=widget;
    pimpl->layout->addWidget(widget);
    minWidth=std::max(widget->minimumWidth(),minWidth);

    if (bottomWidget!=nullptr)
    {
        if (pimpl->bottomWidget!=nullptr)
        {
            destroyWidget(pimpl->bottomWidget);
        }

        pimpl->bottomWidget=bottomWidget;
        pimpl->layout->addWidget(bottomWidget);
        minWidth=std::max(bottomWidget->minimumWidth(),minWidth);
    }

    setMinimumWidth(minWidth);
}

//--------------------------------------------------------------------------

void HTreeListWidget::setNextNodeId(const std::string& id)
{
    for (auto& it:pimpl->items)
    {
        auto item=it.second;
        item->setSelected(item->pathElement().id()==id);
    }
}

//--------------------------------------------------------------------------

QSize HTreeListWidget::sizeHint() const
{
    auto sz=QFrame::sizeHint();

    auto margins=contentsMargins();
    sz.setWidth(pimpl->maxItemWidth+margins.left()+margins.right());
    return sz;
}

//--------------------------------------------------------------------------

void HTreeListWidget::setDefaultMaxItemWith(int val)
{
    pimpl->maxItemWidth=val;
    pimpl->updateMaxWidth();
}

//--------------------------------------------------------------------------

int HTreeListWidget::defaultMaxItemWidth() const noexcept
{
    return pimpl->maxItemWidth;
}

/************************* HTreeList ***********************************/

//--------------------------------------------------------------------------

HTreeList::HTreeList(std::shared_ptr<HTreeListViewBuilder> builder, HTreeTab* treeTab, QWidget* parent)
    : HTreeBranch(treeTab,parent),
      m_builder(std::move(builder))
{
    setContentWidget(doCreateContentWidget());
}

//--------------------------------------------------------------------------

QWidget* HTreeList::doCreateContentWidget()
{
    auto w=new HTreeListWidget(this);
    m_builder->createView(w);
    auto next=nextNode();
    if (next!=nullptr)
    {
        w->setNextNodeId(next->path().id());
    }
    else
    {
        w->setNextNodeId(std::string());
    }
    m_widget=w;
    setMinimumWidth(w->minimumWidth());
    w->setDefaultMaxItemWith(w->minimumWidth());
    return w;
}

//--------------------------------------------------------------------------

QWidget* HTreeList::createContentWidget()
{
    return doCreateContentWidget();
}

//--------------------------------------------------------------------------

void HTreeList::setNextNodeId(const std::string& id)
{
    if (m_widget)
    {
        m_widget->setNextNodeId(id);
    }
}

/************************* HTreeListViewBuilder ****************************/

//--------------------------------------------------------------------------

HTreeListViewBuilder::~HTreeListViewBuilder()
{}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
