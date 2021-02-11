/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/desktop/detail/flyweightlistview_p.ipp
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

#include <uise/desktop/utils/directchildwidget.hpp>

#include <uise/desktop/detail/flyweightlistview_p.hpp>

//#define UISE_DEAKTOP_FLYWEIGHTLISTVIEW_DEBUG

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

namespace detail {

//--------------------------------------------------------------------------
template <typename ItemT>
FlyweightListView_p<ItemT>::FlyweightListView_p(
        FlyweightListView<ItemT>* view,
        size_t prefetchItemCountHint
    ) : m_obj(view),
        m_vbar(nullptr),
        m_hbar(nullptr),
        m_view(nullptr),
        m_prefetchItemCount(prefetchItemCountHint),
        m_prefetchItemCountHint(prefetchItemCountHint),
        m_llist(nullptr),
        m_enableFlyweight(true),
        m_stick(Direction::END),
        m_listSize(QSize(0,0)),
        m_firstViewportItemID(ItemT::defaultId()),
        m_firstViewportSortValue(ItemT::defaultSortValue()),
        m_lastViewportItemID(ItemT::defaultId()),
        m_lastViewportSortValue(ItemT::defaultSortValue()),
        m_atBegin(true),
        m_atEnd(true),
        m_firstWidgetPos(0),
        m_scrollValue(0),
        m_singleStep(1),
        m_pageStep(FlyweightListView<ItemT>::DefaultPageStep),
        m_minPageStep(FlyweightListView<ItemT>::DefaultPageStep),
        m_wheelOffsetAccumulated(0.0f),
        m_wheelOffsetAccumulatedOther(0.0f),
        m_ignoreUpdates(false),
        m_cleared(false),
        m_firstItem(nullptr),
        m_lastItem(nullptr),
        m_maxSortValue(ItemT::defaultSortValue()),
        m_minSortValue(ItemT::defaultSortValue()),
        m_vbarPolicy(Qt::ScrollBarAsNeeded),
        m_hbarPolicy(Qt::ScrollBarAsNeeded),
        m_scrollWheelHorizontal(true)
{
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::setupUi()
{
    auto vlayout=Layout::vertical(m_obj);

    auto middleFrame=new QFrame(m_obj);
    vlayout->addWidget(middleFrame,1);
    m_hbar=new QScrollBar(m_obj);
    m_hbar->setOrientation(Qt::Horizontal);
    vlayout->addWidget(m_hbar);
    auto hlayout=Layout::horizontal(middleFrame);

    m_view=new QFrame(middleFrame);
    hlayout->addWidget(m_view,1);
    m_vbar=new QScrollBar(middleFrame);
    hlayout->addWidget(m_vbar);

    m_view->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    m_view->setFocusPolicy(Qt::StrongFocus);
    m_view->resize(0,0);

    m_llist=new LinkedListView(m_view);
    m_llist->setObjectName("FlyweightListViewLLits");
    m_llist->setFocusProxy(m_view);

    updatePageStep();
    resizeList();

    m_qobjectHelper.setWidgetDestroyedHandler([this](QObject* obj){onWidgetDestroyed(obj);});
    m_qobjectHelper.setListResizeHandler([this](){onListContentResized();});

    if (m_llist->frameWidth()!=0)
    {
        auto err=QString("CSS border, margin and padding for LinkedListView(FlyweightListViewLLits) must be 0 (actual: %1)").arg(m_llist->frameWidth());
        qCritical()<<err;

        //! @todo Support other frame widths using contentsRect()

        Q_ASSERT_X(m_llist->frameWidth()==0,"FlyweightListView",err.toStdString().data());
    }

    keepCurrentConfiguration();

    updateScrollBarOrientation();
    QObject::connect(m_vbar,&QScrollBar::valueChanged,[this](int value){m_vScrollCb(value);});
    QObject::connect(m_hbar,&QScrollBar::valueChanged,[this](int value){m_hScrollCb(value);});

    QTimer::singleShot(0,m_obj,[this](){onResized();});
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::beginUpdate()
{
    m_ignoreUpdates=true;
    beginItemRangeChange();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::endUpdate()
{
    resizeList();
    m_ignoreUpdates=false;
    endItemRangeChange();
    viewportUpdated();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::onListContentResized()
{
    m_resizeListTimer.shot(0,
        [this]()
        {
            resizeList();
            viewportUpdated();
        }
    );
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
    return prefetchItemCount()*0.75;
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
    size_t count=0;

    auto first=firstViewportItem();
    auto last=lastViewportItem();
    if (first&&last)
    {
        auto firstPos=m_llist->widgetSeqPos(first->widget());
        auto lastPos=m_llist->widgetSeqPos(last->widget());
        count=lastPos-firstPos+1;
    }

    return count;
}

//--------------------------------------------------------------------------
template <typename ItemT>
bool FlyweightListView_p<ItemT>::hasItem(const typename ItemT::IdType& id) const noexcept
{
    const auto& idx=itemIdx();
    auto it=idx.find(id);
    return it!=idx.end();
}

//--------------------------------------------------------------------------
template <typename ItemT>
const ItemT* FlyweightListView_p<ItemT>::item(const typename ItemT::IdType &id) const noexcept
{
    const auto& idx=itemIdx();
    auto it=idx.find(id);
    return (it!=idx.end())?&(*it):nullptr;
}

//--------------------------------------------------------------------------
template <typename ItemT>
const auto& FlyweightListView_p<ItemT>::itemOrder() const noexcept
{
    return m_items.template get<0>();
}

//--------------------------------------------------------------------------
template <typename ItemT>
auto& FlyweightListView_p<ItemT>::itemOrder() noexcept
{
    return m_items.template get<0>();
}

//--------------------------------------------------------------------------
template <typename ItemT>
const auto& FlyweightListView_p<ItemT>::itemIdx() const noexcept
{
    return m_items.template get<1>();
}

//--------------------------------------------------------------------------
template <typename ItemT>
auto& FlyweightListView_p<ItemT>::itemIdx() noexcept
{
    return m_items.template get<1>();
}

//--------------------------------------------------------------------------
template <typename ItemT>
const ItemT* FlyweightListView_p<ItemT>::firstItem() const noexcept
{
    const auto& order=itemOrder();
    auto it=order.begin();
    return (it!=order.end())?&(*it):nullptr;
}

//--------------------------------------------------------------------------
template <typename ItemT>
const ItemT* FlyweightListView_p<ItemT>::lastItem() const noexcept
{
    const auto& order=itemOrder();
    auto it=order.rbegin();
    return it!=order.rend()?&(*it):nullptr;
}

//--------------------------------------------------------------------------
template <typename ItemT>
bool FlyweightListView_p<ItemT>::isHorizontal() const noexcept
{
    return m_llist->orientation()==Qt::Horizontal;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::onWidgetDestroyed(QObject* obj)
{
    auto item=PointerHolder::getProperty<ItemT*>(obj,ItemT::Property);
    if (item)
    {
        auto& idx=itemIdx();
        idx.erase(item->id());
        m_resizeListTimer.shot(0,
            [this]()
            {
                resizeList();
            }
        );
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
QPoint FlyweightListView_p<ItemT>::viewportBegin() const
{
    //! @note Using middle point of the llist, so the items must be centered in the view!

    QPoint pos;
    setOProp(pos,OProp::pos,oprop(m_llist,OProp::size,true)/2,true);
    setOProp(pos,OProp::pos,-oprop(m_llist,OProp::pos));
    return pos;
}

//--------------------------------------------------------------------------
template <typename ItemT>
QPoint FlyweightListView_p<ItemT>::listEndInViewport() const
{
    auto pos=m_llist->pos();
    auto propSize=oprop(m_llist,OProp::size);
    if (propSize>0)
    {
        setOProp(pos,OProp::pos,oprop(pos,OProp::pos)+propSize-1);
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
    return oprop(listEndInViewport(),OProp::pos)<=(oprop(m_view,OProp::size)-1);
}

//--------------------------------------------------------------------------
template <typename ItemT>
int FlyweightListView_p<ItemT>::endItemEdge() const
{
    return oprop(m_llist,OProp::edge);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::updateStickingPositions()
{
    auto begin=oprop(m_llist->pos(),OProp::pos);
    auto end=oprop(listEndInViewport(),OProp::pos);
    auto viewPortSize=oprop(m_view,OProp::size);
    if (begin>0)
    {
        scrollToEdge(Direction::HOME);
    }
    else if (end<(viewPortSize-1))
    {
        scrollToEdge(Direction::END);
    }
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::onResized()
{
    auto margins=m_obj->contentsMargins();
    m_hbar->resize(m_obj->width()-m_vbar->width()-margins.left()-margins.right(),m_hbar->height());
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::onViewportResized(QResizeEvent *event)
{
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
    auto otherSize=oprop(event->size(),OProp::size,true);
    auto otherHintSize=oprop(m_llist->sizeHint(),OProp::size,true);
    otherSize=std::max(otherHintSize,otherSize);
    QSize newListSize;
    setOProp(newListSize,OProp::size,oprop(m_llist,OProp::size));
    setOProp(newListSize,OProp::size,otherSize,true);
    m_llist->resize(newListSize);

    // process updated viewport
    viewportUpdated();

    // update stick positions
    m_updateStickingPositionsTimer.shot(
        0,
        [this]()
        {
            updateStickingPositions();
        }
    );
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::compensateSizeChange()
{
    if ((m_atEnd && m_stick==Direction::END ) || (m_atBegin && m_stick==Direction::HOME))
    {
        scrollToEdge(m_stick);
        return;
    }

    const ItemT* oldItem=nullptr;

    const auto& idx=itemIdx();
    const auto& order=itemOrder();
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

    m_updateStickingPositionsTimer.shot(
        0,
        [this]()
        {
            updateStickingPositions();
        }
    );
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::updateScrollBarOrientation()
{
    if (isHorizontal())
    {
        m_hScrollCb=[this](int value){onMainSbarChanged(value);};
        m_vScrollCb=[this](int value){onOtherSbarChanged(value);};

        m_vbar->setSingleStep(1);
        m_vbar->setPageStep(FlyweightListView<ItemT>::DefaultPageStep);

        m_hbar->setSingleStep(m_singleStep);
        m_hbar->setPageStep(m_pageStep);
    }
    else
    {
        m_vScrollCb=[this](int value){onMainSbarChanged(value);};
        m_hScrollCb=[this](int value){onOtherSbarChanged(value);};

        m_vbar->setSingleStep(m_singleStep);
        m_vbar->setPageStep(m_pageStep);

        m_hbar->setSingleStep(1);
        m_hbar->setPageStep(FlyweightListView<ItemT>::DefaultPageStep);
    }
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::setOrientation(Qt::Orientation orientation)
{
    beginUpdate();
    clear();
    m_llist->setOrientation(orientation);
    updateScrollBarOrientation();
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
    if (isHorizontal())
    {
        m_hbar->setPageStep(m_pageStep);
        m_vbar->setPageStep(FlyweightListView<ItemT>::DefaultPageStep);
    }
    else
    {
        m_hbar->setPageStep(FlyweightListView<ItemT>::DefaultPageStep);
        m_vbar->setPageStep(m_pageStep);
    }
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::viewportUpdated()
{
    if (m_ignoreUpdates)
    {
        return;
    }

    informViewportUpdated();

    m_checkItemCountTimer.shot(
        10,
        [this]()
        {
            checkItemCount();
        }
    );
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::informViewportUpdated()
{
    auto l_scrollValue=m_scrollValue;
    auto l_listSize=oprop(m_listSize,OProp::size);
    auto l_firstViewportItemID=m_firstViewportItemID;
    auto l_firstViewportSortValue=m_firstViewportSortValue;
    auto l_lastViewportItemID=m_lastViewportItemID;
    auto l_lastViewportSortValue=m_lastViewportSortValue;
    auto l_cleared=m_cleared;
    m_cleared=false;

    keepCurrentConfiguration();

    m_scrollBarsTimer.shot(0,[this](){updateScrollBars();});

    if (
            l_cleared ||
            l_firstViewportItemID!=m_firstViewportItemID ||
            l_firstViewportSortValue!=m_firstViewportSortValue ||
            l_lastViewportItemID!=m_lastViewportItemID ||
            l_lastViewportSortValue!=m_lastViewportSortValue
        )
    {
        m_informViewportUpdateTimer.shot(1,
            [this]()
            {
                if (m_viewportChangedCb)
                {
                    m_viewportChangedCb(item(m_firstViewportItemID),item(m_lastViewportItemID));
                }
            }
        );
    }    
}

//--------------------------------------------------------------------------
template <typename ItemT>
QWidget* FlyweightListView_p<ItemT>::insertItemToContainer(const ItemT& item, bool findAfterWidget)
{
    auto& idx=itemIdx();
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
        const auto& order=itemOrder();
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
    auto otherSize=oprop(m_view,OProp::size,true);
    auto otherHintSize=oprop(m_llist->sizeHint(),OProp::size,true);
    otherSize=std::max(otherHintSize,otherSize);
    setOProp(listSize,OProp::size,otherSize,true);
    setOProp(listSize,OProp::size,newSize);
    if (m_llist->size()!=listSize)
    {
        m_llist->resize(listSize);
        compensateSizeChange();
    }
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::clear()
{
    const auto& order=itemOrder();

    for (auto&& it : order)
    {
        PointerHolder::clearProperty(it.widget(),ItemT::Property);
        clearWidget(it.widget());
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
    m_wheelOffsetAccumulatedOther=0.0f;
    m_atBegin=true;
    m_atEnd=true;
    m_firstWidgetPos=0;
    m_scrollValue=0;
    m_prefetchItemCount=m_prefetchItemCountHint;

    m_cleared=true;
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

    int scrollOther=0;
    int scrollMain=0;

#ifndef Q_WS_X11 // Qt documentation says that on X11 pixelDelta() is unreliable and should not be used
   if (!numPixels.isNull())
   {
       scrollMain=oprop(numPixels,OProp::pos);
       scrollOther=oprop(numPixels,OProp::pos,true);
   }
   else if (!angleDelta.isNull())
#endif
   {
       auto evalOffset=[this,&angleDelta](float& accumulated, bool other)
       {
           auto deltaPos=qreal(oprop(angleDelta,OProp::pos,other));
           auto scrollLines=QApplication::wheelScrollLines();
           auto numStepsU = m_singleStep * scrollLines * deltaPos / 120;
           if (qAbs(accumulated)>std::numeric_limits<float>::epsilon()
               &&
               (numStepsU/accumulated)<0
               )
           {
               accumulated=0.0f;
           }
           accumulated+=numStepsU;
           auto numSteps=static_cast<int>(accumulated);
           accumulated-=numSteps;

           return numSteps;
       };

       scrollMain=evalOffset(m_wheelOffsetAccumulated,false);
       scrollOther=evalOffset(m_wheelOffsetAccumulatedOther,true);
   }

   scroll(-scrollMain);

   if (
           m_scrollWheelHorizontal
           &&
           isHorizontal()
           &&
           m_view->height()>=m_llist->height()
           &&
           m_view->width()<m_llist->width()
           )
   {
        scroll(-scrollOther);
   }
   else if (scrollOther!=0)
   {
       if (isHorizontal())
       {
           m_vbar->setValue(m_vbar->value()-scrollOther);
       }
       else
       {
           m_hbar->setValue(m_hbar->value()-scrollOther);
       }
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
        minPos=std::max(minPos,-listSize);
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
    const auto& idx=itemIdx();
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
    m_scrollValue=-oprop(m_llist,OProp::pos);

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
    const auto* widget=directChildWidgetAt(m_llist,pos);
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
    //! @note Looking for item in the center of view's end edge, so the items must be centered in the view!

    auto edge=oprop(m_view,OProp::size);
    if (edge!=0)
    {
        --edge;
    }
    QPoint viewLastPos;
    setOProp(viewLastPos,OProp::pos,oprop(m_view,OProp::size,true)/2,true);
    setOProp(viewLastPos,OProp::pos,edge);

    auto listLastViewportPoint=m_llist->mapFromParent(viewLastPos);

#ifdef UISE_DEAKTOP_FLYWEIGHTLISTVIEW_DEBUG
    qDebug() << "lastViewportItem() listLastViewportPoint "<<listLastViewportPoint;
#endif

    const auto* item=itemAtPos(listLastViewportPoint);
    if (item==nullptr)
    {
#ifdef UISE_DEAKTOP_FLYWEIGHTLISTVIEW_DEBUG
        qDebug() << "lastViewportItem() item not found";
#endif
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
    const auto& idx=itemIdx();
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

    auto& idx=itemIdx();
    idx.erase(item->id());
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::beginItemRangeChange() noexcept
{
    m_firstItem=firstItem();
    m_lastItem=lastItem();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::endItemRangeChange()
{
    const auto* first=m_firstItem;
    const auto* last=m_lastItem;
    m_firstItem=firstItem();
    m_lastItem=lastItem();

    if (m_firstItem!=first || m_lastItem!=last)
    {
        QString log("Item range changed: ");
        if (m_firstItem)
        {
            log+=QString("first item %1, ").arg(m_firstItem->id());
        }
        else
        {
            log+=QString("no first item, ");
        }
        if (m_lastItem)
        {
            log+=QString("last item %1").arg(m_lastItem->id());
        }
        else
        {
            log+=QString("no last item");
        }
        std::ignore=log;
#ifdef UISE_DEAKTOP_FLYWEIGHTLISTVIEW_DEBUG
        qDebug() << log;
#endif

        if (m_itemRangeChangedCb)
        {
            m_itemRangeChangedCb(m_firstItem,m_lastItem);
        }
    }
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::checkItemCount()
{
    if (!m_enableFlyweight)
    {
        return;
    }

    if (itemsCount()==0)
    {
        // don't request items if the list was not loaded yet
        return;
    }

    auto maxHidden=maxHiddenItemsBeyondEdge();
    auto minPrefetch=prefetchThreshold();
    auto prefetch=prefetchItemCount();

    int hiddenBefore=0;
    auto first=firstItem();
    auto firstVisible=firstViewportItem();
    if (first&&firstVisible)
    {
        auto from=m_llist->widgetSeqPos(first->widget());
        auto to=m_llist->widgetSeqPos(firstVisible->widget());
        hiddenBefore=to-from;
    }
#ifdef UISE_DEAKTOP_FLYWEIGHTLISTVIEW_DEBUG
    qDebug() << "hiddenBefore "<<hiddenBefore<<" threshold "<<minPrefetch << " prefetch " << prefetch << " maxHidden "<<maxHidden
             << " first->sortValue() "<<first->sortValue()
             << " m_minSortValue "<<m_minSortValue;
#endif
    if (hiddenBefore<minPrefetch && first->sortValue()>m_minSortValue)
    {
        if (m_requestItemsCb)
        {
            m_requestItemsCb(firstItem(),prefetch,Direction::HOME);
        }
    }
    else if (hiddenBefore>maxHidden)
    {
        removeExtraItemsFromBegin(hiddenBefore-maxHidden);
    }

    int hiddenAfter=0;
    auto last=lastItem();
    auto lastVisible=lastViewportItem();
    if (last&&lastVisible)
    {
        auto from=m_llist->widgetSeqPos(lastVisible->widget());
        auto to=m_llist->widgetSeqPos(last->widget());
        hiddenAfter=to-from;

#ifdef UISE_DEAKTOP_FLYWEIGHTLISTVIEW_DEBUG
    qDebug() << "Last from "<<lastVisible->sortValue()<<" to "<<last->sortValue();
#endif
    }
    else
    {
#ifdef UISE_DEAKTOP_FLYWEIGHTLISTVIEW_DEBUG
    qDebug() << "No last item or it is invisible";
#endif
    }
#ifdef UISE_DEAKTOP_FLYWEIGHTLISTVIEW_DEBUG
    qDebug() << "hiddenAfter "<<hiddenAfter<<" threshold "<<minPrefetch << " prefetch " << prefetch << " maxHidden "<<maxHidden
             << " last->sortValue() "<<last->sortValue()
             << " m_maxSortValue "<<m_maxSortValue;
#endif
    if (hiddenAfter<minPrefetch  && last->sortValue()<m_maxSortValue)
    {
        if (m_requestItemsCb)
        {
            m_requestItemsCb(lastItem(),prefetch,Direction::END);
        }
    }
    else if (hiddenAfter>maxHidden)
    {
        removeExtraItemsFromEnd(hiddenAfter-maxHidden);
    }
#ifdef UISE_DEAKTOP_FLYWEIGHTLISTVIEW_DEBUG
    qDebug() << " item count "<<itemsCount();
#endif
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::removeExtraItemsFromBegin(size_t count)
{
    if (count==0)
    {
        return;
    }

    beginUpdate();

    auto& order=itemOrder();
    for (auto it=order.begin();it!=order.end();)
    {
        if (count--==0)
        {
            break;
        }
#ifdef UISE_DEAKTOP_FLYWEIGHTLISTVIEW_DEBUG
        qDebug() << "Removed item "<<it->sortValue()<< " before viewport";
#endif
        clearWidget(it->widget());
        it=order.erase(it);
    }

    endUpdate();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::removeExtraItemsFromEnd(size_t count)
{
    if (count==0)
    {
        return;
    }

    beginUpdate();

    auto& order=itemOrder();
    for (auto it=order.rbegin(), nit=it;it!=order.rend(); it=nit)
    {
        if (count--==0)
        {
            break;
        }

        nit=std::next(it);
#ifdef UISE_DEAKTOP_FLYWEIGHTLISTVIEW_DEBUG
        qDebug() << "Removed item "<<it->sortValue()<< " after viewport";
#endif
        clearWidget(it->widget());
        nit = decltype(it){order.erase(std::next(it).base())};
    }

    endUpdate();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::updateScrollBars()
{
    m_vbar->blockSignals(true);
    m_hbar->blockSignals(true);

    switch (m_vbarPolicy)
    {
        case Qt::ScrollBarAlwaysOff:
            m_vbar->setVisible(false);
        break;

        case Qt::ScrollBarAlwaysOn:
            m_vbar->setVisible(true);
            m_vbar->setMaximum(0);
        break;

        case Qt::ScrollBarAsNeeded:
            m_vbar->setVisible(m_view->height()<m_llist->height());
        break;

        default:
        break;
    }
    if (m_view->height()<m_llist->height())
    {
        m_vbar->setMaximum(m_llist->height()-m_view->height());
        m_vbar->setValue(-m_llist->y());
    }
    else
    {
        m_vbar->setValue(0);
        m_vbar->setMaximum(0);
    }

    switch (m_hbarPolicy)
    {
        case Qt::ScrollBarAlwaysOff:
            m_hbar->setVisible(false);
        break;

        case Qt::ScrollBarAlwaysOn:
            m_hbar->setVisible(true);
        break;

        case Qt::ScrollBarAsNeeded:
            m_hbar->setVisible(m_view->width()<m_llist->width());
        break;

        default:
        break;
    }
    if (m_view->width()<m_llist->width())
    {
        m_hbar->setMaximum(m_llist->width()-m_view->width());
        m_hbar->setValue(-m_llist->x());
    }
    else
    {
        m_hbar->setValue(0);
        m_hbar->setMaximum(0);
    }

    m_vbar->blockSignals(false);
    m_hbar->blockSignals(false);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::onMainSbarChanged(int value)
{
    auto oldPos=oprop(m_llist,OProp::pos);
    auto diff=-value-oldPos;
    scroll(-diff);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView_p<ItemT>::onOtherSbarChanged(int value)
{
    auto pos=m_llist->pos();
    setOProp(pos,OProp::pos,-value,true);
    m_llist->move(pos);
}

//--------------------------------------------------------------------------

} // namespace detail

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_FLYWEIGHTLISTVIEW_P_IPP
