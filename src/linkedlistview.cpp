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

/** @file uise/desktop/src/linkedlistview.cpp
*
*  Contains implementation of LinkedListView.
*
*/

/****************************************************************************/

#include <QDebug>
#include <QEvent>
#include <QCoreApplication>
#include <QStyle>

#include <uise/desktop/utils/orientationinvariant.hpp>

#include <uise/desktop/linkedlistviewitem.hpp>
#include <uise/desktop/linkedlistview.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

/**
 * @brief Check if a widget contributes nothing to the layout.
 *
 * Mirrors QWidgetItem::isEmpty(): a hidden widget is skipped unless its size
 * policy asks to retain its layout size while hidden.
 */
bool isEmptyItem(const QWidget* w)
{
    return w->isHidden() && !w->sizePolicy().retainSizeWhenHidden();
}

/**
 * @brief Effective size hint of a widget as used by a layout.
 *
 * Mirrors QWidgetItem::sizeHint(): a layout never uses the raw sizeHint(), it
 * clamps it into the widget's actual [minimum,maximum] size range.
 */
QSize itemSizeHint(const QWidget* w)
{
    return w->sizeHint()
            .expandedTo(w->minimumSizeHint())
            .boundedTo(w->maximumSize())
            .expandedTo(w->minimumSize());
}

/**
 * @brief Effective minimum size of a widget as used by a layout.
 *
 * Mirrors Qt's private qSmartMinSize().
 */
QSize itemMinSize(const QWidget* w)
{
    QSize s(0,0);
    auto policy=w->sizePolicy();
    auto hint=w->sizeHint();
    auto minHint=w->minimumSizeHint();

    if (policy.horizontalPolicy()!=QSizePolicy::Ignored)
    {
        s.setWidth((policy.horizontalPolicy() & QSizePolicy::ShrinkFlag) ?
                       minHint.width() : qMax(hint.width(),minHint.width()));
    }
    if (policy.verticalPolicy()!=QSizePolicy::Ignored)
    {
        s.setHeight((policy.verticalPolicy() & QSizePolicy::ShrinkFlag) ?
                        minHint.height() : qMax(hint.height(),minHint.height()));
    }

    s=s.boundedTo(w->maximumSize());

    auto minSize=w->minimumSize();
    if (minSize.width()>0)
    {
        s.setWidth(minSize.width());
    }
    if (minSize.height()>0)
    {
        s.setHeight(minSize.height());
    }

    return s.expandedTo(QSize(0,0));
}

/**
 * @brief Effective maximum size of a widget along a single axis, as used by a layout.
 *
 * Mirrors Qt's private qSmartMaxSize(), evaluated for one axis at a time: an
 * unset (QWIDGETSIZE_MAX) maximum is clamped down to the widget's own hint
 * unless its size policy carries QSizePolicy::GrowFlag. If the axis is
 * explicitly aligned rather than filled, the cap is lifted entirely.
 */
int itemMaxCross(const QWidget* w, bool crossIsHorizontal, bool crossAligned)
{
    if (crossAligned)
    {
        return QWIDGETSIZE_MAX;
    }

    int m=crossIsHorizontal?w->maximumWidth():w->maximumHeight();
    if (m==QWIDGETSIZE_MAX)
    {
        auto policy=crossIsHorizontal?w->sizePolicy().horizontalPolicy():w->sizePolicy().verticalPolicy();
        if (!(policy & QSizePolicy::GrowFlag))
        {
            auto hint=w->sizeHint().expandedTo(w->minimumSizeHint()).expandedTo(w->minimumSize());
            m=crossIsHorizontal?hint.width():hint.height();
        }
    }
    return m;
}

} // anonymous namespace

namespace detail {

class LinkedListView_p : public OrientationInvariant
{
    public:

        LinkedListView_p(
                LinkedListView* view,
                Qt::Orientation orientation
            ) : view(view),
                orientation(orientation),
                blockUpdate(false),
                singleWidgetHelper({nullptr}),
                alignment(Qt::Alignment()),
                inRelayout(false)
        {
        }

