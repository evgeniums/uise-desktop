/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** \file uise/desktop/src/linkedlistview.cpp
*
*  Contains implementation of LinkedListView.
*
*/

/****************************************************************************/

#include <QBoxLayout>

#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/layout.hpp>

#include <uise/desktop/linkedlistviewitem.hpp>
#include <uise/desktop/linkedlistview.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace detail {

class LinkedListView_p
{
    public:

        LinkedListView_p(
                LinkedListView* view,
                Qt::Orientation orientation
            ) : view(view),
                orientation(orientation),
                layout(nullptr),
                blockUpdate(false)
        {
        }

        void setupLayout()
        {
            layout=Layout::box(view,orientation);
            if (orientation==Qt::Horizontal)
            {
                view->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Expanding);
            }
            else
            {
                view->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
            }
        }

        std::shared_ptr<LinkedListViewItem> itemForWidget(QWidget *widget)
        {
            if (widget->parent()!=nullptr && widget->parent()!=view)
            {
                auto otherView=qobject_cast<LinkedListView*>(widget->parent());
                if (otherView)
                {
                    otherView->takeWidget(widget);
                }
            }

            QObject::disconnect(widget,SIGNAL(destroyed(QObject*)),view,SLOT(itemDestroyed(Object*)));
            QObject::connect(widget,SIGNAL(destroyed(QObject*)),view,SLOT(itemDestroyed(Object*)));

            auto item=std::make_shared<LinkedListViewItem>(widget);
            item->keepInWidgetProperty();
            return item;
        }

        void takeItem(const std::shared_ptr<LinkedListViewItem>& item)
        {
            if (item)
            {
                LinkedListViewItem::clearWidgetProperty(item->widget());
                if (!blockUpdate)
                {
                    while (auto nextItem=item->next())
                    {
                        nextItem->decPos();
                    }
                }
            }
        }

        void insertWidget(QWidget *newWidget, QWidget *existingWidget, bool after)
        {
            if (existingWidget)
            {
                Q_ASSERT(existingWidget->parent()==view);
            }

            takeItem(LinkedListViewItem::getFromWidgetProperty(newWidget));

            auto newItem=itemForWidget(newWidget);
            auto existingItem=LinkedListViewItem::getFromWidgetProperty(existingWidget);

            size_t pos=0;
            if (!head.lock())
            {
                Q_ASSERT(!existingItem);

                head=newItem;
            }
            else
            {
                Q_ASSERT(existingItem);

                if (after)
                {
                    pos=existingItem->pos()+1;
                    newItem->setPos(pos);
                    newItem->setPrev(existingItem);

                    auto nextItem=existingItem->next();
                    newItem->setNext(nextItem);
                    existingItem->setNext(newItem);

                    while (nextItem)
                    {
                        nextItem->incPos();
                        nextItem=nextItem->next();
                    }
                }
                else
                {
                    pos=existingItem->pos();
                    newItem->setPos(pos);
                    newItem->setNext(existingItem);

                    auto prevItem=existingItem->prev();
                    if (prevItem)
                    {
                        prevItem->setNext(newItem);
                    }
                    newItem->setPrev(prevItem);

                    while (auto nextItem=existingItem)
                    {
                        nextItem->incPos();
                        nextItem=nextItem->next();
                    }
                }
            }

            layout->insertWidget(pos,newWidget,0,Qt::AlignLeft|Qt::AlignTop);
        }

    public:

        LinkedListView* view;
        Qt::Orientation orientation;

        std::weak_ptr<LinkedListViewItem> head;
        QBoxLayout* layout;
        bool blockUpdate;
};

}

//--------------------------------------------------------------------------
LinkedListView::LinkedListView(
        QWidget *parent,
        Qt::Orientation orientation
    ) : QFrame(parent),
        pimpl(std::make_unique<detail::LinkedListView_p>(this,orientation))
{
    pimpl->setupLayout();
}

//--------------------------------------------------------------------------
LinkedListView::~LinkedListView()
{}

//--------------------------------------------------------------------------
void LinkedListView::clear()
{
    blockSignals(true);
    pimpl->blockUpdate=true;
    while (auto item=pimpl->head.lock())
    {
        destroyWidget(item->widget());
        item=item->next();
    }
    pimpl->head.reset();
    blockSignals(false);
    pimpl->blockUpdate=false;
    emit resized();
}

//--------------------------------------------------------------------------
Qt::Orientation LinkedListView::orientation() const noexcept
{
    return pimpl->orientation;
}

//--------------------------------------------------------------------------
void LinkedListView::setOrientation(Qt::Orientation orientation)
{
    if (orientation!=pimpl->orientation)
    {
        blockSignals(true);
        pimpl->blockUpdate=true;

        //! @todo temporary take widgets from layout

        pimpl->orientation=orientation;
        pimpl->setupLayout();

        //! @todo add widgets back to new layout

        blockSignals(false);
        pimpl->blockUpdate=false;
        emit resized();
    }
}

//--------------------------------------------------------------------------
void LinkedListView::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    emit resized();
}

//--------------------------------------------------------------------------
void LinkedListView::insertWidgetAfter(QWidget *newWidget, QWidget *existingWidget)
{
    pimpl->insertWidget(newWidget,existingWidget,true);
}

//--------------------------------------------------------------------------
void LinkedListView::insertWidgetBefore(QWidget *newWidget, QWidget *existingWidget)
{
    pimpl->insertWidget(newWidget,existingWidget,false);
}

//--------------------------------------------------------------------------
void LinkedListView::itemDestroyed(QObject *widget)
{
    // ignore children of other objects, though it must not happen at all
    if (widget->parent()!=this)
    {
        return;
    }

    takeWidget(widget);
}

//--------------------------------------------------------------------------
void LinkedListView::takeWidget(QObject *widget)
{
    auto item=LinkedListViewItem::getFromWidgetProperty(widget);
    pimpl->takeItem(item);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_EMD
