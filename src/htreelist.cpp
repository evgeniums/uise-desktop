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
#include <uise/desktop/framewithmodalstatus.hpp>

#include <uise/desktop/htreelistwidget.hpp>
#include <uise/desktop/htreelist.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************* HTreeListWidget ***********************************/

class HTreeListWidget_p
{
    public:

        FrameWithModalStatus* statusFrame;
        QBoxLayout* layout=nullptr;

        QWidget* topWidget=nullptr;
        QWidget* bottomWidget=nullptr;

        QWidget* listView=nullptr;

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

HTreeListWidget::HTreeListWidget(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<HTreeListWidget_p>())
{
    auto l=Layout::vertical(this);
    pimpl->statusFrame=makeWidget<FrameWithModalStatus>(this);
    l->addWidget(pimpl->statusFrame);

    pimpl->layout=Layout::vertical(pimpl->statusFrame);
}

//--------------------------------------------------------------------------

HTreeListWidget::~HTreeListWidget()
{
}

//--------------------------------------------------------------------------

void HTreeListWidget::onItemInsert(HTreeListItem* item)
{
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
    if (item!=nullptr)
    {
        auto id=item->pathElement().id();
        pimpl->items.erase(id);
    }
}

//--------------------------------------------------------------------------

void HTreeListWidget::setContentWidgets(QWidget* listView, QWidget* topWidget, QWidget* bottomWidget)
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

    if (pimpl->listView!=nullptr)
    {
        destroyWidget(pimpl->listView);
    }

    pimpl->listView=listView;
    pimpl->layout->addWidget(listView);
    minWidth=std::max(listView->minimumWidth(),minWidth);

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

//--------------------------------------------------------------------------

void HTreeListWidget::setBusyWaiting(bool enable)
{
    if (enable)
    {
        pimpl->statusFrame->popupBusyWaiting();
    }
    else
    {
        pimpl->statusFrame->finish();
    }
}

//--------------------------------------------------------------------------

void HTreeListWidget::showError(const QString& message, const QString& title)
{
    popupStatus(message,StatusDialog::Type::Error,title);
}

//--------------------------------------------------------------------------

void HTreeListWidget::popupStatus(const QString& message, StatusDialog::Type type, const QString& title)
{
    pimpl->statusFrame->popupStatus(message,type,title);
}

/************************* HTreeList ***********************************/

//--------------------------------------------------------------------------

HTreeList::HTreeList(HTreeTab* treeTab, QWidget* parent)
    : HTreeBranch(treeTab,parent)
{
}

//--------------------------------------------------------------------------

QWidget* HTreeList::doCreateContentWidget()
{
    m_widget=new HTreeListWidget(this);
    setupContentWidget();
    auto next=nextNode();
    if (next!=nullptr)
    {
        m_widget->setNextNodeId(next->path().id());
    }
    else
    {
        m_widget->setNextNodeId(std::string());
    }    
    setMinimumWidth(m_widget->minimumWidth());
    m_widget->setDefaultMaxItemWith(m_widget->minimumWidth());
    return m_widget;
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

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
