/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** \file uise/desktop/detail/flyweightlistview_p.ipp
*
*  Contains detail implementation of FlyweightListView_p.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FLYWEIGHTLISTVIEW_P_IPP
#define UISE_DESKTOP_FLYWEIGHTLISTVIEW_P_IPP

#include <algorithm>

#include <QApplication>
#include <QResizeEvent>
#include <QStyle>

#include <uise/desktop/detail/flyweightlistview_p.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

namespace detail {

//--------------------------------------------------------------------------
template <typename T>
int GetOProp(bool horizontal, const T& obj, OProp prop) noexcept
{
    if constexpr (std::is_same_v<QPoint,std::decay_t<T>>)
    {
        switch (prop)
        {
            case(OProp::pos):
                return horizontal?obj.x():obj.y();

            case(OProp::size):
            case(OProp::edge):
                return 0;
        }
        return 0;
    }
    else if constexpr (std::is_base_of_v<QWidget,std::decay_t<T>>)
    {
        switch (prop)
        {
            case(OProp::size):
                return horizontal?obj.width():obj.height();

            case(OProp::pos):
                return horizontal?obj.x():obj.y();

            case(OProp::edge):
                return horizontal?obj.geometry().right():obj.geometry().bottom();
        }
        return 0;
    }
    else if constexpr (std::is_same_v<QSize,std::decay_t<T>>)
    {
        return horizontal?obj.width():obj.height();
    }

    return 0;
}

//--------------------------------------------------------------------------
template <typename T>
void SetOProp(bool horizontal, T& obj, OProp prop, int value) noexcept
{
    if constexpr (std::is_same_v<QPoint,std::decay_t<T>>)
    {
        switch (prop)
        {
            case(OProp::pos):
                if (horizontal)
                {
                    obj.setX(value);
                }
                else
                {
                    obj.setY(value);
                }
            break;

            case(OProp::size):
            case(OProp::edge):
            break;
        }
    }
    else if constexpr (std::is_base_of_v<QWidget,std::decay_t<T>>)
    {
        switch (prop)
        {
            case(OProp::size):
                if (horizontal)
                {
                    obj.setWidth(value);
                }
                else
                {
                    obj.setHeight(value);
                }
            break;

            case(OProp::pos):
            case(OProp::edge):
            break;
        }
    }
    else if constexpr (std::is_same_v<QSize,std::decay_t<T>>)
    {
        switch (prop)
        {
            case(OProp::size):
                if (horizontal)
                {
                    obj.setWidth(value);
                }
                else
                {
                    obj.setHeight(value);
                }
            break;

            case(OProp::pos):
            case(OProp::edge):
            break;
        }
    }
}

//--------------------------------------------------------------------------
template <typename ItemT>
template <typename T>
int FlyweightListView_p<ItemT>::oprop(const T& obj, OProp prop, bool other) const noexcept
{
    bool horizontal=isHorizontal();
    if (other)
    {
        horizontal=!horizontal;
    }

    if constexpr (std::is_pointer_v<std::remove_reference_t<T>>)
    {
        return GetOProp(horizontal,*obj,prop);
    }
    else
    {
        return GetOProp(horizontal,obj,prop);
    }
}

//--------------------------------------------------------------------------
template <typename ItemT>
template <typename T>
void FlyweightListView_p<ItemT>::setOProp(T& obj, OProp prop, int value, bool other) noexcept
{
    bool horizontal=isHorizontal();
    if (other)
    {
        horizontal=!horizontal;
    }

    if constexpr (std::is_pointer_v<std::remove_reference_t<T>>)
    {
        SetOProp(horizontal,*obj,prop,value);
    }
    else
    {
        SetOProp(horizontal,obj,prop,value);
    }
}

//--------------------------------------------------------------------------
template <typename ItemT>
template <typename T>
void FlyweightListView_p<ItemT>::setOProp(T& obj, OProp prop, int value, bool other) const noexcept
{
    auto self=const_cast<FlyweightListView_p<ItemT>*>(this);
    self->setOProp(obj,prop,value,other);
}

//--------------------------------------------------------------------------
template <typename ItemT>
FlyweightListView_p<ItemT>::FlyweightListView_p(
        FlyweightListView<ItemT>* view,
        size_t prefetchItemCountHint
    ) : m_view(view),
        m_prefetchItemCount(prefetchItemCountHint),
        m_llist(nullptr),
        m_enableFlyweight(true),
        m_stick(Direction::END),
        m_listSize(QSize(0,0)),
        m_firstViewportItemID(ItemT::defaultId()),
        m_firstViewportSortValue(ItemT::defaultSortValue()),
        m_lastViewportItemID(ItemT::defaultId()),
        m_lastViewportSortValue(ItemT::defaultSortValue()),
        m_singleStep(1),
        m_pageStep(FlyweightListView<ItemT>::DefaultPageStep),
        m_minPageStep(FlyweightListView<ItemT>::DefaultPageStep),
        m_wheelOffsetAccumulated(0.0f),
        m_ignoreUpdates(false),
        m_atBegin(true),
        m_atEnd(true),
        m_firstWidgetPos(0)
{
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::setupUi()
{
    m_view->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    m_view->setFocusPolicy(Qt::StrongFocus);
    m_view->resize(0,0);

    m_llist=new LinkedListView(m_view);
    m_llist->setObjectName("FlyweightListViewLLits");
    m_llist->setFocusProxy(m_view);

    updatePageStep();
    resizeList();

    m_qobjectHelper.setWidgetDestroyedHandler([this](QObject* obj){onWidgetDestroyed(obj);});
    m_qobjectHelper.setListResizedHandler([this](){onListResize();});

    m_view->setStyleSheet("background: blue; padding:2px;");
    m_llist->setStyleSheet("background: red; padding:0px;");

    if (m_llist->frameWidth()!=0)
    {
        auto err=QString("CSS border, margin and padding for LinkedListView(FlyweightListViewLLits) must be 0 (actual: %1)").arg(m_llist->frameWidth());
        qCritical()<<err;

        Q_ASSERT_X(m_llist->frameWidth()==0,"FlyweightListView",err.toStdString().data());
    }

    keepCurrentConfiguration();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::beginUpdate()
{
    m_ignoreUpdates=true;
    keepCurrentConfiguration();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::endUpdate()
{
    resizeList();

    if ((m_atEnd && m_stick==Direction::END ) || (m_atBegin && m_stick==Direction::HOME))
    {
        scrollToEdge(m_stick);
    }
    else
    {
        compensateSizeChange();
    }

    m_ignoreUpdates=false;
    viewportUpdated();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::onListResize()
{
    if (!m_ignoreUpdates)
    {
        // update sticking positions
        scroll(1);
        scroll(-1);
    }
    viewportUpdated();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::configureWidget(const ItemT* item)
{
    auto widget=item->widget();
    PointerHolder::keepProperty(item,widget,ItemT::Property);
    QObject::disconnect(widget,SIGNAL(destroyed(QObject*)),&m_qobjectHelper,SLOT(onWidgetDestroyed(QObject*)));
    QObject::connect(widget,SIGNAL(destroyed(QObject*)),&m_qobjectHelper,SLOT(onWidgetDestroyed(QObject*)));
    widget->removeEventFilter(&m_qobjectHelper);
    widget->installEventFilter(&m_qobjectHelper);
}

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView_p<ItemT>::prefetchItemCount() noexcept
{
    return autoPrefetchCount();
}

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView_p<ItemT>::autoPrefetchCount() noexcept
{
    m_prefetchItemCount=std::max(m_prefetchItemCount,visibleCount()*2);
    return m_prefetchItemCount;
}

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView_p<ItemT>::maxHiddenItemsBeyondEdge() noexcept
{
    return prefetchItemCount()*2;
}

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView_p<ItemT>::prefetchThreshold() noexcept
{
    return prefetchItemCount();
}

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView_p<ItemT>::itemsCount() const noexcept
{
    return m_items.template get<1>().size();
}

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView_p<ItemT>::visibleCount() const noexcept
{
    return 0;
}

//--------------------------------------------------------------------------
template <typename ItemT>
bool FlyweightListView_p<ItemT>::hasItem(const typename ItemT::IdType& id) const noexcept
{
    const auto& idx=m_items.template get<1>();
    auto it=idx.find(id);
    return it!=idx.end();
}

//--------------------------------------------------------------------------
template <typename ItemT>
const ItemT* FlyweightListView_p<ItemT>::item(const typename ItemT::IdType &id) const noexcept
{
    const auto& idx=m_items.template get<0>();
    auto it=idx.find(id);
    return (it!=idx.end())?&(*it):nullptr;
}

//--------------------------------------------------------------------------
template <typename ItemT>
const ItemT* FlyweightListView_p<ItemT>::firstItem() const noexcept
{
    const auto& idx=m_items.template get<0>();
    auto it=idx.begin();
    return (it!=idx.end())?&(*it):nullptr;
}

//--------------------------------------------------------------------------
template <typename ItemT>
const ItemT* FlyweightListView_p<ItemT>::lastItem() const noexcept
{
    const auto& idx=m_items.template get<0>();
    auto it=idx.rbegin();
    return it!=idx.rend()?&(*it):nullptr;
}

//--------------------------------------------------------------------------
template <typename ItemT>
bool FlyweightListView_p<ItemT>::isHorizontal() const noexcept
{
    return m_llist->orientation()==Qt::Horizontal;
}

//--------------------------------------------------------------------------
template <typename ItemT>
bool FlyweightListView_p<ItemT>::isVertical() const noexcept
{
    return !isHorizontal();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::onWidgetDestroyed(QObject* obj)
{
    auto item=PointerHolder::getProperty<ItemT*>(obj,ItemT::Property);
    if (item)
    {
        auto& idx=m_items.template get<1>();
        idx.erase(item->id());

        if (!m_ignoreUpdates)
        {
            endUpdate();
        }
    }
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::setFlyweightEnabled(bool enable) noexcept
{
    m_enableFlyweight=enable;
}

//--------------------------------------------------------------------------
template <typename ItemT>
bool FlyweightListView_p<ItemT>::isFlyweightEnabled() const noexcept
{
    return m_enableFlyweight;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::onListUpdated()
{
    // if stickToEnd and last widget was in viewport then new last widget must stay in viewport
    // if size of widgets before viewport changed then list must be moved to compensate size change
}

//--------------------------------------------------------------------------
template <typename ItemT>
QPoint FlyweightListView_p<ItemT>::viewportBegin() const
{
    return QPoint(0,0)-m_llist->pos();
}

//--------------------------------------------------------------------------
template <typename ItemT>
QPoint FlyweightListView_p<ItemT>::viewportEnd() const
{
    auto pos=m_llist->pos();

    auto propSize=oprop(m_llist,OProp::size);
    if (propSize>0)
    {
        setOProp(pos,OProp::pos,oprop(pos,OProp::pos)+propSize-1);
        setOProp(pos,OProp::pos,0,true);
    }

    return pos;
}

//--------------------------------------------------------------------------
template <typename ItemT>
bool FlyweightListView_p<ItemT>::isAtBegin() const
{
    return oprop(m_llist->pos(),OProp::pos)==0;
}

//--------------------------------------------------------------------------
template <typename ItemT>
bool FlyweightListView_p<ItemT>::isAtEnd() const
{
    return oprop(viewportEnd(),OProp::pos)<=(oprop(m_view,OProp::size)-1);
}

//--------------------------------------------------------------------------
template <typename ItemT>
int FlyweightListView_p<ItemT>::endItemEdge() const
{
    return oprop(m_llist,OProp::edge);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::onViewportResized(QResizeEvent *event)
{
    keepCurrentConfiguration();
    m_viewSize=event->oldSize();
    if (!m_viewSize.isValid())
    {
        m_viewSize=QSize(0,0);
    }

    bool moveList=false;
    QPoint movePos=m_llist->pos();

    // check if list must be moved
    auto oldViewSize=oprop(m_viewSize,OProp::size);
    auto viewSize=oprop(m_view,OProp::size);
    auto edge=endItemEdge();
    bool moveEnd=edge==(oldViewSize-1)
            ||
            (edge>(viewSize-1)) && (viewSize<oldViewSize)
            ||
            (edge<(viewSize-1)) && (viewSize>oldViewSize)
    ;
    bool moveBegin=(edge<(viewSize-1)) && (viewSize>oldViewSize);

    // if size of viewport changed then list will try to fit the viewport as much as possible
    if (m_stick==Direction::HOME && moveBegin || m_stick==Direction::END && moveEnd)
    {
        auto delta=viewSize-oldViewSize;
        auto newPos=oprop(m_llist,OProp::pos)+delta;
        auto listSize=oprop(m_llist,OProp::size);
        if ((newPos+listSize)<0)
        {
            newPos=0;
            if (m_stick==Direction::END)
            {
                newPos=viewSize-listSize;
            }
        }
        if (newPos>0)
        {
            newPos=0;
        }
        setOProp(movePos,OProp::pos,newPos);
        moveList=true;
    }
    if (moveList)
    {
        m_llist->move(movePos);
    }

    // update page step
    updatePageStep();

    // resize only that dimension of the list that doesn't match the orientation
    auto listSize=m_listSize;
    QSize newListSize;
    setOProp(newListSize,OProp::size,oprop(m_llist,OProp::size));
    setOProp(newListSize,OProp::size,oprop(event->size(),OProp::size,true),true);
    m_llist->resize(newListSize);

    // if m_llist was resized then events are already handled, so invoke below only if the list was not resized
    if (m_llist->size()==listSize)
    {
        viewportUpdated();
    }
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::compensateSizeChange()
{
    const ItemT* oldItem=nullptr;

    auto& idx=m_items.template get<1>();
    auto& order=m_items.template get<0>();
    if (auto it=idx.find(m_firstViewportItemID); it!=idx.end())
    {
        oldItem=&(*it);
    }
    else if (auto it=order.find(m_firstViewportSortValue); it!=order.end())
    {
        oldItem=&(*it);
    }
    else
    {
        for (auto it=order.begin();it!=order.end();++it)
        {
            if (it->sortValue()>=m_firstViewportSortValue)
            {
                oldItem=&(*it);
                break;
            }
        }
    }
    if (!oldItem)
    {
        return;
    }
    auto oldWidget=oldItem->widget();
    if (!oldWidget)
    {
        return;
    }

    auto oldWidgetPos=oprop(oldWidget,OProp::pos);
    if (oldWidgetPos!=m_firstWidgetPos)
    {
        auto delta=m_firstWidgetPos-oldWidgetPos;
        auto pos=m_llist->pos();
        setOProp(pos,OProp::pos,oprop(pos,OProp::pos)+delta);
        m_llist->move(pos);
    }
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::setOrientation(Qt::Orientation orientation)
{
    beginUpdate();
    clear();
    m_llist->setOrientation(orientation);
    updatePageStep();
    endUpdate();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::updatePageStep()
{
    m_pageStep=std::max(
                static_cast<size_t>(oprop(m_view,OProp::size)),
                m_minPageStep
                );
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::viewportUpdated()
{
    if (m_ignoreUpdates)
    {
        return;
    }

    onListUpdated();
    keepCurrentConfiguration();
    informViewportUpdated();
    checkNewItemsNeeded();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::informViewportUpdated()
{
    //! @todo Implement informViewportUpdated()
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::checkNewItemsNeeded()
{
    if (!m_enableFlyweight)
    {
        return;
    }

    //! @todo Implement checkNewItemsNeeded()
}

//--------------------------------------------------------------------------
template <typename ItemT>
QWidget* FlyweightListView_p<ItemT>::insertItemToContainer(const ItemT& item, bool findAfterWidget)
{
    auto& idx=m_items.template get<1>();
    auto result=idx.insert(item);
    if (!result.second)
    {
        Q_ASSERT(result.first->widget()==item.widget());
        Q_ASSERT(idx.replace(result.first,item));
    }
    else
    {
        configureWidget(&(*result.first));
    }

    QWidget* afterWidget=nullptr;
    if (findAfterWidget)
    {
        auto& order=m_items.template get<0>();
        auto it=m_items.template project<0>(result.first);
        if (it!=order.begin())
        {
            afterWidget=(--it)->widget();
        }
    }

    return afterWidget;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::insertItem(const ItemT& item)
{
    m_llist->insertWidgetAfter(item.widget(),insertItemToContainer(item));
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::insertContinuousItems(const std::vector<ItemT>& items)
{
    if (items.empty())
    {
        return;
    }

    std::vector<QWidget*> widgets;
    QWidget* afterWidget=nullptr;
    for (size_t i=0;i<items.size();i++)
    {
        const auto& item=items[i];
        auto afterWidgetTmp=insertItemToContainer(item,i==0);
        if (i==0)
        {
            afterWidget=afterWidgetTmp;
        }
        widgets.push_back(item.widget());
    }

    m_llist->insertWidgetsAfter(widgets,afterWidget);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::resizeList()
{
    auto newSize=oprop(m_llist->sizeHint(),OProp::size);
    QSize listSize;
    setOProp(listSize,OProp::size,oprop(m_view,OProp::size,true),true);
    setOProp(listSize,OProp::size,newSize);
    if (m_llist->size()!=listSize)
    {
        m_llist->resize(listSize);

        // update sticking positions
        scroll(1);
        scroll(-1);
    }
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::clear()
{
    const auto& order=m_items.template get<0>();

    for (auto&& it : order)
    {
        PointerHolder::clearProperty(it.widget(),ItemT::Property);
    }

    m_llist->blockSignals(true);
    m_llist->clear(ItemT::dropWidgetHandler());
    m_llist->move(0,0);
    QSize newListSize;
    setOProp(newListSize,OProp::size,oprop(m_llist->sizeHint(),OProp::size));
    setOProp(newListSize,OProp::size,oprop(m_view,OProp::size,true),true);
    m_llist->resize(newListSize);
    m_llist->blockSignals(false);

    m_items.clear();

    m_listSize=m_llist->size();
    m_firstViewportItemID=ItemT::defaultId();
    m_firstViewportSortValue=ItemT::defaultSortValue();
    m_lastViewportItemID=ItemT::defaultId();
    m_lastViewportSortValue=ItemT::defaultSortValue();
    m_wheelOffsetAccumulated=0.0f;
    m_atBegin=true;
    m_atEnd=true;
    m_firstWidgetPos=0;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::scroll(int delta)
{
    auto cb=[delta](int minPos, int maxPos, int oldPos)
    {
        std::ignore=minPos;
        std::ignore=maxPos;
        return oldPos-delta;
    };

    scrollTo(cb);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::wheelEvent(QWheelEvent *event)
{
    auto numPixels = event->pixelDelta();
    auto angleDelta = event->angleDelta();

#ifndef Q_WS_X11 // Qt documentation says that on X11 pixelDelta() is unreliable and should not be used
   if (!numPixels.isNull())
   {
       scroll(-oprop(numPixels,OProp::pos));
   }
   else if (!angleDelta.isNull())
#endif
   {
       auto deltaPos=qreal(oprop(angleDelta,OProp::pos));
       auto scrollLines=QApplication::wheelScrollLines();
       auto numStepsU = m_singleStep * scrollLines * deltaPos / 120;

       if (qAbs(m_wheelOffsetAccumulated)>std::numeric_limits<decltype(m_wheelOffsetAccumulated)>::epsilon()
           &&
           (numStepsU/m_wheelOffsetAccumulated)<0
           )
       {
           m_wheelOffsetAccumulated=0.0f;
       }

       m_wheelOffsetAccumulated+=numStepsU;
       auto numSteps=static_cast<int>(m_wheelOffsetAccumulated);
       m_wheelOffsetAccumulated-=numSteps;

       scroll(-numSteps);
   }

   event->accept();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::scrollTo(const std::function<int (int, int, int)> &cb)
{
    auto viewportSize=oprop(m_view,OProp::size);
    auto listSize=oprop(m_llist,OProp::size);

    int minPos=0;
    if (listSize>viewportSize)
    {
        minPos=viewportSize-listSize;
    }
    int maxPos=0;

    auto pos=m_llist->pos();
    auto posCoordinate=oprop(pos,OProp::pos);
    auto newCoordinate=cb(minPos,maxPos,posCoordinate);

    newCoordinate=qBound(minPos,newCoordinate,maxPos);
    if (newCoordinate!=posCoordinate)
    {
        setOProp(pos,OProp::pos,newCoordinate);
        m_llist->move(pos);
        viewportUpdated();
    }
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::scrollToEdge(Direction direction)
{
    auto cb=[direction](int minPos, int maxPos, int oldPos)
    {
        std::ignore=oldPos;
        switch (direction)
        {
            case Direction::END:
                return minPos; // because pos is negative
                break;

            case Direction::HOME:
                return maxPos; // because pos is negative
                break;

            default:
                break;
        }

        return 0;
    };

    scrollTo(cb);
}

//--------------------------------------------------------------------------
template <typename ItemT>
bool FlyweightListView_p<ItemT>::scrollToItem(const typename ItemT::IdType &id, size_t offset)
{
    auto& idx=m_items.template get<1>();
    auto it=idx.find(id);
    if (it==idx.end())
    {
        return false;
    }

    auto cb=[offset,&it,this](int minPos, int maxPos, int oldPos)
    {
        auto widget=it->widget();
        if (widget && widget->parent()==m_llist)
        {
            QPoint widgetListPos=widget->pos();
            auto widgetSize=oprop(widget,OProp::size);
            auto widgetViewPos=oprop(m_llist->mapToParent(widgetListPos),OProp::pos);

            auto widgetBegin=oldPos-widgetViewPos;
            int newPos=widgetBegin-offset;

            QPoint newListPos;
            setOProp(newListPos,OProp::pos,newPos);
            setOProp(newListPos,OProp::pos,0,true);
            auto newViewPos=oprop(m_llist->mapFromParent(newListPos),OProp::pos);
            auto newViewEnd=newViewPos+widgetSize;
            auto viewSize=oprop(m_view,OProp::size);
            if (
                    (newViewPos>viewSize) // widget would move after viewport
                    ||
                    newViewEnd<=0 // widget would move before viewport
                )
            {
                // ignore invalid offset
                newPos=widgetBegin;
            }

            newPos=qBound(minPos,newPos,maxPos);

            return newPos;
        };

        return oldPos;
    };

    scrollTo(cb);
    return true;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::keepCurrentConfiguration()
{
    m_listSize=m_llist->size();
    m_viewSize=m_view->size();

    auto keep=[](const ItemT* item, typename ItemT::IdType& id, typename ItemT::SortValueType& sortValue)
    {
        if (item)
        {
            id=item->id();
            sortValue=item->sortValue();
        }
        else
        {
            id=ItemT::defaultId();
            sortValue=ItemT::defaultSortValue();
        }
    };

    const auto* item=firstViewportItem();
    if (item && item->widget())
    {
        m_firstWidgetPos=oprop(item->widget(),OProp::pos);
    }
    else
    {
        m_firstWidgetPos=0;
    }

    keep(item,m_firstViewportItemID,m_firstViewportSortValue);
    item=lastViewportItem();
    keep(item,m_lastViewportItemID,m_lastViewportSortValue);

    m_atBegin=isAtBegin();
    m_atEnd=isAtEnd();
}

//--------------------------------------------------------------------------
template <typename ItemT>
const ItemT* FlyweightListView_p<ItemT>::itemAtPos(const QPoint &pos) const
{
    const auto* widget=m_llist->childAt(pos);
    return PointerHolder::getProperty<const ItemT*>(widget,ItemT::Property);
}

//--------------------------------------------------------------------------
template <typename ItemT>
const ItemT* FlyweightListView_p<ItemT>::firstViewportItem() const
{
    return itemAtPos(viewportBegin());
}

//--------------------------------------------------------------------------
template <typename ItemT>
const ItemT* FlyweightListView_p<ItemT>::lastViewportItem() const
{
    const auto* item=itemAtPos(viewportEnd());
    if (item==nullptr)
    {
        item=lastItem();
    }
    return item;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::clearWidget(QWidget* widget)
{
    if (!widget)
    {
        return;
    }

    PointerHolder::clearProperty(widget,ItemT::Property);
    m_llist->takeWidget(widget);
    widget->removeEventFilter(&m_qobjectHelper);
    QObject::disconnect(widget,SIGNAL(destroyed(QObject*)),&m_qobjectHelper,SLOT(onWidgetDestroyed(QObject*)));
    ItemT::dropWidget(widget);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::removeItem(const typename ItemT::IdType &id)
{
    auto& idx=m_items.template get<1>();
    auto it=idx.find(id);
    if (it!=idx.end())
    {
        auto* item=const_cast<ItemT*>(&(*it));
        removeItem(item);
    }
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::removeItem(ItemT* item)
{
    clearWidget(item->widget());

    auto& idx=m_items.template get<1>();
    idx.erase(item->id());
}

#if 0

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView_p<ItemT>::removeExtraHiddenItems()
{
    size_t removedSize=0;
#if 0
    qDebug() << "removeExtraHiddenItems direction="<<static_cast<int>(m_scrolled);

    if (m_scrolled==Direction::END)
    {
        if (m_lastBeginWidget)
        {
            auto maxHiddenCount=maxHiddenItemsBeyondEdge();
            if (m_hiddenBefore>maxHiddenCount)
            {
                auto removeCount=m_hiddenBefore-maxHiddenCount;

                qDebug() << "Remove fromBegin "<<removeCount;

                removedSize=removeItemsFromBegin(removeCount);
            }
        }
    }
    else if (m_scrolled==Direction::HOME)
    {
        if (m_lastEndWidget)
        {
            auto maxHiddenCount=maxHiddenItemsBeyondEdge();
            if (m_hiddenAfter>maxHiddenCount)
            {
                auto removeCount=m_hiddenAfter-maxHiddenCount;

                qDebug() << "Remove fromEnd "<<removeCount;

                removedSize=removeItemsFromEnd(removeCount);
            }
        }
    }
#endif
    return removedSize;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::checkNeedsItemsBefore()
{
#if 0
    if (m_scrolled!=Direction::HOME)
    {
        return;
    }

    if ((m_hiddenBefore<prefetchThreshold() || count()==0) && m_requestItemsBeforeCb)
    {
        const ItemT* firstItem=nullptr;
        const auto& order=m_items.template get<0>();
        auto it=order.begin();
        if (it!=order.end())
        {
            firstItem=&(*it);
        }

        m_requestPending=true;
        m_requestItemsBeforeCb(firstItem,prefetchItemCount());
    }
#endif
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::checkNeedsItemsAfter()
{
#if 0
    if (m_scrolled!=Direction::END && !m_lastEndWidget)
    {
        return;
    }

    if ((m_hiddenAfter<prefetchThreshold() || !m_lastEndWidget ) && m_requestItemsAfterCb)
    {
        const ItemT* lastItem=nullptr;
        const auto& order=m_items.template get<0>();
        auto it=order.rbegin();
        if (it!=order.rend())
        {
            lastItem=&(*it);
        }

        m_requestPending=true;
        m_requestItemsAfterCb(lastItem,prefetchItemCount());
    }
#endif
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::checkNeedsMoreItems()
{
#if 0
    if (!m_enableFlyweight)
    {
        return;
    }

    auto beginWidget=m_lastBeginWidget;
    auto endWidget=m_lastEndWidget;

    onViewportUpdated();

    bool updated=m_autoLoad || m_lastBeginWidget.get()!=beginWidget || m_lastEndWidget!=endWidget
                || count()>0&&!m_lastEndWidget;
    if (updated && !m_requestPending)
    {
        checkNeedsItemsBefore();
        checkNeedsItemsAfter();
    }
#endif
}

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView_p<ItemT>::removeItemsFromBegin(size_t count, size_t offset)
{
    size_t removedSize=0;
#if 0
    qDebug() << "Remove from begin "<<count<<"/"<<this->count();

    if (count==0)
    {
        return 0;
    }

    auto& order=m_items.template get<0>();
    for (auto it=order.begin();it!=order.end();)
    {
        if (offset>0)
        {
            --offset;
            continue;
        }

        removedSize+=isHorizontal()?it->widget()->width() : it->widget()->height();
        clearWidget(it->widget());
        it=order.erase(it);

        if (--count==0)
        {
            break;
        }
    }
#endif
    return removedSize;
}

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView_p<ItemT>::removeItemsFromEnd(size_t count, size_t offset)
{
    size_t removedSize=0;

#if 0
    qDebug() << "Remove from end "<<count<<"/"<<this->count();

    if (count==0)
    {
        return 0;
    }

    auto& order=m_items.template get<0>();
    for (auto it=order.rbegin(), nit=it;it!=order.rend(); it=nit)
    {
        if (offset>0)
        {
            --offset;
            continue;
        }

        nit=std::next(it);

        removedSize+=isHorizontal()?it->widget()->width() : it->widget()->height();
        clearWidget(it->widget());

        nit = decltype(it){order.erase(std::next(it).base())};

        if (--count==0)
        {
            break;
        }
    }
#endif
    return removedSize;
}
#endif

} // namespace detail

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_EMD

#endif // UISE_DESKTOP_FLYWEIGHTLISTVIEW_P_IPP
