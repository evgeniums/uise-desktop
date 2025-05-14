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

class HTreeList_p
{
    public:

        QBoxLayout* layout=nullptr;
        QWidget* view=nullptr;

        std::map<std::string,HTreeListItem*> items;

        int maxItemWidth=HTreeList::DefaultMaxItemWidth;

        void updateMaxWidth();
};

//--------------------------------------------------------------------------

void HTreeList_p::updateMaxWidth()
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

HTreeList::HTreeList(HTreeTab* treeTab, QWidget* parent)
    : HTreeBranch(treeTab,parent),
      pimpl(std::make_unique<HTreeList_p>())
{
    pimpl->layout=Layout::horizontal(this);
}

//--------------------------------------------------------------------------

HTreeList::HTreeList(QWidget* parent)
    : HTreeList(nullptr,parent)
{    
}

//--------------------------------------------------------------------------

HTreeList::~HTreeList()
{}

//--------------------------------------------------------------------------

void HTreeList::onItemInsert(HTreeListItem* item)
{
    connect(
        item,
        &HTreeListItem::openRequested,
        this,
        &HTreeList::openNextNode
    );
    connect(
        item,
        &HTreeListItem::openInNewTabRequested,
        this,
        &HTreeList::openNextNodeInNewTab
    );
    connect(
        item,
        &HTreeListItem::openInNewTreeRequested,
        this,
        &HTreeList::openNextNodeInNewTree
    );

    pimpl->items[item->pathElement().id()]=item;

    pimpl->maxItemWidth=std::max(pimpl->maxItemWidth,item->sizeHint().width());

    for (auto& it:pimpl->items)
    {
        it.second->setMaximumWidth(pimpl->maxItemWidth);
    }
}

//--------------------------------------------------------------------------

void HTreeList::onItemRemove(HTreeListItem* item)
{
    disconnect(
        item,
        &HTreeListItem::openRequested,
        this,
        &HTreeList::openNextNode
    );
    disconnect(
        item,
        &HTreeListItem::openInNewTabRequested,
        this,
        &HTreeList::openNextNodeInNewTab
    );
    disconnect(
        item,
        &HTreeListItem::openInNewTreeRequested,
        this,
        &HTreeList::openNextNodeInNewTree
    );

    if (item!=nullptr)
    {
        auto id=item->pathElement().id();
        pimpl->items.erase(id);
    }
}

//--------------------------------------------------------------------------

void HTreeList::setViewWidget(QWidget* widget)
{
    pimpl->view=widget;
    pimpl->layout->addWidget(widget);
}

//--------------------------------------------------------------------------

void HTreeList::setNextNodeId(const std::string& id)
{
    for (auto& it:pimpl->items)
    {
        auto item=it.second;
        item->setSelected(item->pathElement().id()==id);
    }
}

//--------------------------------------------------------------------------

QSize HTreeList::sizeHint() const
{
    auto sz=HTreeBranch::sizeHint();

    auto margins=contentsMargins();
    sz.setWidth(pimpl->maxItemWidth+margins.left()+margins.right());
    return sz;
}

//--------------------------------------------------------------------------

void HTreeList::setDefaultMaxItemWith(int val)
{
    pimpl->maxItemWidth=val;
    pimpl->updateMaxWidth();
}

//--------------------------------------------------------------------------

int HTreeList::defaultMaxItemWidth() const noexcept
{
    return pimpl->maxItemWidth;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
