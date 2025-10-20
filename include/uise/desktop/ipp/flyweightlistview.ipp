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

/** @file uise/desktop/ipp/flyweightlistview.ipp
*
*  Contains implementation of FlyweightListView.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FLYWEIGHTLISTVIEW_IPP
#define UISE_DESKTOP_FLYWEIGHTLISTVIEW_IPP

#include <QEvent>
#include <QKeyEvent>

#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/detail/flyweightlistview_p.hpp>
#include <uise/desktop/detail/flyweightlistview_p.ipp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
FlyweightListView<ItemT,OrderComparer,IdComparer>::FlyweightListView(
        QWidget* parent,
        size_t prefetchItemWindow,
        OrderComparer orderComparer,
        IdComparer idComparer
    ) : QFrame(parent),
        pimpl(std::make_unique<detail::FlyweightListView_p<ItemT,OrderComparer,IdComparer>>(
                    this,prefetchItemWindow,std::move(orderComparer),std::move(idComparer)
                )
             )
{
    pimpl->setupUi();

    pimpl->m_view->installEventFilter(this);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
FlyweightListView<ItemT,OrderComparer,IdComparer>::FlyweightListView(
    size_t prefetchItemWindow,
    OrderComparer orderComparer,
    IdComparer idComparer
    ) : FlyweightListView(nullptr,prefetchItemWindow,std::move(orderComparer),std::move(idComparer))
{
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
FlyweightListView<ItemT,OrderComparer,IdComparer>::~FlyweightListView()
{
    beginUpdate();
    resetCallbacks();
    pimpl->clear();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::resetCallbacks()
{
    pimpl->resetCallbacks();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setPrefetchItemWindowHint(size_t val) noexcept
{
    pimpl->m_prefetchItemWindow=val;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView<ItemT,OrderComparer,IdComparer>::prefetchItemWindow() const noexcept
{
    return pimpl->prefetchItemWindow();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView<ItemT,OrderComparer,IdComparer>::prefetchItemWindowHint() const noexcept
{
    return pimpl->m_prefetchItemWindowHint;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView<ItemT,OrderComparer,IdComparer>::visibleItemCount() const noexcept
{
    return pimpl->visibleCount();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView<ItemT,OrderComparer,IdComparer>::maxHiddenItemCount() const noexcept
{
    return pimpl->maxHiddenItemsBeyondEdge();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView<ItemT,OrderComparer,IdComparer>::itemCount() const noexcept
{
    return pimpl->m_items.size();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::beginUpdate()
{
    pimpl->beginUpdate();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::endUpdate()
{
    pimpl->endUpdate();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setRequestItemsCb(RequestItemsCb cb) noexcept
{
    pimpl->m_requestItemsCb=std::move(cb);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setViewportChangedCb(ItemRangeCb cb) noexcept
{
    pimpl->m_viewportChangedCb=std::move(cb);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setItemRangeChangedCb(ItemRangeCb cb) noexcept
{
    pimpl->m_itemRangeChangedCb=std::move(cb);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setRequestHomeCb(RequestJumpCb cb) noexcept
{
    pimpl->m_homeRequestCb=std::move(cb);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setRequestEndCb(RequestJumpCb cb) noexcept
{
    pimpl->m_endRequestCb=std::move(cb);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setInsertItemCb(InsertItemCb cb) noexcept
{
    pimpl->m_insertItemCb=std::move(cb);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setRemoveItemCb(RemoveItemCb cb) noexcept
{
    pimpl->m_removeItemCb=std::move(cb);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::clear()
{
    beginUpdate();
    pimpl->clear();
    endUpdate();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setOrientation(Qt::Orientation orientation)
{
    pimpl->setOrientation(orientation);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
Qt::Orientation FlyweightListView<ItemT,OrderComparer,IdComparer>::orientation() const noexcept
{
    return pimpl->m_llist->orientation();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::loadItems(const std::vector<ItemT> &items)
{
    beginUpdate();
    pimpl->clear();
    pimpl->insertContinuousItems(items);
    endUpdate();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::insertItems(const std::vector<ItemT> &items)
{
    beginUpdate();
    for (auto&& item:items)
    {
        pimpl->insertItem(item);
    }
    endUpdate();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::insertContinuousItems(const std::vector<ItemT> &items)
{
    beginUpdate();
    pimpl->insertContinuousItems(items);
    endUpdate();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::insertItem(const ItemT& item)
{
    pimpl->insertItem(item);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::reorderItem(const ItemT& item)
{
    pimpl->reorderItem(item);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::removeItem(const typename ItemT::IdType &id)
{
    pimpl->removeItem(id);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::removeItems(const std::vector<typename ItemT::IdType> &ids)
{
    beginUpdate();
    for (auto&& id : ids)
    {
        removeItem(id);
    }
    endUpdate();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
bool FlyweightListView<ItemT,OrderComparer,IdComparer>::scrollToItem(const typename ItemT::IdType &id, int offset)
{
    return pimpl->scrollToItem(id,offset);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
bool FlyweightListView<ItemT,OrderComparer,IdComparer>::hasItem(const typename ItemT::IdType &id) const noexcept
{
    return pimpl->hasItem(id);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
const ItemT* FlyweightListView<ItemT,OrderComparer,IdComparer>::item(const typename ItemT::IdType &id) const noexcept
{
    return pimpl->item(id);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
const ItemT* FlyweightListView<ItemT,OrderComparer,IdComparer>::firstItem() const noexcept
{
    return pimpl->firstItem();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
const ItemT* FlyweightListView<ItemT,OrderComparer,IdComparer>::firstViewportItem() const noexcept
{
    return pimpl->firstViewportItem();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
const ItemT* FlyweightListView<ItemT,OrderComparer,IdComparer>::lastItem() const noexcept
{
    return pimpl->lastItem();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
const ItemT* FlyweightListView<ItemT,OrderComparer,IdComparer>::lastViewportItem() const noexcept
{
    return pimpl->lastViewportItem();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
bool FlyweightListView<ItemT,OrderComparer,IdComparer>::isScrollAtEdge(Direction direction) const noexcept
{
    return direction==Direction::END?pimpl->isAtEnd():pimpl->isAtBegin();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::scrollToEdge(Direction direction)
{
    pimpl->scrollToEdge(direction);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setWheelHorizontalScrollEnabled(bool enabled) noexcept
{
    pimpl->m_scrollWheelHorizontal=enabled;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
bool FlyweightListView<ItemT,OrderComparer,IdComparer>::isWheelHorizontaScrollEnabled() const noexcept
{
    return pimpl->m_scrollWheelHorizontal;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    pimpl->onResized();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setFlyweightEnabled(bool enable) noexcept
{
    return pimpl->setFlyweightEnabled(enable);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
bool FlyweightListView<ItemT,OrderComparer,IdComparer>::isFlyweightEnabled() const noexcept
{
    return pimpl->isFlyweightEnabled();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setStickMode(Direction mode) noexcept
{
    pimpl->m_stick=mode;
    pimpl->m_jumpEdge->setDirection(mode);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
Direction FlyweightListView<ItemT,OrderComparer,IdComparer>::stickMode() const noexcept
{
    return pimpl->m_stick;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::scroll(int delta)
{
    pimpl->scroll(delta);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setSingleScrollStep(size_t value) noexcept
{
    pimpl->m_singleStep=value;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView<ItemT,OrderComparer,IdComparer>::singleScrollStep() const noexcept
{
    return pimpl->m_singleStep;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setMinPageScrollStep(size_t value) noexcept
{
    pimpl->m_minPageStep=value;
    pimpl->updatePageStep();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView<ItemT,OrderComparer,IdComparer>::minPageScrollStep() const noexcept
{
    return pimpl->m_minPageStep;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView<ItemT,OrderComparer,IdComparer>::pageScrollStep() const noexcept
{
    return pimpl->m_pageStep;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::keyPressEvent(QKeyEvent *event)
{
    if (event->key()==(pimpl->isHorizontal()?Qt::Key_Left:Qt::Key_Up))
    {
        scroll(-static_cast<int>(pimpl->m_singleStep));
    }
    else if (event->key()==(pimpl->isHorizontal()?Qt::Key_Right:Qt::Key_Down))
    {
        scroll(static_cast<int>(pimpl->m_singleStep));
    }
    else if (event->key()==Qt::Key_PageUp)
    {
        scroll(-static_cast<int>(pimpl->m_pageStep));
    }
    else if (event->key()==Qt::Key_PageDown)
    {
        scroll(static_cast<int>(pimpl->m_pageStep));
    }
    else if (event->key()==Qt::Key_Home)
    {
        pimpl->jumpToEdge(Direction::HOME,false,event->modifiers());
    }
    else if (event->key()==Qt::Key_End)
    {
        pimpl->jumpToEdge(Direction::END,false,event->modifiers());
    }

    QFrame::keyPressEvent(event);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::wheelEvent(QWheelEvent *event)
{
    pimpl->wheelEvent(event);
    QFrame::wheelEvent(event);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
bool FlyweightListView<ItemT,OrderComparer,IdComparer>::eventFilter(QObject *watched, QEvent *event)
{
    if (watched==pimpl->m_view && event->type()==QEvent::Resize)
    {
        auto e=dynamic_cast<QResizeEvent*>(event);
        pimpl->onViewportResized(e);
    }

    return false;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setMaxSortValue(const typename ItemT::SortValueType &value) noexcept
{
    pimpl->m_maxSortValue=value;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
typename ItemT::SortValueType FlyweightListView<ItemT,OrderComparer,IdComparer>::maxSortValue() const noexcept
{
    return pimpl->m_maxSortValue;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setMinSortValue(const typename ItemT::SortValueType &value) noexcept
{
    pimpl->m_minSortValue=value;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
typename ItemT::SortValueType FlyweightListView<ItemT,OrderComparer,IdComparer>::minSortValue() const noexcept
{
    return pimpl->m_minSortValue;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
QScrollBar* FlyweightListView<ItemT,OrderComparer,IdComparer>::verticalScrollBar() const noexcept
{
    return pimpl->m_vbar;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
Qt::ScrollBarPolicy FlyweightListView<ItemT,OrderComparer,IdComparer>::verticalScrollBarPolicy() const noexcept
{
    return pimpl->m_vbarPolicy;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setVerticalScrollBarPolicy(Qt::ScrollBarPolicy policy)
{
    pimpl->m_vbarPolicy=policy;
    pimpl->updateScrollBars();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
QScrollBar* FlyweightListView<ItemT,OrderComparer,IdComparer>::horizontalScrollBar() const noexcept
{
    return pimpl->m_hbar;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
Qt::ScrollBarPolicy FlyweightListView<ItemT,OrderComparer,IdComparer>::horizontalScrollBarPolicy() const noexcept
{
    return pimpl->m_hbarPolicy;
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy policy)
{
    pimpl->m_hbarPolicy=policy;
    pimpl->updateScrollBars();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
QSize FlyweightListView<ItemT,OrderComparer,IdComparer>::viewportSize() const noexcept
{
    return pimpl->m_view->size();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setPrefetchScreensCount(double value)
{
    pimpl->setPrefetchScreensCount(value);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
double FlyweightListView<ItemT,OrderComparer,IdComparer>::prefetchScreensCount() const noexcept
{
    return pimpl->prefetchScreensCount();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setPrefetchThresholdRatio(double value)
{
    pimpl->setPrefetchThresholdRatio(value);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
double FlyweightListView<ItemT,OrderComparer,IdComparer>::prefetchThresholdRatio() const noexcept
{
    return pimpl->prefetchThresholdRatio();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setPrefetchItemCount(size_t value)
{
    pimpl->setPrefetchItemCount(value);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::resetPrefetchItemCount()
{
    pimpl->resetPrefetchItemCount();
}


//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView<ItemT,OrderComparer,IdComparer>::prefetchItemCount() const noexcept
{
    return pimpl->prefetchItemCount();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView<ItemT,OrderComparer,IdComparer>::prefetchItemCountAuto() noexcept
{
    return pimpl->prefetchItemCountAuto();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView<ItemT,OrderComparer,IdComparer>::prefetchItemCountEffective() noexcept
{
    return pimpl->prefetchItemCountEffective();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setJumpEdgeControlEnabled(bool value)
{
    pimpl->setJumpEdgeControlEnabled(value);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
bool FlyweightListView<ItemT,OrderComparer,IdComparer>::isJumpEdgeControlEnabled() const
{
    return pimpl->isJumpEdgeControlEnabled();
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
void FlyweightListView<ItemT,OrderComparer,IdComparer>::setJumpEdgeInvisibleItemCount(size_t value)
{
    pimpl->setJumpEdgeInvisibleItemCount(value);
}

//--------------------------------------------------------------------------
template <typename ItemT, typename OrderComparer, typename IdComparer>
size_t FlyweightListView<ItemT,OrderComparer,IdComparer>::jumpEdgeInvisibleItemCount() const
{
    return pimpl->jumpEdgeInvisibleItemCount();
}

template <typename ItemT, typename OrderComparer, typename IdComparer>
JumpEdge* FlyweightListView<ItemT,OrderComparer,IdComparer>::jumpEdgeControl() const
{
    return pimpl->jumpEdgeControl();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

#endif // #define UISE_DESKTOP_FLYWEIGHTLISTVIEW_IPP
