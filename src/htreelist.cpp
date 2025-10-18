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

#include <QPointer>

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

        QBoxLayout* mainLayout;

        QPointer<QWidget> layoutFrame;

        QPointer<FrameWithModalStatus> statusFrame;
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
    if (maxItemWidth==0)
    {
        return;
    }

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
    : WidgetQFrame(parent),
      pimpl(std::make_unique<HTreeListWidget_p>())
{
    pimpl->mainLayout=Layout::vertical(this);
}

//--------------------------------------------------------------------------

HTreeListWidget::~HTreeListWidget()
{
    pimpl->items.clear();
}

//--------------------------------------------------------------------------

void HTreeListWidget::onItemInsert(HTreeListItem* item)
{
    connect(
        item->qobject(),
        &HTreeListItemTQ::selectionChanged,
        this,
        [id=item->uniqueId(),this](bool selected)
        {
            if (selected)
            {
                for (auto& item1 : pimpl->items)
                {
                    if (item1.first!=id)
                    {
                        item1.second->setSelected(false);
                    }
                }
            }
        }
    );

    pimpl->items[item->uniqueId()]=item;

    if (pimpl->maxItemWidth!=0)
    {
        pimpl->maxItemWidth=std::max(pimpl->maxItemWidth,item->sizeHint().width()+ItemExtraWidth);
        for (auto& it:pimpl->items)
        {
            it.second->setMaximumWidth(pimpl->maxItemWidth);
        }
    }
}

//--------------------------------------------------------------------------

void HTreeListWidget::onItemRemove(HTreeListItem* item)
{
    if (item!=nullptr)
    {
        auto id=item->uniqueId();
        pimpl->items.erase(id);
    }
}

//--------------------------------------------------------------------------

void HTreeListWidget::setLayoutFrame(QWidget* frame, QVBoxLayout* layout)
{
    if (pimpl->layoutFrame)
    {
        destroyWidget(pimpl->layoutFrame);
    }

    pimpl->layoutFrame=frame;
    pimpl->mainLayout->addWidget(frame);

    if (layout==nullptr)
    {
        pimpl->layout=Layout::vertical(frame);
    }
    else
    {
        pimpl->layout=layout;
    }
}

//--------------------------------------------------------------------------

void HTreeListWidget::setContentWidgets(QWidget* listView, QWidget* topWidget, QWidget* bottomWidget)
{
    if (!pimpl->layoutFrame)
    {
        pimpl->statusFrame=makeWidget<FrameWithModalStatus>(this);
        setLayoutFrame(pimpl->statusFrame);
    }

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
    pimpl->layout->addWidget(listView,1);
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
        item->setSelected(item->uniqueId()==id);
    }
}

//--------------------------------------------------------------------------

QSize HTreeListWidget::sizeHint() const
{
    auto margins=contentsMargins();
    auto sz=QFrame::sizeHint();

    if (pimpl->maxItemWidth==0)
    {
        auto w=sz.width();
        for (auto& it:pimpl->items)
        {
            auto item=it.second;
            w=std::max(item->sizeHint().width(),w);
        }

        sz.setWidth(w+margins.left()+margins.right());
        return sz;
    }

    auto width=pimpl->maxItemWidth;
    sz.setWidth(width+margins.left()+margins.right());
    return sz;
}

//--------------------------------------------------------------------------

void HTreeListWidget::setDefaultMaxItemWidth(int val)
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
    if (!pimpl->statusFrame)
    {
        return;
    }

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
    if (!pimpl->statusFrame)
    {
        return;
    }

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
        m_widget->setNextNodeId(next->path().uniqueId());
    }
    else
    {
        m_widget->setNextNodeId(std::string());
    }    
    setMinimumWidth(m_widget->minimumWidth());
    if (m_widget->defaultMaxItemWidth()!=0)
    {
        m_widget->setDefaultMaxItemWidth(m_widget->minimumWidth());
    }
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
