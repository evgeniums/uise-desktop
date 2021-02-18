/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/desktop/flyweightlistview.ipp
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
template <typename ItemT>
FlyweightListView<ItemT>::FlyweightListView(
        QWidget* parent,
        size_t prefetchItemCount
    ) : QFrame(parent),
        pimpl(std::make_unique<detail::FlyweightListView_p<ItemT>>(this,prefetchItemCount))
{
    pimpl->setupUi();

    pimpl->m_view->installEventFilter(this);
}

//--------------------------------------------------------------------------
template <typename ItemT>
FlyweightListView<ItemT>::~FlyweightListView()
{
    beginUpdate();
    pimpl->clear();
}

//--------------------------------------------------------------------------
template <typename ItemT>
FlyweightListView<ItemT>::FlyweightListView(
        size_t prefetchItemCount
    ) : FlyweightListView(nullptr,prefetchItemCount)
{
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setPrefetchItemCountHint(size_t val) noexcept
{
    pimpl->m_prefetchItemCount=val;
}

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView<ItemT>::prefetchItemCount() const noexcept
{
    return pimpl->prefetchItemCount();
}

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView<ItemT>::prefetchItemCountHint() const noexcept
{
    return pimpl->m_prefetchItemCountHint;
}

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView<ItemT>::visibleItemCount() const noexcept
{
    return pimpl->visibleCount();
}

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView<ItemT>::maxHiddenItemCount() const noexcept
{
    return pimpl->maxHiddenItemsBeyondEdge();
}

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView<ItemT>::itemCount() const noexcept
{
    return pimpl->m_items.size();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::beginUpdate()
{
    pimpl->beginUpdate();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::endUpdate()
{
    pimpl->endUpdate();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setRequestItemsCb(RequestItemsCb cb) noexcept
{
    pimpl->m_requestItemsCb=std::move(cb);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setViewportChangedCb(ItemRangeCb cb) noexcept
{
    pimpl->m_viewportChangedCb=std::move(cb);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setItemRangeChangedCb(ItemRangeCb cb) noexcept
{
    pimpl->m_itemRangeChangedCb=std::move(cb);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setRequestHomeCb(RequestJumpCb cb) noexcept
{
    pimpl->m_homeRequestCb=std::move(cb);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setRequestEndCb(RequestJumpCb cb) noexcept
{
    pimpl->m_endRequestCb=std::move(cb);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::clear()
{
    beginUpdate();
    pimpl->clear();
    endUpdate();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setOrientation(Qt::Orientation orientation)
{
    pimpl->setOrientation(orientation);
}

//--------------------------------------------------------------------------
template <typename ItemT>
Qt::Orientation FlyweightListView<ItemT>::orientation() const noexcept
{
    return pimpl->m_llist->orientation();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::loadItems(const std::vector<ItemT> &items)
{
    beginUpdate();
    pimpl->clear();
    pimpl->insertContinuousItems(items);
    endUpdate();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::insertItems(const std::vector<ItemT> &items)
{
    beginUpdate();
    for (auto&& item:items)
    {
        pimpl->insertItem(item);
    }
    endUpdate();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::insertContinuousItems(const std::vector<ItemT> &items)
{
    beginUpdate();
    pimpl->insertContinuousItems(items);
    endUpdate();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::insertItem(const ItemT& item)
{
    pimpl->insertItem(item);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::removeItem(const typename ItemT::IdType &id)
{
    pimpl->removeItem(id);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::removeItems(const std::vector<typename ItemT::IdType> &ids)
{
    beginUpdate();
    for (auto&& id : ids)
    {
        removeItem(id);
    }
    endUpdate();
}

//--------------------------------------------------------------------------
template <typename ItemT>
bool FlyweightListView<ItemT>::scrollToItem(const typename ItemT::IdType &id, int offset)
{
    return pimpl->scrollToItem(id,offset);
}

//--------------------------------------------------------------------------
template <typename ItemT>
bool FlyweightListView<ItemT>::hasItem(const typename ItemT::IdType &id) const noexcept
{
    return pimpl->hasItem(id);
}

//--------------------------------------------------------------------------
template <typename ItemT>
const ItemT* FlyweightListView<ItemT>::item(const typename ItemT::IdType &id) const noexcept
{
    return pimpl->item(id);
}

//--------------------------------------------------------------------------
template <typename ItemT>
const ItemT* FlyweightListView<ItemT>::firstItem() const noexcept
{
    return pimpl->firstItem();
}

//--------------------------------------------------------------------------
template <typename ItemT>
const ItemT* FlyweightListView<ItemT>::firstViewportItem() const noexcept
{
    return pimpl->firstViewportItem();
}

//--------------------------------------------------------------------------
template <typename ItemT>
const ItemT* FlyweightListView<ItemT>::lastItem() const noexcept
{
    return pimpl->lastItem();
}

//--------------------------------------------------------------------------
template <typename ItemT>
const ItemT* FlyweightListView<ItemT>::lastViewportItem() const noexcept
{
    return pimpl->lastViewportItem();
}

//--------------------------------------------------------------------------
template <typename ItemT>
bool FlyweightListView<ItemT>::isScrollAtEdge(Direction direction) const noexcept
{
    return direction==Direction::END?pimpl->isAtEnd():pimpl->isAtBegin();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::scrollToEdge(Direction direction)
{
    pimpl->scrollToEdge(direction);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setWheelHorizontalScrollEnabled(bool enabled) noexcept
{
    pimpl->m_scrollWheelHorizontal=enabled;
}

//--------------------------------------------------------------------------
template <typename ItemT>
bool FlyweightListView<ItemT>::islWheelHorizontaScrollEnabled() const noexcept
{
    return pimpl->m_scrollWheelHorizontal;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    pimpl->onResized();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setFlyweightEnabled(bool enable) noexcept
{
    return pimpl->setFlyweightEnabled(enable);
}

//--------------------------------------------------------------------------
template <typename ItemT>
bool FlyweightListView<ItemT>::isFlyweightEnabled() const noexcept
{
    return pimpl->isFlyweightEnabled();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setStickMode(Direction mode) noexcept
{
    pimpl->m_stick=mode;
}

//--------------------------------------------------------------------------
template <typename ItemT>
Direction FlyweightListView<ItemT>::stickMode() const noexcept
{
    return pimpl->m_stick;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::scroll(int delta)
{
    pimpl->scroll(delta);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setSingleScrollStep(size_t value) noexcept
{
    pimpl->m_singleStep=value;
}

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView<ItemT>::singleScrollStep() const noexcept
{
    return pimpl->m_singleStep;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setMinPageScrollStep(size_t value) noexcept
{
    pimpl->m_minPageStep=value;
    pimpl->updatePageStep();
}

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView<ItemT>::minPageScrollStep() const noexcept
{
    return pimpl->m_minPageStep;
}

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView<ItemT>::pageScrollStep() const noexcept
{
    return pimpl->m_pageStep;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::keyPressEvent(QKeyEvent *event)
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
        if (pimpl->m_homeRequestCb)
        {
            pimpl->m_homeRequestCb(event->modifiers());
        }
    }
    else if (event->key()==Qt::Key_End)
    {
        if (pimpl->m_endRequestCb)
        {
            pimpl->m_endRequestCb(event->modifiers());
        }
    }

    QFrame::keyPressEvent(event);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::wheelEvent(QWheelEvent *event)
{
    pimpl->wheelEvent(event);
}

//--------------------------------------------------------------------------
template <typename ItemT>
bool FlyweightListView<ItemT>::eventFilter(QObject *watched, QEvent *event)
{
    if (watched==pimpl->m_view && event->type()==QEvent::Resize)
    {
        auto e=dynamic_cast<QResizeEvent*>(event);
        pimpl->onViewportResized(e);
    }

    return false;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setMaxSortValue(const typename ItemT::SortValueType &value) noexcept
{
    pimpl->m_maxSortValue=value;
}

//--------------------------------------------------------------------------
template <typename ItemT>
typename ItemT::SortValueType FlyweightListView<ItemT>::maxSortValue() const noexcept
{
    return pimpl->m_maxSortValue;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setMinSortValue(const typename ItemT::SortValueType &value) noexcept
{
    pimpl->m_minSortValue=value;
}

//--------------------------------------------------------------------------
template <typename ItemT>
typename ItemT::SortValueType FlyweightListView<ItemT>::minSortValue() const noexcept
{
    return pimpl->m_minSortValue;
}

//--------------------------------------------------------------------------
template <typename ItemT>
QScrollBar* FlyweightListView<ItemT>::verticalScrollBar() const noexcept
{
    return pimpl->m_vbar;
}

//--------------------------------------------------------------------------
template <typename ItemT>
Qt::ScrollBarPolicy FlyweightListView<ItemT>::verticalScrollBarPolicy() const noexcept
{
    return pimpl->m_vbarPolicy;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setVerticalScrollBarPolicy(Qt::ScrollBarPolicy policy)
{
    pimpl->m_vbarPolicy=policy;
    pimpl->updateScrollBars();
}

//--------------------------------------------------------------------------
template <typename ItemT>
QScrollBar* FlyweightListView<ItemT>::horizontalScrollBar() const noexcept
{
    return pimpl->m_hbar;
}

//--------------------------------------------------------------------------
template <typename ItemT>
Qt::ScrollBarPolicy FlyweightListView<ItemT>::horizontalScrollBarPolicy() const noexcept
{
    return pimpl->m_hbarPolicy;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy policy)
{
    pimpl->m_hbarPolicy=policy;
    pimpl->updateScrollBars();
}

//--------------------------------------------------------------------------
template <typename ItemT>
QSize FlyweightListView<ItemT>::viewportSize() const noexcept
{
    return pimpl->m_view->size();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

#endif // #define UISE_DESKTOP_FLYWEIGHTLISTVIEW_IPP
