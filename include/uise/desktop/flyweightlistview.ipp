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

#include <QEvent>
#include <QKeyEvent>

#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/detail/flyweightlistview_p.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------
template <typename ItemT>
FlyweightListView<ItemT>::FlyweightListView(
        QWidget* parent,
        size_t prefetchItemCount
    ) : FlyweightListFrame(parent),
        pimpl(std::make_unique<detail::FlyweightListView_p<ItemT>>(this,prefetchItemCount))
{
    pimpl->setupUi();
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
void FlyweightListView<ItemT>::setViewportChangedCb(ViewportChangedCb cb) noexcept
{
    pimpl->m_viewportChangedCb=std::move(cb);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::clear()
{
    pimpl->removeAllItems();
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
void FlyweightListView<ItemT>::setOrientation(Qt::Orientation orientation) noexcept
{
    pimpl->m_llist->setOrientation(orientation);
}

//--------------------------------------------------------------------------
template <typename ItemT>
Qt::Orientation FlyweightListView<ItemT>::orientation() const noexcept
{
    return pimpl->m_llist->orientation();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::loadItems(const std::vector<ItemT> &items, Direction scrollTo)
{
    clear();
    beginUpdate();
    pimpl->insertContinuousItems(items);
    endUpdate();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::insertItems(const std::vector<ItemT> &items, Direction scrollTo)
{
    auto wasAtEdge=isScrollAtEdge(scrollTo);
    beginUpdate();
    for (auto&& item:items)
    {
        pimpl->insertItem(item);
    }
    endUpdate();
    pimpl->scrollToEdge(scrollTo,wasAtEdge);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::insertContinuousItems(const std::vector<ItemT> &items, Direction scrollTo)
{
    auto wasAtEdge=isScrollAtEdge(scrollTo);
    beginUpdate();
    pimpl->insertContinuousItems(items);
    endUpdate();
    pimpl->scrollToEdge(scrollTo,wasAtEdge);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::insertItem(const ItemT& item, Direction scrollTo)
{
    auto wasAtEdge=isScrollAtEdge(scrollTo);
    pimpl->insertItem(ItemT(item));
    pimpl->scrollToEdge(scrollTo,wasAtEdge);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::insertItem(ItemT&& item, Direction scrollTo)
{
    auto wasAtEdge=isScrollAtEdge(scrollTo);
    pimpl->insertItem(std::move(item));
    pimpl->scrollToEdge(scrollTo,wasAtEdge);
}

//--------------------------------------------------------------------------
template <typename ItemT>
bool FlyweightListView<ItemT>::scrollToItem(const typename ItemT::IdType &id, size_t offset)
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
bool FlyweightListView<ItemT>::isScrollAtEdge(Direction direction, size_t maxOffset) const noexcept
{
    auto bar=pimpl->bar();
    return (direction==Direction::END||direction==Direction::STAY_AT_END_EDGE)
            ?
                ((bar->maximum()-bar->sliderPosition())<maxOffset)
            :
                ((bar->sliderPosition()-bar->minimum())<maxOffset);
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::scrollToEdge(Direction offsetDirection)
{
    pimpl->scrollToEdge(offsetDirection,true);
}

//--------------------------------------------------------------------------
template <typename ItemT>
bool FlyweightListView<ItemT>::eventFilter(QObject *watched, QEvent *event)
{
    if (watched==pimpl->m_scArea->viewport())
    {
        if (event->type()==QEvent::Resize)
        {
            pimpl->updateBeginEndWidgets();
        }
    }
    else if (watched==pimpl->m_scArea)
    {
        if (event->type()==QEvent::KeyPress)
        {
            auto e=dynamic_cast<QKeyEvent*>(event);
            if (e)
            {
                if (e->key()==Qt::Key_PageUp)
                {
                    pimpl->bar()->triggerAction(QScrollBar::SliderPageStepSub);
                }
                else if (e->key()==Qt::Key_PageDown)
                {
                    pimpl->bar()->triggerAction(QScrollBar::SliderPageStepAdd);
                }
                else if (e->key()==Qt::Key_Home && (e->modifiers() & Qt::ControlModifier))
                {
                    emit homeRequested();
                }
                else if (e->key()==Qt::Key_End && (e->modifiers() & Qt::ControlModifier))
                {
                    emit endRequested();
                }
            }
        }
    }

    return false;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_EMD
