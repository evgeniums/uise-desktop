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
    setFocusPolicy(Qt::ClickFocus);
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
    pimpl->removeAllItems();
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::reload()
{
    pimpl->m_autoLoad=true;
    clear();
    pimpl->checkNeedsMoreItems();
    pimpl->m_autoLoad=false;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::setOrientation(Qt::Orientation orientation) noexcept
{
    pimpl->m_llist->setOrientation(orientation);    
    pimpl->updatePageStep();

    //! @todo Maybe forbid changing orintation when list is not empty?
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
    insertContinuousItems(items,scrollTo);
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
    if (items.empty())
    {
        return;
    }

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
const ItemT* FlyweightListView<ItemT>::firstItem() const noexcept
{
    return pimpl->firstItem();
}

//--------------------------------------------------------------------------
template <typename ItemT>
const ItemT* FlyweightListView<ItemT>::lastItem() const noexcept
{
    return pimpl->lastItem();
}

//--------------------------------------------------------------------------
template <typename ItemT>
bool FlyweightListView<ItemT>::isScrollAtEdge(Direction direction, size_t maxOffset) const noexcept
{
    //! @todo Implement isScrollAtEdge
    return false;
}

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::scrollToEdge(Direction offsetDirection)
{
    pimpl->scrollToEdge(offsetDirection,true);
}

#if 0
    if (event->type()==QEvent::KeyPress)
    {
        auto e=dynamic_cast<QKeyEvent*>(event);
        if (e)
        {
            qDebug() << "Key pressed "<<e;

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
                qDebug() << "Key Ctrl+Home pressed ";

                if (pimpl->m_homeRequestCb)
                {
                    pimpl->m_homeRequestCb();
                }
            }
            else if (e->key()==Qt::Key_End && (e->modifiers() & Qt::ControlModifier))
            {
                qDebug() << "Key Ctrl+End pressed ";

                if (pimpl->m_endRequestCb)
                {
                    pimpl->m_endRequestCb();
                }
            }
        }
    }
#endif

//--------------------------------------------------------------------------
template <typename ItemT>
void FlyweightListView<ItemT>::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    pimpl->onViewportResized(event);
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
    return pimpl->m_stick=mode;
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
    else if (event->key()==Qt::Key_Home && (event->modifiers() & Qt::ControlModifier))
    {
        if (pimpl->m_homeRequestCb)
        {
            pimpl->m_homeRequestCb();
        }
    }
    else if (event->key()==Qt::Key_End && (event->modifiers() & Qt::ControlModifier))
    {
        if (pimpl->m_endRequestCb)
        {
            pimpl->m_endRequestCb();
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

UISE_DESKTOP_NAMESPACE_EMD
