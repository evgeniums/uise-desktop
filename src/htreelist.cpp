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
};

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
}

//--------------------------------------------------------------------------

void HTreeList::setViewWidget(QWidget* widget)
{
    pimpl->view=widget;
    pimpl->layout->addWidget(widget);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