        bool isHorizontal() const noexcept override
        {
            return orientation==Qt::Horizontal;
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

        void takeItem(const std::shared_ptr<LinkedListViewItem>& item, bool destroyed=false)
        {
            if (item)
            {
                LinkedListViewItem::clearWidgetProperty(item->widget());
                if (!destroyed)
                {
                    item->widget()->setVisible(false);
                    item->widget()->setParent(nullptr);
                }
                if (!blockUpdate)
                {
                    if (head.lock().get()==item.get())
                    {
                        head=item->next();;
                    }

                    auto next=item->next();
                    auto prev=item->prev();
                    if (prev)
                    {
                        prev->setNextAuto(next);
                    }

                    for (auto nextItem=item->next();nextItem;)
                    {
                        nextItem->decPos();
                        nextItem=nextItem->next();
                    }

                    item->reset();
                }
            }
        }

        void insertWidget(QWidget *newWidget, QWidget *existingWidget, bool after)
        {
            // check if inserting or reordering not needed
            auto newItem=LinkedListViewItem::getFromWidgetProperty(newWidget);
            if (newItem)
            {
                if (existingWidget==nullptr)
                {
                    if (after)
                    {
                        if (newItem->prev() == nullptr)
                        {
#if 0
                            qDebug() << "LinkedListView_p::insertWidget stays first";
#endif
                            return;
                        }
                    }
                    else
                    {
                        if (newItem->next() == nullptr)
                        {
#if 0
                            qDebug() << "LinkedListView_p::insertWidget stays last";
#endif
                            return;
                        }
                    }
                }
                else
                {
                    auto existingItem=LinkedListViewItem::getFromWidgetProperty(existingWidget);
                    if (existingItem)
                    {
                        if (after)
                        {
                            if (existingItem->next() == newItem)
                            {
#if 0
                                qDebug() << "LinkedListView_p::insertWidget stays in the same position after";
#endif
                                return;
                            }
                        }
                        else
                        {
                            if (existingItem->prev() == newItem)
                            {
#if 0
                                qDebug() << "LinkedListView_p::insertWidget stays in the same position before";
#endif
                                return;
                            }
                        }
                    }
                }
            }
#if 0
            qDebug() << "LinkedListView_p::insertWidget insert to new position";
#endif
            // insert widget
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

                // QBoxLayout::insertWidget reparented the widget as a side effect;
                // do it explicitly, and only when it actually changes, so a mere
                // reorder of an already-owned widget does not hide/re-show it and
                // storm FlyweightListView_q's child event filter
                if (newWidget->parentWidget()!=view)
                {
                    newWidget->setParent(view);
                }
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

            relayout();
            view->updateGeometry();
        }

        /**
         * @brief Reposition and resize all child widgets in a single deterministic pass.
         *
         * Replaces the QBoxLayout that used to do this job. Reproduces
         * QWidgetItem::setGeometry()'s per-child metrics and QStyle::visualAlignment()'s
         * cross-axis default exactly, since FlyweightListView_p's horizontal-mode
         * hit-testing depends on the latter (see itemMaxCross()/isEmptyItem() above).
         */
        void relayout()
        {
            if (inRelayout)
            {
                return;
            }
            inRelayout=true;

            auto rect=view->contentsRect();
            auto visual=QStyle::visualAlignment(view->layoutDirection(),alignment);
            bool crossHorizontal=!isHorizontal();
            bool crossAligned=crossHorizontal ?
                                   bool(alignment & Qt::AlignHorizontal_Mask) :
                                   bool(alignment & Qt::AlignVertical_Mask);

            int mainOrigin=oprop(rect,OProp::pos);
            int mainSpace=oprop(rect,OProp::size);
            int crossOrigin=oprop(rect,OProp::pos,true);
            int crossSpace=oprop(rect,OProp::size,true);

            // pass 1: measure content main extent
            int contentMain=0;
            for (auto item=head.lock(); item; item=item->next())
            {
                auto w=item->widget();
                if (w==nullptr || isEmptyItem(w))
                {
                    continue;
                }
                contentMain+=oprop(itemSizeHint(w),OProp::size);
            }

            // main-axis packing origin: pack to the far edge only if alignment names
            // it and the container exceeds the content, so items are never left with
            // gaps between them (unlike QBoxLayout's own surplus-spreading, which is
            // unreachable here because every caller sizes the list to its sizeHint())
            int pos=mainOrigin;
            if (mainSpace>contentMain)
            {
                bool packAtEnd=crossHorizontal ?
                                   bool(alignment & Qt::AlignBottom) :
                                   bool(visual & Qt::AlignRight);
                if (packAtEnd)
                {
                    pos=mainOrigin+mainSpace-contentMain;
                }
            }

            // pass 2: place
            for (auto item=head.lock(); item; item=item->next())
            {
                auto w=item->widget();
                if (w==nullptr || isEmptyItem(w))
                {
                    continue;
                }

                int mainSize=oprop(itemSizeHint(w),OProp::size);
                int crossSize=qMin(crossSpace,itemMaxCross(w,crossHorizontal,crossAligned));

                int crossPos;
                if (crossHorizontal)
                {
                    if (visual & Qt::AlignRight)
                    {
                        crossPos=crossOrigin+crossSpace-crossSize;
                    }
                    else if (visual & Qt::AlignLeft)
                    {
                        crossPos=crossOrigin;
                    }
                    else
                    {
                        crossPos=crossOrigin+(crossSpace-crossSize)/2;
                    }
                }
                else
                {
                    if (alignment & Qt::AlignBottom)
                    {
                        crossPos=crossOrigin+crossSpace-crossSize;
                    }
                    else if (alignment & Qt::AlignTop)
                    {
                        crossPos=crossOrigin;
                    }
                    else
                    {
                        crossPos=crossOrigin+(crossSpace-crossSize)/2;
                    }
                }

                QPoint p;
                QSize s;
                setOProp(p,OProp::pos,pos);
                setOProp(p,OProp::pos,crossPos,true);
                setOProp(s,OProp::size,mainSize);
                setOProp(s,OProp::size,crossSize,true);
                w->setGeometry(QRect(p,s));

                pos+=mainSize;
            }

            inRelayout=false;
        }

        /**
         * @brief Post a coalesced relayout request.
         *
         * Used on the removal paths, matching Qt's own timing: with a real
         * QLayout, removing/hiding a child only posts QEvent::LayoutRequest
         * (compressed by QApplication), it does not relayout synchronously.
         */
        void scheduleRelayout()
        {
            view->updateGeometry();
            QCoreApplication::postEvent(view,new QEvent(QEvent::LayoutRequest));
        }

        /**
         * @brief Compute the aggregate size hint or minimum size hint of the list.
         *
         * Mirrors QLayout::totalSizeHint()/QBoxLayout's minimum: main axis is the
         * sum of per-child extents, cross axis is the max over children, plus the
         * view's own contentsMargins on both axes.
         */
        QSize calcSizeHint(bool minimum) const
        {
            int main=0;
            int cross=0;
            for (auto item=head.lock(); item; item=item->next())
            {
                auto w=item->widget();
                if (w==nullptr || isEmptyItem(w))
                {
                    continue;
                }
                auto s=minimum?itemMinSize(w):itemSizeHint(w);
                main+=oprop(s,OProp::size);
                cross=qMax(cross,oprop(s,OProp::size,true));
            }

            auto m=view->contentsMargins();
            QSize r;
            setOProp(r,OProp::size,main+oprop(m,OProp::size));
            setOProp(r,OProp::size,cross+oprop(m,OProp::size,true),true);
            return r;
        }

    public:

        LinkedListView* view;
        Qt::Orientation orientation;

        std::weak_ptr<LinkedListViewItem> head;
        bool blockUpdate;

        std::vector<QWidget*> singleWidgetHelper;
        Qt::Alignment alignment;

        bool inRelayout;
};

}

//--------------------------------------------------------------------------
LinkedListView::LinkedListView(
        QWidget *parent,
        Qt::Orientation orientation
    ) : QFrame(parent),
        pimpl(std::make_unique<detail::LinkedListView_p>(this,orientation))
{
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
        auto next=item->next();
        dropWidget(item->widget());
        item=next;
    }
    pimpl->head.reset();
    blockSignals(false);
    pimpl->blockUpdate=false;
    pimpl->scheduleRelayout();
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

