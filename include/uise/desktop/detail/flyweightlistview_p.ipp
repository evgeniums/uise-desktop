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
#include <cstddef>

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
        m_vbarHolder(nullptr),
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
        m_firstItem(nullptr),
        m_lastItem(nullptr),
        m_singleStep(10),
        m_pageStep(FlyweightListView<ItemT>::DefaultPageStep),
        m_minPageStep(FlyweightListView<ItemT>::DefaultPageStep),
        m_wheelOffsetAccumulated(0.0f),
        m_wheelOffsetAccumulatedOther(0.0f),
        m_ignoreUpdates(false),
        m_cleared(false),
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
        m_maxHiddenRatio(FlyweightListView<ItemT>::MaxHiddenRatio),
        m_enableJumpEdgeControl(true),
        m_jumpEdge(nullptr),
        m_jumpEdgeOffset(FlyweightListView<ItemT>::DefaultJumpEdgeXOffset,FlyweightListView<ItemT>::DefaultJumpEdgeYOffset),
        m_jumpEdgeInvisibleItemCount(FlyweightListView<ItemT>::DefaultJumpInvisibleItemCount),
        m_itemsAlignment(FlyweightListViewAlignment::Center),
        m_firstShowDone(false)
{
    m_currentBatchCount=0;    
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
FlyweightListView_p<ItemT,OrderComparer,IdComparer>::~FlyweightListView_p()
{
    resetCallbacks();
    clear();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::resetCallbacks()
{
    m_removeItemCb=decltype(m_removeItemCb){};
    m_requestItemsCb=decltype(m_requestItemsCb){};
    m_viewportChangedCb=decltype(m_viewportChangedCb){};
    m_itemRangeChangedCb=decltype(m_itemRangeChangedCb){};
    m_userScrolledCb=decltype(m_userScrolledCb){};
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
    m_vbarHolder=new VerticalScrollBar(middleFrame);
    m_vbar=m_vbarHolder->bar();
    m_vbarHolder->setVisible(false);
    hlayout->addWidget(m_vbarHolder);

    m_view->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    m_view->setFocusPolicy(Qt::StrongFocus);

    m_llist=new LinkedListView(m_view);
    m_llist->setFocusProxy(m_view);

    // REVERTED 2026-08-26 (see todo-chat-scroll-drift-on-history-prefetch.md "Round 4: found and
    // reverted"): this connection was added believing it was safe because the work it triggers
    // (m_informViewportUpdateTimer/m_checkItemCountTimer) is deferred -- but viewportUpdated()'s
    // FIRST action, keepCurrentConfiguration(), is NOT deferred, and it overwrites m_atEnd/
    // m_atBegin/m_firstWidgetPos synchronously. resizeList() relies on those staying exactly as
    // they were *before* its own m_llist->resize() call, so compensateSizeChange() (called right
    // after) can correctly decide "was the view at the edge before this specific change." Since
    // resize() synchronously emits resized() before returning, this connection refreshed that
    // state in between resizeList()'s own resize() and compensateSizeChange() calls -- so
    // compensateSizeChange() saw the *post*-growth, transiently-not-at-end geometry (the
    // compensating move hadn't happened yet) instead of the correct *pre*-growth snapshot, took
    // the anchor-preserving branch instead of the true-edge snap, and left the list permanently
    // short of the real end by however much the triggering resize grew it. Confirmed via
    // UISE_FWLV_CHECK=1 trace: m_atEnd read 1 going into resizeList(), then flipped to 0 by a
    // keepCurrentConfiguration() call sandwiched between the resize and the compensate, which
    // then took the wrong branch. The two other m_llist->resize() call sites do not need this
    // wiring: onViewportResized() already calls viewportUpdated() itself right after its own
    // resize, and clear()'s resize runs under blockSignals(true) so this would not have fired
    // there anyway. No remaining call site benefits from this connection; only resizeList() was
    // harmed by it.

    updatePageStep();
    resizeList("setupUi");

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

    updateListAlignment();
    QTimer::singleShot(0,m_obj,[this](){onResized();});
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::onFirstShow()
{
    if (m_firstShowDone)
    {
        return;
    }
    m_firstShowDone=true;

    // setupUi()'s own QTimer::singleShot(0,...) onResized() call above exists precisely because
    // m_obj can still have Qt's pre-layout default size when it is constructed, and that call
    // is what eventually corrects it -- one event loop turn later. A host that builds and shows
    // its whole widget tree in one synchronous pass (e.g. a window opening its initial content
    // from its own constructor, before its own show()) paints that one deferred turn as a
    // visible first-frame flash: the list area empty, then populated. Forcing m_obj's own
    // layout to activate synchronously right on first show -- instead of waiting for the
    // deferred call -- gives middleFrame/m_hbar/m_view their real geometry immediately; m_view
    // resizing in turn fires the QResizeEvent that the eventFilter() above forwards to
    // onViewportResized(), which is what actually populates/positions the list content. This is
    // additive, not a replacement: the deferred call above still runs normally and stays a
    // no-op once geometry has already settled here.
    if (m_obj->layout()!=nullptr)
    {
        m_obj->layout()->invalidate();
        m_obj->layout()->activate();
    }
    onResized();
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
    resizeList("endUpdate");
    m_ignoreUpdates=false;
    endItemRangeChange();
    viewportUpdated();

    checkInvariants("endUpdate");
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::onListContentResized()
{
    // Shares m_resizeListTimer with configureWidget()'s widget-destroyed handler below --
    // both must do the same superset of work, since SingleShotTimer::shot() keeps only the
    // last-posted handler (see that call site's comment).
    m_resizeListTimer.shot(0,
        [this]()
        {
#if 0
            qDebug() << printCurrentDateTime() << ": FlyweightListView_p::onListContentResized() shot 0 " << m_obj;
#endif
            resizeList("onListContentResized");
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

                        // Same superset as onListContentResized()'s handler on this shared
                        // timer -- SingleShotTimer::shot() keeps only the LAST-posted handler
                        // (see its own doc comment), so whichever of the two wins the race must
                        // still do everything the other one would have done. This used to
                        // instead conditionally call endUpdate() based on m_ignoreUpdates
                        // *captured at subscribe time* (i.e. when the item was originally
                        // inserted, not when it was destroyed) -- stale by construction, and if
                        // it won the race against onListContentResized()'s handler,
                        // viewportUpdated() was skipped entirely: no checkItemCount(), no
                        // prefetch, no scrollbar/jump-edge refresh, so the view could sit short
                        // of items and never refill (the "gaps" variant of
                        // todo-chat-messages-missing-after-insert.md). viewportUpdated() itself
                        // already no-ops when m_ignoreUpdates is live-true (its own top-of-body
                        // check), so there is no need to re-check it here.
                        m_resizeListTimer.shot(0,
                                               [this]()
                                               {
                                                   resizeList("widget-destroyed");
                                                   viewportUpdated();
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
    return prefetchItemWindow()*m_maxHiddenRatio;
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
        // Mirrors checkItemCount()'s own hiddenBefore/hiddenAfter underflow guard -- see its
        // comment. firstViewportItem()/lastViewportItem() are independent hit-tests (one has a
        // lastItem() fallback the other doesn't) that can disagree in direction if a widget is
        // unlinked/stale, and widgetSeqPos() returns 0 for a widget it doesn't recognise --
        // either way an unsigned lastPos-firstPos can wrap to ~2^64. That feeds straight into
        // the monotonically-non-decreasing m_prefetchItemWindow (autoPrefetchCount() below),
        // which only clear() ever resets -- one bad hit-test would otherwise poison it for the
        // rest of the view's life, silently disabling eviction and triggering endless prefetch.
        auto diff=static_cast<std::ptrdiff_t>(lastPos)-static_cast<std::ptrdiff_t>(firstPos);
        if (diff<0)
        {
            if (fwlvDebugEnabled())
            {
                std::cerr << "CHAT-FWLV-DEBUG: visibleCount() underflow guarded: firstPos="
                           << firstPos << " lastPos=" << lastPos << std::endl;
            }
            diff=-1;
        }
        count=static_cast<size_t>(diff)+1;
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
        if (fwlvDebugEnabled())
        {
            std::cerr << "CHAT-FWLV-DEBUG: updateStickingPositions() begin=" << begin
                       << ">0 -> scrollToEdge(HOME), overriding whatever compensateSizeChange() "
                          "just set (m_stick=" << static_cast<int>(m_stick) << ")" << std::endl;
        }
        scrollToEdge(Direction::HOME);
    }
    else if (end<(viewPortSize-1))
    {
        if (fwlvDebugEnabled())
        {
            std::cerr << "CHAT-FWLV-DEBUG: updateStickingPositions() end=" << end << " < "
                          "viewPortSize-1=" << (viewPortSize-1) << " -> scrollToEdge(END), "
                          "overriding whatever compensateSizeChange() just set (m_stick="
                       << static_cast<int>(m_stick) << ")" << std::endl;
        }
        scrollToEdge(Direction::END);
    }
    else if (fwlvDebugEnabled())
    {
        std::cerr << "CHAT-FWLV-DEBUG: updateStickingPositions() begin=" << begin << " end="
                   << end << " viewPortSize=" << viewPortSize << " -- no edge snap" << std::endl;
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
            ((edge>(viewSize-1)) && (viewSize<oldViewSize))
            ||
            ((edge<(viewSize-1)) && (viewSize>oldViewSize))
    ;
    bool moveBegin=(edge<(viewSize-1)) && (viewSize>oldViewSize);

    // if size of viewport changed then list will try to fit the viewport as much as possible
    if ((m_stick==Direction::HOME && moveBegin) || (m_stick==Direction::END && moveEnd))
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
        if (fwlvDebugEnabled())
        {
            std::cerr << "CHAT-FWLV-DEBUG: compensateSizeChange() at-edge branch, m_stick="
                       << static_cast<int>(m_stick) << " llist.pos=" << oprop(m_llist->pos(),OProp::pos)
                       << " -> scrollToEdge" << std::endl;
        }
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
        if (fwlvDebugEnabled())
        {
            // ItemT::IdType is generic (not necessarily std::ostream-streamable, e.g. ObjectId
            // in the whitemdesktop chat instantiation only has toString()) -- not printed here.
            std::cerr << "CHAT-FWLV-DEBUG: compensateSizeChange() anchor item not found at all "
                          "(neither by id, by sort value, nor by linear scan) -- no compensation "
                          "applied" << std::endl;
        }
        return;
    }
    auto oldWidget=oldItem->widget();
    if (!oldWidget)
    {
        if (fwlvDebugEnabled())
        {
            std::cerr << "CHAT-FWLV-DEBUG: compensateSizeChange() anchor item found but has a "
                          "null widget -- no compensation applied" << std::endl;
        }
        return;
    }

    auto oldWidgetPos=oprop(oldWidget,OProp::pos);
    if (oldWidgetPos!=m_firstWidgetPos)
    {
        auto delta=m_firstWidgetPos-oldWidgetPos;
        auto pos=m_llist->pos();
        auto oldListPos=oprop(pos,OProp::pos);
        setOProp(pos,OProp::pos,oprop(pos,OProp::pos)+delta);
        if (fwlvDebugEnabled())
        {
            std::cerr << "CHAT-FWLV-DEBUG: compensateSizeChange() anchor-based: rememberedPos="
                       << m_firstWidgetPos << " currentPos=" << oldWidgetPos << " delta=" << delta
                       << " llist.pos " << oldListPos << " -> " << oprop(pos,OProp::pos)
                       << std::endl;
        }
        m_llist->move(pos);
    }
    else if (fwlvDebugEnabled())
    {
        std::cerr << "CHAT-FWLV-DEBUG: compensateSizeChange() anchor-based: no change, anchor "
                      "already at remembered pos " << m_firstWidgetPos << std::endl;
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
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::checkInvariants(const char* op) const
{
    if (!fwlvDebugEnabled())
    {
        return;
    }

    const auto& order=itemOrder();

    // 1) every item in the sort-order index must still be a live member of the linked list.
    size_t orderCount=0;
    for (auto it=order.begin();it!=order.end();++it)
    {
        ++orderCount;
        auto widget=it->widget();
        if (widget==nullptr)
        {
            std::cerr << "CHAT-FWLV-DEBUG[" << op << "]: item at order pos " << (orderCount-1)
                       << " has a null widget" << std::endl;
            continue;
        }
        if (!m_llist->containsWidget(widget))
        {
            std::cerr << "CHAT-FWLV-DEBUG[" << op << "]: item at order pos " << (orderCount-1)
                       << " widget=" << static_cast<const void*>(widget)
                       << " is orphaned from the linked list" << std::endl;
        }
    }

    // 2) m_llist must never be smaller (main axis) than its own sizeHint(). relayout() has no
    // bound against placing items past the widget's current rect -- if resizeList() hasn't
    // caught up to the true content size yet (e.g. adjustCurrentMessagesList() toggling
    // separator visibility on other items between an insert's own synchronous relayout() and
    // endUpdate()'s resizeList() call), trailing items are positioned past m_llist's
    // bottom/right edge and silently clipped by Qt: still fully "loaded" (present in the index,
    // linked into m_llist, isVisible()==true) but never actually drawn. Checks 1/3 above/below
    // only look at membership and order, not size, so this is the one gap they can't see -- see
    // todo-chat-messages-missing-after-insert.md.
    {
        auto currentMain=oprop(m_llist->size(),OProp::size);
        auto hintMain=oprop(m_llist->sizeHint(),OProp::size);
        // currentMain>0: a zero-size list is "not sized yet", not "undersized" -- endUpdate()'s
        // own resizeList() call is about to give it its first real size. insertContinuousItems()
        // runs its check before that, so without this guard every batch load reports a false
        // positive (observed: "size 0 is smaller than its own sizeHint() 424" on chat open).
        if (currentMain>0 && currentMain<hintMain)
        {
            std::cerr << "CHAT-FWLV-DEBUG[" << op << "]: m_llist main-axis size " << currentMain
                       << " is smaller than its own sizeHint() " << hintMain
                       << " -- trailing items may be clipped past its edge" << std::endl;
        }
    }

    // 3) walking the linked list front-to-back must visit the same n widgets, in the same
    // order, as iterating the sort-order index. widgetAtSeqPos() is O(pos) per call, so this
    // is O(n^2); only ever runs under the debug env var. The probe is capped well beyond the
    // expected length as a defensive bound in case the two have diverged in a way that would
    // otherwise make this loop unbounded.
    std::vector<QWidget*> llWidgets;
    size_t maxProbe=orderCount+64;
    for (size_t pos=0; pos<maxProbe; ++pos)
    {
        auto w=m_llist->widgetAtSeqPos(pos);
        if (w==nullptr)
        {
            break;
        }
        llWidgets.push_back(w);
    }

    if (llWidgets.size()!=orderCount)
    {
        std::cerr << "CHAT-FWLV-DEBUG[" << op << "]: linked-list length " << llWidgets.size()
                   << " != sort-order length " << orderCount << std::endl;
        return;
    }

    size_t i=0;
    for (auto it=order.begin();it!=order.end();++it,++i)
    {
        if (it->widget()!=llWidgets[i])
        {
            std::cerr << "CHAT-FWLV-DEBUG[" << op << "]: order/linked-list order mismatch at "
                          "pos " << i << std::endl;
            break;
        }
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
            //! @todo Latent hazard, not fixed in this pass: FlyweightListItem::sortValue()
            //! reads the sort key *live* from the wrapped, mutable message object rather than
            //! a value copied in at insertion time. This no-op modify() is here so boost knows
            //! the ordered_non_unique index's key *might* have changed and should be
            //! re-checked/re-positioned -- which is only correct because no live update path
            //! mutates a still-inserted item's sort value. The two that could are both
            //! accounted for: the dedup branch above goes through removeItem()+reinsert, and
            //! ChatMessagesView::updateMessage() (which DOES have a caller now --
            //! ChatMessages::upsertMessage()) only ever runs for changes that leave
            //! chat_msg::sort_oid untouched, because ChatMessages::inPlaceUpdateFields()
            //! refuses any sort-key change outright. If a future path ever does change an
            //! item's sort value while it is live in this index, this modify() call is exactly
            //! where it must be paired with the mutation, or the ordered index silently
            //! corrupts (boost's contract: keys must not change without notifying the index).
            idx.modify(result.first,[](auto&){});
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

    // The batch path (insertContinuousItems()) is protected against a stale anchor by
    // LinkedListView_p::insertWidgets()'s own tail-append fallback (triggered when the anchor's
    // LinkedListViewItem property is gone) -- but that fallback is reached only downstream, by
    // accident of what insertWidgets() happens to catch. This single-item path -- used for
    // every dedup remove+reinsert (an already-displayed id whose widget changed) and every live
    // arrival -- had no equivalent check of its own before handing the anchor off.
    // containsWidget() exists specifically to validate an anchor (added for the batch-path fix)
    // but until now was only ever called from the debug-only checkInvariants(). Validate here
    // too: an anchor insertWidgetAfter() can't actually find is worse than no anchor at all --
    // falling back to nullptr (insertWidgetAfter()'s own "no anchor" branch resolves to
    // inserting before head) is always safe, whereas handing over a stale pointer risks
    // whatever insertWidgets() does when its own downstream checks don't happen to catch it.
    if (afterWidget!=nullptr && !m_llist->containsWidget(afterWidget))
    {
        if (fwlvDebugEnabled())
        {
            std::cerr << "CHAT-FWLV-DEBUG: insertItemToContainer() anchor widget is not (or no "
                         "longer) part of the linked list, falling back to no anchor" << std::endl;
        }
        afterWidget=nullptr;
    }

    return afterWidget;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::insertItem(const ItemT& item, bool adjustMinMax)
{
    if (adjustMinMax)
    {
        if (m_orderComparer(item.sortValue(),m_minSortValue))
        {
            m_minSortValue=item.sortValue();
        }
        if (m_orderComparer(m_maxSortValue,item.sortValue()))
        {
            m_maxSortValue=item.sortValue();
        }
    }

    m_llist->insertWidgetAfter(item.widget(),insertItemToContainer(item));
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::reorderItem(const ItemT& item, bool adjustMinMax)
{
    //! @todo Landmine for the "message updated while scrolled away from the stick edge"
    //! symptom in todo-chat-messages-missing-after-insert.md: below, an item whose sort value
    //! moves it past the edge the view is NOT currently sticking to is not reordered but
    //! *deleted* (removeItem()), silently, with no way for the caller to know it vanished from
    //! the view.
    //!
    //! Still unreachable from whitemdesktop, but NOT for the reason this comment used to give
    //! (that ChatMessagesView::updateMessage() had no caller of its own -- it does now, see
    //! ChatMessages::upsertMessage()). What keeps it unreachable is that
    //! ChatMessages::inPlaceUpdateFields() deliberately does NOT exclude chat_msg::sort_oid
    //! from its comparison, so any sort-key change fails the in-place test and falls back to a
    //! full rebuild rather than reaching updateMessage()'s reorder branch. Keep that property
    //! in mind before loosening that check. Fixing this properly needs either re-fetching the
    //! item on demand when scrolled back to that edge, or keeping it hidden-but-tracked
    //! instead of dropped; still out of scope.
    if (adjustMinMax)
    {
        if (m_orderComparer(item.sortValue(),m_minSortValue))
        {
            m_minSortValue=item.sortValue();
        }
        if (m_orderComparer(m_maxSortValue,item.sortValue()))
        {
            m_maxSortValue=item.sortValue();
        }
    }

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

    if (first!=nullptr && m_orderComparer(item.sortValue(),first->sortValue()))
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

    // Insert every item into the sort-order/id container first, WITHOUT asking
    // insertItemToContainer() for an anchor widget yet (findAfterWidget=false). Computing the
    // anchor up front, at i==0, as this used to, is unsafe: a *later* iteration's dedup-by-id
    // branch (see insertItemToContainer() above) can removeItem() an existing entry -- and
    // that entry can be the very widget just captured as the anchor -- leaving
    // insertWidgetsAfter() a dangling/orphaned pointer (the "insertWidgets() silently orphans
    // the entire list" defect). Look the anchor up once, after every container mutation this
    // batch will make is already done.
    std::vector<QWidget*> widgets;
    widgets.reserve(items.size());
    for (const auto& item : items)
    {
        insertItemToContainer(item,false);
        widgets.push_back(item.widget());
    }

    const auto& idx=itemIdx();
    const auto& order=itemOrder();

    QWidget* afterWidget=nullptr;
    bool contiguous=false;
    auto frontIdxIt=idx.find(items.front().id());
    if (frontIdxIt!=idx.end())
    {
        auto orderIt=m_items.template project<0>(frontIdxIt);
        if (orderIt!=order.begin())
        {
            auto beforeIt=orderIt;
            --beforeIt;
            afterWidget=beforeIt->widget();
        }

        // Verify the batch really is contiguous in the final sort order -- "items must be
        // pre-sorted" (documented on the public overload) doesn't imply "contiguous with
        // what's already loaded"; ChatMessagesView::insertFetched()'s incremental branch, for
        // instance, hands whatever range the server returned. insertWidgetsAfter() places the
        // whole batch as one contiguous run after a single anchor -- if some other,
        // already-loaded item actually belongs between two entries of this batch, that
        // placement would silently diverge from sort order, which is exactly the precondition
        // for checkItemCount()'s underflow defect.
        contiguous=true;
        auto checkIt=orderIt;
        for (size_t i=0;i<widgets.size();++i)
        {
            if (checkIt==order.end() || checkIt->widget()!=widgets[i])
            {
                contiguous=false;
                break;
            }
            ++checkIt;
        }
    }

    if (contiguous)
    {
        m_llist->insertWidgetsAfter(widgets,afterWidget);
    }
    else
    {
        // Fall back to placing items one at a time -- correct regardless of ordering, just
        // O(n^2) for this batch. Each item is already registered in the container above, so
        // insertItemToContainer() here only re-derives its (now authoritative) individual
        // anchor; it does not insert a duplicate.
        if (fwlvDebugEnabled())
        {
            std::cerr << "CHAT-FWLV-DEBUG: insertContinuousItems() batch of " << items.size()
                       << " items is not contiguous with the existing sort order, falling back "
                          "to per-item insert" << std::endl;
        }
        for (const auto& item : items)
        {
            m_llist->insertWidgetAfter(item.widget(),insertItemToContainer(item,true));
        }
    }

    checkInvariants("insertContinuousItems");
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::resizeList(const char* caller)
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
        if (fwlvDebugEnabled())
        {
            auto oldListSize=m_llist->size();
            std::cerr << "CHAT-FWLV-DEBUG: resizeList[" << caller << "]() m_llist.size ("
                       << oldListSize.width() << "x" << oldListSize.height() << ") -> ("
                       << listSize.width() << "x" << listSize.height() << ") (sizeHint main="
                       << newSize << ") m_atEnd=" << m_atEnd << " m_atBegin=" << m_atBegin
                       << std::endl;
        }
#if 0
        qDebug() << printCurrentDateTime() << ": FlyweightListView_p::resizeList()  " << m_obj << " set size " << listSize;
#endif
        m_llist->resize(listSize);
        compensateSizeChange();
    }
    else if (fwlvDebugEnabled())
    {
        std::cerr << "CHAT-FWLV-DEBUG: resizeList[" << caller << "]() no change, m_llist.size "
                      "already (" << listSize.width() << "x" << listSize.height() << ")"
                   << std::endl;
    }
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::clear(bool onDestroy)
{
    const auto& order=itemOrder();

    if (!onDestroy)
    {
        for (auto&& it : order)
        {
            auto* widget=it.widget();

            if (m_removeItemCb)
            {
                m_removeItemCb(widget);
            }

            // Widget is dropped (hidden + deleteLater()) below by m_llist->clear(), so there is
            // no need to also take/unlink it from m_llist item by item first -- that would only
            // pay for a redundant reparent (see destroyWidgetFast()) and linked-list bookkeeping
            // that m_llist->clear() is about to discard wholesale anyway.
            PointerHolder::clearProperty(widget,ItemT::Property);
            widget->removeEventFilter(&m_qobjectHelper);
            QObject::disconnect(widget,nullptr,&m_qobjectHelper,nullptr);
        }
    }

    m_llist->blockSignals(true);
    m_llist->clear(ItemT::dropWidgetHandler());
    if (!onDestroy)
    {
        m_llist->move(0,0);
        QSize newListSize;
        setOProp(newListSize,OProp::size,oprop(m_llist->sizeHint(),OProp::size));
        setOProp(newListSize,OProp::size,oprop(m_view,OProp::size,true),true);
        m_llist->resize(newListSize);
    }
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
    m_currentBatchCount=0;

    m_cleared=true;
    m_jumpEdge->setVisible(false);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::scroll(int delta)
{
    auto oldPos=oprop(m_llist,OProp::pos);

    auto cb=[delta](int minPos, int maxPos, int oldPos)
    {
        std::ignore=minPos;
        std::ignore=maxPos;
        return oldPos-delta;
    };

    scrollTo(cb);

    if (m_userScrolledCb && oprop(m_llist,OProp::pos)!=oldPos)
    {
        m_userScrolledCb();
    }
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
    bool firstFound=item!=nullptr;

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

    if (fwlvDebugEnabled())
    {
        std::cerr << "CHAT-FWLV-DEBUG: keepCurrentConfiguration() firstViewportItem="
                   << (firstFound ? "found" : "null") << " lastViewportItem="
                   << (item!=nullptr ? "found" : "null") << " m_firstWidgetPos=" << m_firstWidgetPos
                   << " llist.pos=" << oprop(m_llist->pos(),OProp::pos) << " -> m_atBegin="
                   << m_atBegin << " m_atEnd=" << m_atEnd << std::endl;
    }
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

    checkInvariants("checkItemCount:enter");

    if (itemsCount()==0)
    {
        // don't request items if the list was not loaded yet
        return;
    }

    auto maxHidden=maxHiddenItemsBeyondEdge();
    auto minPrefetch=prefetchThreshold();
    auto prefetch=prefetchItemCountEffective();

    size_t hiddenBefore=0;
    size_t from=0;
    size_t to=0;
    auto first=firstItem();
    auto firstVisible=firstViewportItem();
    if (first&&firstVisible)
    {
        from=m_llist->widgetSeqPos(first->widget());
        to=m_llist->widgetSeqPos(firstVisible->widget());
        // `first` (sort order) and `firstVisible` (visual hit-test) should never disagree in
        // direction, but if the linked list and the sort-order index have drifted apart (the
        // defects guarded against above/below) `to` can end up less than `from`. On unsigned
        // size_t that wraps to ~2^64 and removeExtraItemsFromBegin() below would try to evict
        // the entire list. Compute signed and clamp instead of trusting the order blindly.
        auto diff=static_cast<std::ptrdiff_t>(to)-static_cast<std::ptrdiff_t>(from);
        if (diff<0)
        {
            if (fwlvDebugEnabled())
            {
                std::cerr << "CHAT-FWLV-DEBUG: checkItemCount() hiddenBefore underflow guarded: "
                             "from=" << from << " to=" << to << std::endl;
            }
            diff=0;
        }
        hiddenBefore=static_cast<size_t>(diff);
    }
    bool canFetchBefore=first && m_orderComparer(m_minSortValue,first->sortValue());

#ifdef UISE_DESKTOP_FLYWEIGHTLISTVIEW_DEBUG
    std::cout << printCurrentDateTime() << ": FlyweightListView_p::checkItemCount hiddenBefore "<<hiddenBefore<<" minPrefetch "<<minPrefetch << " prefetch " << prefetch << " maxHidden "<<maxHidden
             << " m_prefetchItemWindow=" << m_prefetchItemWindow
             << " visibleCount()=" << visibleCount()
             << " m_prefetchScreenCount=" << m_prefetchScreenCount
             << " m_prefetchThresholdRatio=" << m_prefetchThresholdRatio
             << " prefetchItemWindow()="<<prefetchItemWindow()
             << " m_prefetchScreenCount="<<m_prefetchScreenCount
             << " m_currentBatchCount="<<m_currentBatchCount
             << " from="<<from
             << " to="<<to;
#endif

    if ((m_currentBatchCount>0 || hiddenBefore<minPrefetch) && canFetchBefore)
    {
        if (m_requestItemsCb)
        {
            if (hiddenBefore<minPrefetch)
            {
                m_currentBatchCount=m_prefetchScreenCount;
            }
            m_currentBatchCount--;
            if (m_currentBatchCount<0)
            {
                m_currentBatchCount=0;
            }
            m_requestItemsCb(firstItem(),prefetch,Direction::HOME);
        }
    }
    else if (hiddenBefore>maxHidden)
    {
        removeExtraItemsFromBegin(hiddenBefore-maxHidden);
    }

    size_t hiddenAfter=0;
    auto last=lastItem();
    auto lastVisible=lastViewportItem();
    size_t fromAfter=0;
    size_t toAfter=0;
    if (last&&lastVisible)
    {
        fromAfter=m_llist->widgetSeqPos(lastVisible->widget());
        toAfter=m_llist->widgetSeqPos(last->widget());
        // Mirrors the hiddenBefore underflow guard above -- see its comment.
        auto diff=static_cast<std::ptrdiff_t>(toAfter)-static_cast<std::ptrdiff_t>(fromAfter);
        if (diff<0)
        {
            if (fwlvDebugEnabled())
            {
                std::cerr << "CHAT-FWLV-DEBUG: checkItemCount() hiddenAfter underflow guarded: "
                             "fromAfter=" << fromAfter << " toAfter=" << toAfter << std::endl;
            }
            diff=0;
        }
        hiddenAfter=static_cast<size_t>(diff);
    }

#ifdef UISE_DESKTOP_FLYWEIGHTLISTVIEW_DEBUG
    std::cout << printCurrentDateTime() << ": FlyweightListView_p::checkItemCount hiddenAfter "<<hiddenAfter
             << " from="<<fromAfter
             << " to="<<toAfter
             << " itemCount="<<itemsCount();
#endif

    bool canFetchAfter=last && m_orderComparer(last->sortValue(),m_maxSortValue);
    if ((m_currentBatchCount>0 || hiddenAfter<minPrefetch)  && canFetchAfter)
    {
        if (m_requestItemsCb)
        {
            if (hiddenAfter<minPrefetch)
            {
                m_currentBatchCount=m_prefetchScreenCount;
            }
            m_currentBatchCount--;
            if (m_currentBatchCount<0)
            {
                m_currentBatchCount=0;
            }
            m_requestItemsCb(lastItem(),prefetch,Direction::END);
        }
    }
    else if (hiddenAfter>maxHidden)
    {
        removeExtraItemsFromEnd(hiddenAfter-maxHidden);
    }

    if (!canFetchBefore && !canFetchAfter)
    {
        m_currentBatchCount=0;
    }
}

#if 0
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

    size_t hiddenBefore=0;
    size_t fromBefore=0;
    size_t toBefore=0;
    auto first=firstItem();
    auto firstVisible=firstViewportItem();
    if (first&&firstVisible)
    {
        fromBefore=m_llist->widgetSeqPos(first->widget());
        toBefore=m_llist->widgetSeqPos(firstVisible->widget());
        hiddenBefore=fromBefore-toBefore;
    }

    size_t hiddenAfter=0;
    auto last=lastItem();
    auto lastVisible=lastViewportItem();
    if (last&&lastVisible)
    {
        auto from=m_llist->widgetSeqPos(lastVisible->widget());
        auto to=m_llist->widgetSeqPos(last->widget());
        hiddenAfter=to-from;
    }

    bool canFetchBefore=first && m_orderComparer(m_minSortValue,first->sortValue());
    bool canFetchAfter=last && m_orderComparer(last->sortValue(),m_maxSortValue);
    if (!canFetchBefore && !canFetchAfter)
    {
        m_currentBatchCount=0;
    }

// #ifdef UISE_DESKTOP_FLYWEIGHTLISTVIEW_DEBUG
    qDebug() << printCurrentDateTime() << ": FlyweightListView_p::checkItemCount hiddenBefore "<<hiddenBefore<<" minPrefetch "<<minPrefetch << " prefetch " << prefetch << " maxHidden "<<maxHidden
             << " m_prefetchItemWindow=" << m_prefetchItemWindow
             << " visibleCount()=" << visibleCount()
             << " m_prefetchScreenCount=" << m_prefetchScreenCount
             << " m_prefetchThresholdRatio=" << m_prefetchThresholdRatio
             << " prefetchItemWindow()="<<prefetchItemWindow()
             << " m_prefetchScreenCount="<<m_prefetchScreenCount
             << " m_currentBatchCount="<<m_currentBatchCount
             << " canFetchBefore="<<canFetchBefore
             << " canFetchAfter="<<canFetchAfter
             << " from="<<fromBefore
             << " to="<<toBefore;
// #endif

    if ((m_currentBatchCount>0 || hiddenBefore<minPrefetch) && canFetchBefore)
    {
        if (m_requestItemsCb)
        {
            if (hiddenBefore<minPrefetch)
            {
                m_currentBatchCount=m_prefetchScreenCount;
            }
            m_currentBatchCount--;
            if (m_currentBatchCount<0)
            {
                m_currentBatchCount=0;
            }
            m_requestItemsCb(firstItem(),prefetch,Direction::HOME);
        }
    }
    else if (hiddenBefore>maxHidden)
    {
        removeExtraItemsFromBegin(hiddenBefore-maxHidden);
    }

    if ((m_currentBatchCount>0 || hiddenAfter<minPrefetch)  && canFetchAfter)
    {
        if (m_requestItemsCb)
        {
            if (hiddenAfter<minPrefetch)
            {
                m_currentBatchCount=m_prefetchScreenCount;
            }
            m_currentBatchCount--;
            if (m_currentBatchCount<0)
            {
                m_currentBatchCount=0;
            }
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

#endif

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::removeExtraItemsFromBegin(size_t count)
{
    if (count==0)
    {
        return;
    }

    // Defensive boundary: whatever `count` says, never evict the item currently visible at the
    // top of the viewport, or anything after it. `count` is derived from widgetSeqPos()
    // distances that checkItemCount() already guards against going negative, but stopping at a
    // real viewport boundary here makes "count too large -> whole list disappears" structurally
    // impossible rather than merely unlikely -- belt and braces for the same underflow defect.
    auto boundary=firstViewportItem();

    beginUpdate();

    auto& order=itemOrder();
    for (auto it=order.begin();it!=order.end();)
    {
        if (count--==0)
        {
            break;
        }
        if (boundary!=nullptr && &(*it)==boundary)
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

    // Mirrors removeExtraItemsFromBegin()'s boundary guard -- see its comment.
    auto boundary=lastViewportItem();

    beginUpdate();

    auto& order=itemOrder();
    for (auto it=order.rbegin(), nit=it;it!=order.rend(); it=nit)
    {
        if (count--==0)
        {
            break;
        }
        if (boundary!=nullptr && &(*it)==boundary)
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
            m_vbarHolder->setVisible(false);
        break;

        case Qt::ScrollBarAlwaysOn:
            m_vbarHolder->setVisible(true);
            m_vbar->setMaximum(0);
        break;

        case Qt::ScrollBarAsNeeded:
            m_vbarHolder->setVisible(m_view->height()<m_llist->height());
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

    auto jumpEdgeInvisibleItemCount=m_jumpEdge->badgeText().isEmpty()? m_jumpEdgeInvisibleItemCount : 2;

    if (m_stick==Direction::HOME)
    {
        auto it=order.find(m_firstViewportSortValue);
        if (it!=order.end())
        {
            for (auto it1=order.begin();it1!=it;++it1)
            {
                invisibleCount++;
                if (invisibleCount==jumpEdgeInvisibleItemCount)
                {
                    showControl=true;
                    break;
                }
            }
        }
    }
    else
    {
        // Exclude the last-viewport item itself, matching the HOME branch's own [begin, it)
        // above, which never counts its own boundary item -- this used to start the count AT
        // m_lastViewportSortValue's own hit, so the control showed with one fewer genuinely
        // below-the-fold item than the HOME direction required.
        //
        // Briefly reverted on 2026-08-25 while todo-chat-messages-missing-after-insert.md was
        // open: that bug left trailing items in the index but undrawn, and the off-by-one was
        // the only visual cue they existed. Re-applied once the root cause (the setNextAuto()
        // ordering defect in LinkedListView_p::insertWidgets(), see linkedlistview.cpp) was
        // fixed and confirmed, so the control no longer has to double as a bug indicator.
        auto it=order.find(m_lastViewportSortValue);
        if (it!=order.end())
        {
            ++it;
        }
        for (;it!=order.end();++it)
        {
            invisibleCount++;
            if (invisibleCount==jumpEdgeInvisibleItemCount)
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
    // Must stay in lockstep with checkItemCount()'s canFetchBefore/canFetchAfter (this is
    // literally their negation: "nothing more beyond the loaded window" <=> "a local scroll is
    // enough"). checkItemCount() uses the live firstItem()/lastItem() and a strict less-than
    // against m_minSortValue/m_maxSortValue; this used to instead test *exact* equality against
    // the *cached* m_firstItem/m_lastItem. Those two only need to disagree once -- e.g. after a
    // bulk load whose last item's sort value overshoots a stale m_maxSortValue (loadItems()/
    // insertContinuousItems()/insertItems() never adjust it, and clear() never resets it) -- for
    // the jump-edge button to trigger a full reload where the scroll-driven prefetch already
    // considers the window complete.
    if (direction==Direction::END)
    {
        auto last=lastItem();
        if (!m_enableFlyweight)
        {
            scrollToEdge(Direction::END);
        }
        else if (last!=nullptr && !m_orderComparer(last->sortValue(),m_maxSortValue))
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
        auto first=firstItem();
        if (!m_enableFlyweight)
        {
            scrollToEdge(Direction::HOME);
        }
        else if (first!=nullptr && !m_orderComparer(m_minSortValue,first->sortValue()))
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
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::setVerticalScrollBarPlaceHolder(bool enable)
{
    m_vbarHolder->setHoldPlace(enable);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
bool FlyweightListView_p<ItemT,OrderComparer,IdComparer>::isVerticalScrollBarPlaceHolder() const
{
    return m_vbarHolder->isHoldPlace();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView_p<ItemT,OrderComparer,IdComparer>::updateListAlignment()
{
    Qt::Alignment val;

    if (m_llist->orientation()==Qt::Horizontal)
    {
        switch(m_stick)
        {
            case(Direction::HOME):
                val=Qt::AlignLeft;
                break;

            case(Direction::END):
                val=Qt::AlignRight;
                break;

            default:
                break;
        }
    }
    else
    {
        switch(m_stick)
        {
            case(Direction::HOME):
                val=Qt::AlignTop;
                break;

            case(Direction::END):
                val=Qt::AlignBottom;
                break;

            default:
                break;
        }
    }

    m_llist->setAlignment(val);
}

//--------------------------------------------------------------------------

} // namespace detail

//--------------------------------------------------------------------------

#ifdef _MSC_VER

#pragma warning(pop) // size_t to int warnings

#endif

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_FLYWEIGHTLISTVIEW_P_IPP
