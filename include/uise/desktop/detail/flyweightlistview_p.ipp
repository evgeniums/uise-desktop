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

#include <uise/desktop/utils/datetime.hpp>
#include <uise/desktop/utils/directchildwidget.hpp>

#include <uise/desktop/detail/flyweightlistview_p.hpp>

//#define UISE_DESKTOP_FLYWEIGHTLISTVIEW_DEBUG

#ifdef _MSC_VER

#pragma warning(push)
#pragma warning(disable : 4267) // disable size_t to int warnings

#endif

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

namespace detail {

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
FlyweightListView_p<ItemT,OrderComparer,IdComparer>::FlyweightListView_p(
        FlyweightListView<ItemT,OrderComparer,IdComparer>* view,
        size_t prefetchItemWindowHint,
        OrderComparer orderComparer,
        IdComparer idComparer
    ) : m_obj(view),
        m_vbar(nullptr),
        m_hbar(nullptr),
        m_view(nullptr),
        m_prefetchItemWindow(prefetchItemWindowHint),
        m_prefetchItemWindowHint(prefetchItemWindowHint),
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
        m_scrollWheelHorizontal(true),
        m_items(
          boost::make_tuple(
            boost::make_tuple(OrderIdxFn{},orderComparer),
            boost::make_tuple(IdIdxFn{},std::move(idComparer))
          )
        ),
        m_orderComparer(orderComparer),
        m_prefetchScreenCount(FlyweightListView<ItemT>::PrefetchScreensCountHint),
        m_prefetchThresholdRatio(FlyweightListView<ItemT>::PrefetchThresholdRatio),
        m_enableJumpEdgeControl(true),
        m_jumpEdge(nullptr),
        m_jumpEdgeOffset(FlyweightListView<ItemT>::DefaultJumpEdgeXOffset,FlyweightListView<ItemT>::DefaultJumpEdgeYOffset),
        m_jumpEdgeInvisibleItemCount(FlyweightListView<ItemT>::DefaultJumpInvisibleItemCount),
        m_itemsAlignment(FlyweightListViewAlignment::Center)
{
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
FlyweightListView_p<ItemT,OrderComparer,IdComparer>::~FlyweightListView_p()
{
    resetCallbacks();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::resetCallbacks()
{
    m_removeItemCb=decltype(m_removeItemCb){};
    m_requestItemsCb=decltype(m_requestItemsCb){};
    m_viewportChangedCb=decltype(m_viewportChangedCb){};
    m_itemRangeChangedCb=decltype(m_itemRangeChangedCb){};
    m_homeRequestCb=decltype(m_homeRequestCb){};
    m_endRequestCb=decltype(m_endRequestCb){};
    m_insertItemCb=decltype(m_insertItemCb){};
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::setupUi()
{
    auto vlayout=Layout::vertical(m_obj);

    auto middleFrame=new QFrame(m_obj);
    middleFrame->setObjectName("uiseFlyweightListViewM");
    vlayout->addWidget(middleFrame,1);
    m_hbar=new QScrollBar(m_obj);
    m_hbar->setOrientation(Qt::Horizontal);
    m_hbar->setVisible(false);
    vlayout->addWidget(m_hbar);
    auto hlayout=Layout::horizontal(middleFrame);

    m_view=new QFrame(middleFrame);
    m_view->setObjectName("uiseFlyweightListViewV");
    hlayout->addWidget(m_view,1);
    QFrame* paddingFrame=new QFrame(middleFrame);
    paddingFrame->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
    hlayout->addWidget(paddingFrame);
    m_vbar=new QScrollBar(middleFrame);
    m_vbar->setVisible(false);
    hlayout->addWidget(m_vbar);

    m_view->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    m_view->setFocusPolicy(Qt::StrongFocus);

    m_llist=new LinkedListView(m_view);
    m_llist->setFocusProxy(m_view);

    updatePageStep();
    resizeList();

    m_qobjectHelper.setListResizeHandler([this](){onListContentResized();});

    if (m_llist->frameWidth()!=0)
    {
        auto err=QString("CSS border, margin and padding for LinkedListView(FlyweightListView) must be 0 (actual: %1)").arg(m_llist->frameWidth());
        qCritical()<<err;

        //! @todo Support other frame widths using contentsRect()

        Q_ASSERT_X(m_llist->frameWidth()==0,"FlyweightListView",err.toStdString().data());
    }

    keepCurrentConfiguration();

    updateScrollBarOrientation();
    QObject::connect(m_vbar,&QScrollBar::valueChanged,[this](int value){m_vScrollCb(value);});
    QObject::connect(m_hbar,&QScrollBar::valueChanged,[this](int value){m_hScrollCb(value);});

    m_jumpEdge=new JumpEdge(m_view);
    m_jumpEdge->setDirection(m_stick);
    m_jumpEdge->setOrientation(m_llist->orientation());
    updateJumpEdgeVisibility();
    QObject::connect(
        m_jumpEdge,
        &JumpEdge::clicked,
        m_view,
        [this]()
        {
            onJumpEdgeClicked();
        }
    );

    QTimer::singleShot(0,m_obj,[this](){onResized();});
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::beginUpdate()
{
    m_ignoreUpdates=true;
    beginItemRangeChange();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::endUpdate()
{
#if 0
    qDebug() << printCurrentDateTime() << ": FlyweightListView_p::endUpdate()  " << m_obj;
#endif
    resizeList();
    m_ignoreUpdates=false;
    endItemRangeChange();
    viewportUpdated();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::onListContentResized()
{
    m_resizeListTimer.shot(0,
        [this]()
        {
#if 0
            qDebug() << printCurrentDateTime() << ": FlyweightListView_p::onListContentResized() shot 0 " << m_obj;
#endif
            resizeList();
            viewportUpdated();
        }
    );
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::configureWidget(const ItemT* item)
{
    auto mutableItem=const_cast<ItemT*>(item);
    auto widget=mutableItem->widget();

    PointerHolder::keepProperty(item,widget,ItemT::Property);
    QObject::disconnect(widget,nullptr,&m_qobjectHelper,nullptr);

    QObject::connect(widget,
                     &QObject::destroyed,
                     &m_qobjectHelper,
                     [this,id=item->id()]()
                     {
                        auto& idx=itemIdx();
                        idx.erase(id);

                        auto& order=itemOrder();
                        auto b=order.begin();
                        if (b!=order.end())
                        {
                            m_firstItem=&(*b);
                        }
                        else
                        {
                            m_firstItem=nullptr;
                        }
                        auto e=order.rbegin();
                        if (e!=order.rend())
                        {
                            m_lastItem=&(*e);
                        }
                        else
                        {
                            m_lastItem=nullptr;
                        }

                        m_resizeListTimer.shot(0,
                                               [this,ignoreUpdates=m_ignoreUpdates]()
                                               {
                                                   resizeList();
                                                   if (!ignoreUpdates)
                                                   {
                                                       endUpdate();
                                                   }
                                               }
                                            );
                     }
                );

    widget->removeEventFilter(&m_qobjectHelper);
    widget->installEventFilter(&m_qobjectHelper);

    if (m_insertItemCb)
    {
        m_insertItemCb(mutableItem);
    }
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView_p<ItemT,OrderComparer,IdComparer>::prefetchItemWindow() noexcept
{
    return autoPrefetchCount();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView_p<ItemT,OrderComparer,IdComparer>::autoPrefetchCount() noexcept
{
    m_prefetchItemWindow=std::max(m_prefetchItemWindow,static_cast<size_t>(qRound(visibleCount()*m_prefetchScreenCount)));
    return m_prefetchItemWindow;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView_p<ItemT,OrderComparer,IdComparer>::maxHiddenItemsBeyondEdge() noexcept
{
    return prefetchItemWindow()*m_prefetchScreenCount;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView_p<ItemT,OrderComparer,IdComparer>::prefetchThreshold() noexcept
{
    return prefetchItemWindow()*m_prefetchThresholdRatio;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView_p<ItemT,OrderComparer,IdComparer>::itemsCount() const noexcept
{
    return m_items.template get<1>().size();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView_p<ItemT,OrderComparer,IdComparer>::visibleCount() const noexcept
{
    size_t count=0;

    auto first=firstViewportItem();
    auto last=lastViewportItem();

#ifdef UISE_DESKTOP_FLYWEIGHTLISTVIEW_DEBUG
    qDebug() << printCurrentDateTime() << ": visibleCount() first="<<first<<" last="<<last;
#endif

    if (first&&last)
    {
        auto firstPos=m_llist->widgetSeqPos(first->widget());
        auto lastPos=m_llist->widgetSeqPos(last->widget());
        count=lastPos-firstPos+1;
#ifdef UISE_DESKTOP_FLYWEIGHTLISTVIEW_DEBUG
        qDebug() << printCurrentDateTime() << ": visibleCount() firstPos="<<firstPos<<" lastPos="<<lastPos<<" count=" << count;
#endif
    }

    return count;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
bool FlyweightListView_p<ItemT,OrderComparer,IdComparer>::hasItem(const typename ItemT::IdType& id) const noexcept
{
    const auto& idx=itemIdx();
    auto it=idx.find(id);
    return it!=idx.end();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
const ItemT* FlyweightListView_p<ItemT,OrderComparer,IdComparer>::item(const typename ItemT::IdType &id) const noexcept
{
    const auto& idx=itemIdx();
    auto it=idx.find(id);
    return (it!=idx.end())?&(*it):nullptr;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
const auto& FlyweightListView_p<ItemT,OrderComparer,IdComparer>::itemOrder() const noexcept
{
    return m_items.template get<0>();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
auto& FlyweightListView_p<ItemT,OrderComparer,IdComparer>::itemOrder() noexcept
{
    return m_items.template get<0>();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
const auto& FlyweightListView_p<ItemT,OrderComparer,IdComparer>::itemIdx() const noexcept
{
    return m_items.template get<1>();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
auto& FlyweightListView_p<ItemT,OrderComparer,IdComparer>::itemIdx() noexcept
{
    return m_items.template get<1>();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
const ItemT* FlyweightListView_p<ItemT,OrderComparer,IdComparer>::firstItem() const noexcept
{
    const auto& order=itemOrder();
    auto it=order.begin();
    return (it!=order.end())?&(*it):nullptr;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
const ItemT* FlyweightListView_p<ItemT,OrderComparer,IdComparer>::lastItem() const noexcept
{
    const auto& order=itemOrder();
    auto it=order.rbegin();
    return it!=order.rend()?&(*it):nullptr;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
bool FlyweightListView_p<ItemT,OrderComparer,IdComparer>::isHorizontal() const noexcept
{
    return m_llist->orientation()==Qt::Horizontal;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::setFlyweightEnabled(bool enable) noexcept
{
    m_enableFlyweight=enable;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
bool FlyweightListView_p<ItemT,OrderComparer,IdComparer>::isFlyweightEnabled() const noexcept
{
    return m_enableFlyweight;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
QPoint FlyweightListView_p<ItemT,OrderComparer,IdComparer>::viewportBegin() const
{
    QPoint pos;

    switch (m_itemsAlignment)
    {
        case(FlyweightListViewAlignment::Center):
        {
            setOProp(pos,OProp::pos,oprop(m_llist,OProp::size,true)/2,true);
        }
        break;

        case(FlyweightListViewAlignment::Begin):
        {
            setOProp(pos,OProp::pos,0,true);
        }
        break;

        case(FlyweightListViewAlignment::End):
        {
            setOProp(pos,OProp::pos,oprop(m_llist,OProp::size,true) - 1,true);
        }
        break;
    }

    setOProp(pos,OProp::pos,-oprop(m_llist,OProp::pos));
    return pos;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
QPoint FlyweightListView_p<ItemT,OrderComparer,IdComparer>::listEndInViewport() const
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
template <typename ItemT, typename OrderComparer, typename IdComparer>
bool FlyweightListView_p<ItemT,OrderComparer,IdComparer>::isAtBegin() const
{
    return oprop(m_llist->pos(),OProp::pos)==0;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
bool FlyweightListView_p<ItemT,OrderComparer,IdComparer>::isAtEnd() const
{
    return oprop(listEndInViewport(),OProp::pos)<=(oprop(m_view,OProp::size)-1);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
int FlyweightListView_p<ItemT,OrderComparer,IdComparer>::endItemEdge() const
{
    return oprop(m_llist,OProp::edge);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::updateStickingPositions()
{
    auto begin=oprop(m_llist->pos(),OProp::pos);
    auto end=oprop(listEndInViewport(),OProp::pos);
    auto viewPortSize=oprop(m_view,OProp::size);

#if 0
    qDebug() << printCurrentDateTime() << ": FlyweightListView_p::updateStickingPositions() " << m_obj
                       << " begin="<<begin << " end="<<end << " viewPortSize="<<viewPortSize;
#endif

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
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::onResized()
{
    auto margins=m_obj->contentsMargins();
    m_hbar->resize(m_obj->width()-m_vbar->width()-margins.left()-margins.right(),m_hbar->height());
    updateJumpEdgePosition();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::onViewportResized(QResizeEvent *event)
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
        if (newPos>0
            &&
            !(m_stick==Direction::END && listSize<viewSize)
            )
        {
            newPos=0;
        }
        setOProp(movePos,OProp::pos,newPos);
        moveList=true;
    }

#if 0

    qDebug() << printCurrentDateTime() << ": FlyweightListView_p::onViewportResized() " << m_obj << " old m_viewSize=" << m_viewSize << " movePos="<<movePos << " moveEnd="<<moveEnd<<" moveBegin="<<moveBegin << " moveList="<<moveList;

#endif

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
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::compensateSizeChange()
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
            if (m_orderComparer(m_firstViewportSortValue,it->sortValue()) || itemOrdersEqual(m_firstViewportSortValue,it->sortValue()))
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
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::updateScrollBarOrientation()
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
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::setOrientation(Qt::Orientation orientation)
{
    beginUpdate();
    clear();
    m_llist->setOrientation(orientation);
    updateScrollBarOrientation();
    updatePageStep();
    m_jumpEdge->setOrientation(orientation);
    endUpdate();    
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::updatePageStep()
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
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::viewportUpdated()
{
#if 0
    qDebug() << printCurrentDateTime() << ": FlyweightListView_p::viewportUpdated()  " << m_obj << " m_ignoreUpdates="<<m_ignoreUpdates;
#endif
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
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::informViewportUpdated()
{
#if 0
    qDebug() << printCurrentDateTime() << ": FlyweightListView_p::informViewportUpdated()  " << m_obj;
#endif

    auto l_firstViewportItemID=m_firstViewportItemID;
    auto l_firstViewportSortValue=m_firstViewportSortValue;
    auto l_lastViewportItemID=m_lastViewportItemID;
    auto l_lastViewportSortValue=m_lastViewportSortValue;
    auto l_cleared=m_cleared;
    m_cleared=false;

    keepCurrentConfiguration();

    m_scrollBarsTimer.shot(10,[this](){updateScrollBars();});

    //! @todo Use ID comparer for comparing od ids
    if (
            l_cleared ||
            l_firstViewportItemID!=m_firstViewportItemID ||
            !itemOrdersEqual(l_firstViewportSortValue,m_firstViewportSortValue) ||
            l_lastViewportItemID!=m_lastViewportItemID ||
            !itemOrdersEqual(l_lastViewportSortValue,m_lastViewportSortValue)
        )
    {
#if 0
        qDebug() << printCurrentDateTime() << ": FlyweightListView_p::informViewportUpdated()  " << m_obj << " inform";
#endif
        m_informViewportUpdateTimer.shot(0,
            [this]()
            {
                updateJumpEdgeVisibility();
                if (m_viewportChangedCb)
                {
                    m_viewportChangedCb(item(m_firstViewportItemID),item(m_lastViewportItemID));
                }
            }
        );
    }
    else
    {
#if 0
        qDebug() << printCurrentDateTime() << ": FlyweightListView_p::informViewportUpdated()  " << m_obj << " skip";
#endif
    }
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
QWidget* FlyweightListView_p<ItemT,OrderComparer,IdComparer>::insertItemToContainer(const ItemT& item, bool findAfterWidget)
{
    auto& idx=itemIdx();
    auto result=idx.insert(item);
    if (!result.second)
    {
        if (result.first->widget()!=item.widget())
        {
            removeItem(result.first->id());
            result=idx.insert(item);
            Q_ASSERT(result.second);
            configureWidget(&(*result.first));
        }
        else
        {
            m_llist->takeWidget(item.widget());
            Q_ASSERT(idx.replace(result.first,item));
            result.first=idx.find(item.id());
        }
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
            --it;
            afterWidget=it->widget();
        }
    }

    return afterWidget;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::insertItem(const ItemT& item)
{
    m_llist->insertWidgetAfter(item.widget(),insertItemToContainer(item));
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::reorderItem(const ItemT& item)
{
    auto first=firstItem();
    auto last=lastItem();

    if (last!=nullptr && m_orderComparer(last->sortValue(),item.sortValue()))
    {
        if (m_stick==Direction::END && isAtEnd())
        {
            m_llist->insertWidgetAfter(item.widget(),insertItemToContainer(item));
        }
        else
        {
            removeItem(item.id());
        }

        return;
    }
    else if (first!=nullptr && m_orderComparer(item.sortValue(),first->sortValue()))
    {
        if (m_stick==Direction::HOME && isAtBegin())
        {
            insertItemToContainer(item);
            m_llist->insertWidgetAfter(item.widget(),nullptr);
        }
        else
        {
            removeItem(item.id());
        }

        return;
    }

    m_llist->insertWidgetAfter(item.widget(),insertItemToContainer(item));
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::insertContinuousItems(const std::vector<ItemT>& items)
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
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::resizeList()
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
#if 0
        qDebug() << printCurrentDateTime() << ": FlyweightListView_p::resizeList()  " << m_obj << " set size " << listSize;
#endif
        m_llist->resize(listSize);
        compensateSizeChange();
    }
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::clear()
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
    m_firstItem=nullptr;
    m_lastItem=nullptr;
    m_firstWidgetPos=0;
    m_prefetchItemWindow=m_prefetchItemWindowHint;

    m_cleared=true;
    m_jumpEdge->setVisible(false);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::scroll(int delta)
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
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::wheelEvent(QWheelEvent *event)
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

   if (isVertical() && !m_scrollWheelHorizontal)
   {
       scrollOther=0;
   }

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
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::scrollTo(const std::function<int (int, int, int)> &cb)
{
#if 0
    qDebug() << printCurrentDateTime() << ": FlyweightListView_p::scrollTo() " << m_obj;
#endif
    auto viewportSize=oprop(m_view,OProp::size);
    auto listSize=oprop(m_llist,OProp::size);

    int minPos=0;
    int maxPos=0;
    if (listSize>viewportSize)
    {
        minPos=viewportSize-listSize;
        minPos=std::max(minPos,-listSize);
    }
    else if (m_stick==Direction::END)
    {
        minPos=viewportSize-listSize;
        maxPos=minPos;
    }

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
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::scrollToEdge(Direction direction)
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
template <typename ItemT, typename OrderComparer, typename IdComparer>
bool FlyweightListView_p<ItemT,OrderComparer,IdComparer>::scrollToItem(const typename ItemT::IdType &id, int offset)
{
#if 0
    qDebug() << printCurrentDateTime() << ": Scroll to item "<<id<<" offset "<<offset;
#endif
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
            auto widgetViewPos=oprop(m_llist->mapToParent(widgetListPos),OProp::pos);
            auto widgetBegin=oldPos-widgetViewPos;
            int newPos=widgetBegin-offset;
            newPos=qBound(minPos,newPos,maxPos);
            return newPos;
        };

        return oldPos;
    };

    scrollTo(cb);
    return true;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::keepCurrentConfiguration()
{
    m_listSize=m_llist->size();
    m_viewSize=m_view->size();

#if 0
    qDebug() << printCurrentDateTime() << ": FlyweightListView_p::keepCurrentConfiguration()  " << m_obj << " m_viewSize=" << m_viewSize<< " m_listSize="<<m_listSize;
#endif

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
#if 0
        qDebug()  <<  printCurrentDateTime() << ": FlyweightListView_p::keepCurrentConfiguration()  " << m_obj << " first view item not found";
#endif
    }

    keep(item,m_firstViewportItemID,m_firstViewportSortValue);
    item=lastViewportItem();
    keep(item,m_lastViewportItemID,m_lastViewportSortValue);

    m_atBegin=isAtBegin();
    m_atEnd=isAtEnd();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
const ItemT* FlyweightListView_p<ItemT,OrderComparer,IdComparer>::itemAtPos(const QPoint &pos) const
{
#if 0
    qDebug() << printCurrentDateTime() << ": itemAtPos() "<<pos << " m_llist->size() " << m_llist->size();
#endif
    const auto* widget=directChildWidgetAt(m_llist,pos);
    return PointerHolder::getProperty<const ItemT*>(widget,ItemT::Property);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
const ItemT* FlyweightListView_p<ItemT,OrderComparer,IdComparer>::firstViewportItem() const
{
    return itemAtPos(viewportBegin());
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
const ItemT* FlyweightListView_p<ItemT,OrderComparer,IdComparer>::lastViewportItem() const
{
    auto edge=oprop(m_view,OProp::size);
    if (edge!=0)
    {
        --edge;
    }
    QPoint viewLastPos;

    switch (m_itemsAlignment)
    {
        case(FlyweightListViewAlignment::Center):
        {
            setOProp(viewLastPos,OProp::pos,oprop(m_view,OProp::size,true)/2,true);
        }
        break;

        case(FlyweightListViewAlignment::Begin):
        {
            setOProp(viewLastPos,OProp::pos,0,true);
        }
        break;

        case(FlyweightListViewAlignment::End):
        {
            setOProp(viewLastPos,OProp::pos,oprop(m_view,OProp::size,true) - 1,true);
        }
        break;
    }

    setOProp(viewLastPos,OProp::pos,edge);

    auto listLastViewportPoint=m_llist->mapFromParent(viewLastPos);

#ifdef UISE_DESKTOP_FLYWEIGHTLISTVIEW_DEBUG
    qDebug << printCurrentDateTime() << ": lastViewportItem() listLastViewportPoint "<<listLastViewportPoint;
#endif

    const auto* item=itemAtPos(listLastViewportPoint);
    if (item==nullptr)
    {
#ifdef UISE_DESKTOP_FLYWEIGHTLISTVIEW_DEBUG
        qDebug() << printCurrentDateTime() << ": lastViewportItem() item not found";
#endif
        item=lastItem();
    }
    return item;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::clearWidget(typename ItemT::WidgetType* widget)
{
    if (!widget)
    {
        return;
    }

    if (m_removeItemCb)
    {
        m_removeItemCb(widget);
    }

    PointerHolder::clearProperty(widget,ItemT::Property);
    m_llist->takeWidget(widget);
    widget->removeEventFilter(&m_qobjectHelper);
    QObject::disconnect(widget,nullptr,&m_qobjectHelper,nullptr);

    ItemT::dropWidget(widget);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::removeItem(const typename ItemT::IdType &id)
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
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::removeItem(ItemT* item)
{
    clearWidget(item->widget());

    auto& idx=itemIdx();
    idx.erase(item->id());
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::beginItemRangeChange() noexcept
{
    m_firstItem=firstItem();
    m_lastItem=lastItem();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::endItemRangeChange()
{
    const auto* first=m_firstItem;
    const auto* last=m_lastItem;
    m_firstItem=firstItem();
    m_lastItem=lastItem();

    //! @todo use item comparer
    if (m_firstItem!=first || m_lastItem!=last)
    {
#ifdef UISE_DESKTOP_FLYWEIGHTLISTVIEW_DEBUG
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
        qDebug() << log;
#endif

        if (m_itemRangeChangedCb)
        {
            m_itemRangeChangedCb(m_firstItem,m_lastItem);
        }
    }
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::checkItemCount()
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
    auto prefetch=prefetchItemCountEffective();

    int hiddenBefore=0;
    auto first=firstItem();
    auto firstVisible=firstViewportItem();
    if (first&&firstVisible)
    {
        auto from=m_llist->widgetSeqPos(first->widget());
        auto to=m_llist->widgetSeqPos(firstVisible->widget());
        hiddenBefore=to-from;
    }
#ifdef UISE_DESKTOP_FLYWEIGHTLISTVIEW_DEBUG
    qDebug() << printCurrentDateTime() << ": FlyweightListView_p::checkItemCount hiddenBefore "<<hiddenBefore<<" threshold "<<minPrefetch << " prefetch " << prefetch << " maxHidden "<<maxHidden
             << " first->sortValue() "<<first->sortValue()
             << " m_minSortValue "<<m_minSortValue;
#endif
    if (hiddenBefore<minPrefetch && first && m_orderComparer(m_minSortValue,first->sortValue()))
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

#ifdef UISE_DESKTOP_FLYWEIGHTLISTVIEW_DEBUG
    qDebug() << printCurrentDateTime() << ": Last from "<<lastVisible->sortValue()<<" to "<<last->sortValue();
#endif
    }
    else
    {
#ifdef UISE_DESKTOP_FLYWEIGHTLISTVIEW_DEBUG
    qDebug() << printCurrentDateTime() << ": No last item or it is invisible";
#endif
    }
#ifdef UISE_DESKTOP_FLYWEIGHTLISTVIEW_DEBUG
    qDebug() << printCurrentDateTime() << ": hiddenAfter "<<hiddenAfter<<" threshold "<<minPrefetch << " prefetch " << prefetch << " maxHidden "<<maxHidden
             << " last->sortValue() "<<last->sortValue()
             << " m_maxSortValue "<<m_maxSortValue;
#endif
    if (hiddenAfter<minPrefetch  && last && m_orderComparer(last->sortValue(),m_maxSortValue))
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
#ifdef UISE_DESKTOP_FLYWEIGHTLISTVIEW_DEBUG
    qDebug() << printCurrentDateTime() << ":  item count "<<itemsCount();
#endif
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::removeExtraItemsFromBegin(size_t count)
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
#ifdef UISE_DESKTOP_FLYWEIGHTLISTVIEW_DEBUG
        qDebug() << printCurrentDateTime() << ": Removed item "<<it->sortValue()<< " before viewport";
#endif
        clearWidget(it->widget());
        it=order.erase(it);
    }

    endUpdate();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::removeExtraItemsFromEnd(size_t count)
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
#ifdef UISE_DESKTOP_FLYWEIGHTLISTVIEW_DEBUG
        qDebug() << printCurrentDateTime() << ": Removed item "<<it->sortValue()<< " after viewport";
#endif
        clearWidget(it->widget());
        nit = decltype(it){order.erase(std::next(it).base())};
    }

    endUpdate();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::updateScrollBars()
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
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::onMainSbarChanged(int value)
{
    auto oldPos=oprop(m_llist,OProp::pos);
    auto diff=-value-oldPos;
    scroll(-diff);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::onOtherSbarChanged(int value)
{
    auto pos=m_llist->pos();
    setOProp(pos,OProp::pos,-value,true);
    m_llist->move(pos);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::setPrefetchScreensCount(double value)
{
    m_prefetchScreenCount=value;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
double FlyweightListView_p<ItemT,OrderComparer,IdComparer>::prefetchScreensCount() const noexcept
{
    return m_prefetchScreenCount;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::setPrefetchThresholdRatio(double value)
{
    m_prefetchThresholdRatio=value;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
double FlyweightListView_p<ItemT,OrderComparer,IdComparer>::prefetchThresholdRatio() const noexcept
{
    return m_prefetchThresholdRatio;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::setPrefetchItemCount(size_t value)
{
    m_prefetchItemCount=value;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::resetPrefetchItemCount()
{
    m_prefetchItemCount.reset();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView_p<ItemT,OrderComparer,IdComparer>::prefetchItemCount() const noexcept
{
    return m_prefetchItemCount.value_or(0);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView_p<ItemT,OrderComparer,IdComparer>::prefetchItemCountAuto() noexcept
{
    return prefetchItemWindow();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView_p<ItemT,OrderComparer,IdComparer>::prefetchItemCountEffective() noexcept
{
    return m_prefetchItemCount.value_or(prefetchItemCountAuto());
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::updateJumpEdgeVisibility()
{
    if (!m_enableJumpEdgeControl)
    {
        m_jumpEdge->setVisible(false);
        return;
    }

    const auto& order=itemOrder();
    size_t invisibleCount=0;
    bool showControl=false;

    if (m_stick==Direction::HOME)
    {
        auto it=order.find(m_firstViewportSortValue);
        if (it!=order.end())
        {
            for (auto it1=order.begin();it1!=it;++it1)
            {
                invisibleCount++;
                if (invisibleCount==m_jumpEdgeInvisibleItemCount)
                {
                    showControl=true;
                    break;
                }
            }
        }
    }
    else
    {
        for (auto it=order.find(m_lastViewportSortValue);it!=order.end();++it)
        {
            invisibleCount++;
            if (invisibleCount==m_jumpEdgeInvisibleItemCount)
            {
                showControl=true;
                break;
            }
        }
#if 0
        qDebug() << "FlyweightListView_p updateJumpEdgeVisibility "
                           << " invisibleCount="<<invisibleCount
                           << " m_jumpEdgeInvisibleItemCount="<<m_jumpEdgeInvisibleItemCount;
#endif
    }

    m_jumpEdge->setVisible(showControl);
    updateJumpEdgePosition();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::updateJumpEdgePosition()
{
    auto r=m_view->rect();
    auto jeSize=m_jumpEdge->size();
    QPoint jePos{r.right()-jeSize.width()-m_jumpEdgeOffset.width(),r.bottom()-jeSize.height()-m_jumpEdgeOffset.height()};

    m_jumpEdge->move(jePos);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::setJumpEdgeControlEnabled(bool enable)
{
    m_enableJumpEdgeControl=enable;
    updateJumpEdgeVisibility();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
bool FlyweightListView_p<ItemT,OrderComparer,IdComparer>::isJumpEdgeControlEnabled() const
{
    return m_enableJumpEdgeControl;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::setJumpEdgeInvisibleItemCount(size_t value)
{
    m_jumpEdgeInvisibleItemCount=value;
    updateJumpEdgeVisibility();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView_p<ItemT,OrderComparer,IdComparer>::jumpEdgeInvisibleItemCount() const
{
    return m_jumpEdgeInvisibleItemCount;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::onJumpEdgeClicked()
{
    auto direction=Direction::HOME;
    if (m_jumpEdge->iconDirection()==JumpEdge::IconDirection::Down
        ||
        m_jumpEdge->iconDirection()==JumpEdge::IconDirection::Right
        )
    {
        direction=Direction::END;
    }
    jumpToEdge(direction);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::jumpToEdge(Direction direction, bool forceLongfJump, Qt::KeyboardModifiers modifiers)
{
    if (direction==Direction::END)
    {
        if (!m_enableFlyweight)
        {
            scrollToEdge(Direction::END);
        }
        else if (m_lastItem!=nullptr && itemOrdersEqual(m_lastItem->sortValue(),m_maxSortValue))
        {
            scrollToEdge(Direction::END);
        }
        else if (m_endRequestCb)
        {
            m_endRequestCb(forceLongfJump,modifiers);
        }
    }
    else
    {
        if (!m_enableFlyweight)
        {
            scrollToEdge(Direction::HOME);
        }
        else if (m_firstItem!=nullptr && itemOrdersEqual(m_firstItem->sortValue(),m_minSortValue))
        {
            scrollToEdge(Direction::HOME);
        }
        else if (m_homeRequestCb)
        {
            m_homeRequestCb(forceLongfJump,modifiers);
        }
    }
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
template <typename T>
bool FlyweightListView_p<ItemT,OrderComparer,IdComparer>::itemOrdersEqual(const T& l, const T& r) const
{
    return !m_orderComparer(l,r) && !m_orderComparer(r,l);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
bool FlyweightListView_p<ItemT,OrderComparer,IdComparer>::itemOrdersEqual(const ItemT* l, const ItemT* r) const
{
    if (l==nullptr || r==nullptr)
    {
        return false;
    }

    return itemOrdersEqual(l->sortValue(),r->sortValue());
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
bool FlyweightListView_p<ItemT,OrderComparer,IdComparer>::eachItem(typename ViewType::EachItemHandler handler)
{
    auto& order=itemOrder();
    for (auto&& it : order)
    {
        auto ok=handler(&it);
        if (!ok)
        {
            return false;
        }
    }
    return true;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
bool FlyweightListView_p<ItemT,OrderComparer,IdComparer>::rEachItem(typename ViewType::EachItemHandler handler)
{
    auto& order=itemOrder();
    for (auto it=order.rbegin(); it!=order.rend(); ++it)
    {
        auto ok=handler(&(*it));
        if (!ok)
        {
            return false;
        }
    }
    return true;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::setItemsAlignment(FlyweightListViewAlignment value) noexcept
{
    m_itemsAlignment=value;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
FlyweightListViewAlignment FlyweightListView_p<ItemT,OrderComparer,IdComparer>::itemsAlignment() const noexcept
{
    return m_itemsAlignment;
}

//--------------------------------------------------------------------------

} // namespace detail

//--------------------------------------------------------------------------

#ifdef _MSC_VER

#pragma warning(pop) // size_t to int warnings

#endif

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_FLYWEIGHTLISTVIEW_P_IPP