        blockSignals(false);
        pimpl->blockUpdate=false;

        pimpl->relayout();
        updateGeometry();
    }
}

//--------------------------------------------------------------------------
void LinkedListView::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    pimpl->relayout();
    emit resized();
}

//--------------------------------------------------------------------------
bool LinkedListView::event(QEvent *event)
{
    switch (event->type())
    {
        case (QEvent::LayoutRequest): [[fallthrough]];
        case (QEvent::ContentsRectChange):
        {
            pimpl->relayout();
        }
        break;

        default:
        break;
    }

    return QFrame::event(event);
}

//--------------------------------------------------------------------------
QSize LinkedListView::sizeHint() const
{
    return pimpl->calcSizeHint(false);
}

//--------------------------------------------------------------------------
QSize LinkedListView::minimumSizeHint() const
{
    return pimpl->calcSizeHint(true);
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
    if (widget->parent() && widget->parent()!=this)
    {
        return;
    }

    takeWidget(widget,true);
}

//--------------------------------------------------------------------------
void LinkedListView::takeWidget(QObject *widget, bool destroyed)
{
    auto item=LinkedListViewItem::getFromWidgetProperty(widget);
    pimpl->takeItem(item,destroyed);
    pimpl->scheduleRelayout();
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
void LinkedListView::setAlignment(Qt::Alignment alignment) noexcept
{
    if (alignment!=pimpl->alignment)
    {
        pimpl->alignment=alignment;
        pimpl->relayout();
    }
}

//--------------------------------------------------------------------------
Qt::Alignment LinkedListView::alignment() const noexcept
{
    return pimpl->alignment;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
