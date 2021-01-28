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
                blockUpdate(false),
                singleWidgetHelper({nullptr})
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

            QObject::disconnect(widget,SIGNAL(destroyed(QObject*)),view,SLOT(itemDestroyed(QObject*)));
            QObject::connect(widget,SIGNAL(destroyed(QObject*)),view,SLOT(itemDestroyed(QObject*)));

            auto item=std::make_shared<LinkedListViewItem>(widget);
            item->keepInWidgetProperty();
            return item;
        }

        void takeItem(const std::shared_ptr<LinkedListViewItem>& item)
        {
            if (item)
            {
                LinkedListViewItem::clearWidgetProperty(item->widget());
                item->widget()->setVisible(false);
                item->widget()->setParent(nullptr);
                if (!blockUpdate)
                {
                    if (head.lock().get()==item.get())
                    {
                        head=item->next();;
                    }

                    for (auto nextItem=item->next();nextItem;)
                    {
                        nextItem->decPos();
                        nextItem=nextItem->next();
                    }
                }
            }
        }

        void insertWidget(QWidget *newWidget, QWidget *existingWidget, bool after)
        {
            singleWidgetHelper[0]=newWidget;
            insertWidgets(singleWidgetHelper,existingWidget,after);
        }

        void insertWidgets(const std::vector<QWidget*>& newWidgets, QWidget *existingWidget, bool after)
        {
            // nothing to do on empty list
            if (newWidgets.empty())
            {
                return;
            }

            if (existingWidget)
            {
                // check constraints for existing widget
                Q_ASSERT(existingWidget->parent()==view);
                Q_ASSERT(head.lock());
            }
            else
            {
                // if existingWidget is not set then insert before head
                auto headItem=head.lock();
                if (headItem)
                {
                    existingWidget=headItem->widget();
                    after=false;
                }
            }

            // check item for existing widget
            auto existingItem=LinkedListViewItem::getFromWidgetProperty(existingWidget);
            if (existingWidget)
            {
                Q_ASSERT(existingItem);
            }

            // calculate position of the first new item
            size_t pos=0;
            if (existingItem)
            {
                pos=after?(existingItem->pos()+1):existingItem->pos();
            }
            bool firstItemIsHead=pos==0;

            // construct item list from input widgets
            std::shared_ptr<LinkedListViewItem> firstItem;
            std::shared_ptr<LinkedListViewItem> lastItem;
            for (auto&& newWidget : newWidgets)
            {
                takeItem(LinkedListViewItem::getFromWidgetProperty(newWidget));
                auto newItem=itemForWidget(newWidget);
                newItem->setPrevAuto(lastItem);

                layout->insertWidget(pos,newWidget,0,Qt::AlignLeft|Qt::AlignTop);
                newWidget->setVisible(true);
                newItem->setPos(pos++);

                if (!firstItem)
                {
                    firstItem=newItem;
                }
                lastItem=std::move(newItem);
            }
            if (firstItemIsHead)
            {
                head=firstItem;
            }

            // insert constructed list into existing list
            if (existingItem)
            {
                if (after)
                {
                    lastItem->setNextAuto(existingItem->next());
                    existingItem->setNextAuto(firstItem);
                }
                else
                {
                    firstItem->setPrevAuto(existingItem->prev());
                    lastItem->setNextAuto(existingItem);
                }
            }

            // update positions of items after last inserted item
            pos=lastItem->pos();
            for (auto item=lastItem->next(); item;)
            {
                item->setPos(++pos);
                item=item->next();
            }

            view->resize((view->sizeHint()));
        }

    public:

        LinkedListView* view;
        Qt::Orientation orientation;

        std::weak_ptr<LinkedListViewItem> head;
        QBoxLayout* layout;
        bool blockUpdate;

        std::vector<QWidget*> singleWidgetHelper;
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
void LinkedListView::clear(const DropWidgetHandler &dropWidget)
{
    blockSignals(true);
    pimpl->blockUpdate=true;
    for (auto item=pimpl->head.lock(); item;)
    {
        item->clearWidgetProperty(item->widget());
        dropWidget(item->widget());
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
        clear();

        blockSignals(true);
        pimpl->blockUpdate=true;

        pimpl->orientation=orientation;
        pimpl->setupLayout();

        blockSignals(false);
        pimpl->blockUpdate=false;
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
void LinkedListView::insertWidgetsAfter(const std::vector<QWidget*>& newWidgets, QWidget *existingWidget)
{
    pimpl->insertWidgets(newWidgets,existingWidget,true);
}

//--------------------------------------------------------------------------
void LinkedListView::insertWidgetsBefore(const std::vector<QWidget*>& newWidgets, QWidget *existingWidget)
{
    pimpl->insertWidgets(newWidgets,existingWidget,false);
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
size_t LinkedListView::widgetSeqPos(QObject *widget) const
{
    auto item=LinkedListViewItem::getFromWidgetProperty(widget);
    if (item)
    {
        return item->pos();
    }
    return 0;
}

//--------------------------------------------------------------------------
QWidget* LinkedListView::widgetAtSeqPos(size_t pos) const
{
    auto item=pimpl->head.lock();
    for (size_t i=0;i<pos;i++)
    {
        if (!item)
        {
            return nullptr;
        }
        item=item->next();
    }
    if (!item)
    {
        return nullptr;
    }
    return item->widget();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_EMD
