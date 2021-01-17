/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** \file uise/desktop/flyweightlistview.ipp
*
*  Contains implementation of FlyweightListView.
*
*/

/****************************************************************************/

#include <set>

#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/detail/flyweightlistview_p.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------
template <typename ItemT>
FlyweightListView<ItemT>::FlyweightListView(
        QWidget* parent,
        size_t prefetchItemCount
    ) : FlyweightListFrame(parent),
        pimpl(std::make_unique<detail::FlyweightListView_p<ItemT>>(prefetchItemCount))
{
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
void FlyweightListView<ItemT>::setPrefetchItemCount(size_t val) noexcept
{
    pimpl->m_prefetchItemCount=val;
}

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView<ItemT>::prefetchItemCount() const noexcept
{
    return pimpl->m_prefetchItemCount;
}

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView<ItemT>::itemCount() const noexcept
{
    return pimpl->m_items.size();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::beginInserting() noexcept
{
    pimpl->m_inserting=true;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::endInserting()
{
    pimpl->m_inserting=false;
    //! @todo repaint
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setRequestItemsBeforeCb(RequestItemsCb cb) noexcept
{
    pimpl->m_requestItemsBeforeCb=std::move(cb);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setRequestItemsAfterCb(RequestItemsCb cb) noexcept
{
    pimpl->m_requestItemsAfterCb=std::move(cb);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setSingleScrollStep(size_t step) noexcept
{
    pimpl->m_singleScrollStep=step;
}

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView<ItemT>::singleScrollStep() const noexcept
{
    return pimpl->m_singleScrollStep;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setPageScrollStep(size_t step) noexcept
{
    pimpl->m_pageScrollStep=step;
}

//--------------------------------------------------------------------------
template <typename ItemT>
size_t FlyweightListView<ItemT>::pageScrollStep() const noexcept
{
    return pimpl->m_pageScrollStep;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::clear()
{
    //! @todo implement clear
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::reload()
{
    clear();
    if (pimpl->m_requestItemsBeforeCb)
    {
        pimpl->m_requestItemsBeforeCb(nullptr,pimpl->m_prefetchItemCount);
    }
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::resort()
{
    //! @todo implement resort
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setOrientation(Qt::Orientation orientation) noexcept
{
    pimpl->m_orientation=orientation;
}

//--------------------------------------------------------------------------
template <typename ItemT>
Qt::Orientation FlyweightListView<ItemT>::orientation() const noexcept
{
    return pimpl->m_orientation;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::loadItems(const std::vector<ItemT> &items, Direction scrollTo)
{
    clear();
    insertItems(items,scrollTo);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::insertItems(const std::vector<ItemT> &items, Direction scrollTo)
{
    beginInserting();
    for (auto&& item:items)
    {
        pimpl->insertItem(item);
    }
    endInserting();
    scrollToEdge(scrollTo);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::insertItem(ItemT item, Direction scrollTo)
{
    pimpl->insertItem(std::move(item));
    scrollToEdge(scrollTo);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::scroll(int64_t delta)
{
    //! @todo Implement scroll
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::scrollTo(const typename ItemT::IdType &id, size_t offset, Direction offsetDirection)
{
    //! @todo Implement scrollTo
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::scrollToEdge(Direction offsetDirection)
{
    //! @todo Implement scrollToEdge
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::collapse(size_t animationDurationMs, Direction offsetDirection)
{
    //! @todo Implement collapse
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::expand(size_t animationDurationMs)
{
    //! @todo Implement expand
}

//--------------------------------------------------------------------------
template <typename ItemT>
bool FlyweightListView<ItemT>::isCollapsed() const noexcept
{
    return pimpl->m_collapsed;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_EMD
