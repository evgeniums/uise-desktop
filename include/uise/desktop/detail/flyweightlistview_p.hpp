/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** \file uise/desktop/detail/flyweightlistview_p.hpp
*
*  Contains declaration of FlyweightListView_p.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FLYWEIGHTLISTVIEW_P_HPP
#define UISE_DESKTOP_FLYWEIGHTLISTVIEW_P_HPP

#include <functional>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>

#include <QDebug>
#include <QScrollArea>
#include <QScrollBar>
#include <QPointer>

#include <uise/desktop/utils/pointerholder.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/linkedlistview.hpp>
#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/flyweightlistitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

namespace detail {

class UISE_DESKTOP_EXPORT FlyweightListView_q : public QObject
{
    Q_OBJECT

    public:

        void setWidgetDestroyedHandler(std::function<void (QObject*)>&& handler)
        {
            widgetDestroyedHandler=std::move(handler);
        }

        void setListResizedHandler(std::function<void ()>&& handler)
        {
            listResizedHandler=std::move(handler);
        }

    public slots:

        void onWidgetDestroyed(QObject* obj)
        {
            widgetDestroyedHandler(obj);
        }

        void onListResized()
        {
            listResizedHandler();
        }

    private:

        std::function<void (QObject*)> widgetDestroyedHandler;
        std::function<void ()> listResizedHandler;
};

enum class OProp : uint8_t
{
    size,
    pos,
    edge
};

template <typename ItemT>
class FlyweightListView_p
{
    public:

        constexpr static const char* LastWidgetPosProperty="uise_dt_FlyweightListItemPos";

        FlyweightListView_p(
            FlyweightListView<ItemT>* view,
            size_t prefetchItemCountHint
        );

        void setupUi();

        void beginUpdate();

        void endUpdate();

        size_t configureWidget(const ItemT* item);

        size_t prefetchItemCount() noexcept;

        size_t autoPrefetchCount() noexcept;

        size_t maxHiddenItemsBeyondEdge() noexcept;

        size_t prefetchThreshold() noexcept;

        size_t itemsCount() const noexcept;

        size_t visibleCount() const noexcept;

        void insertItem(const ItemT& item);

        void setFlyweightEnabled(bool enable) noexcept;

        bool isFlyweightEnabled() const noexcept;

        void insertContinuousItems(const std::vector<ItemT>& items);

        void removeAllItems();

        void scrollToEdge(Direction offsetDirection, bool wasAtEdge);

        bool scrollToItem(const typename ItemT::IdType &id, size_t offset);

        bool hasItem(const typename ItemT::IdType& id) const noexcept;

        const ItemT* firstItem() const noexcept;

        const ItemT* lastItem() const noexcept;

        bool isHorizontal() const noexcept;

        bool isVertical() const noexcept;

        void onWidgetDestroyed(QObject* obj);

        void onListResize();

        void onListUpdated();

        void onViewportResized(QResizeEvent* event);

        void keepCurrentConfiguration();

        const ItemT* itemAtPos(const QPoint& pos) const;
        QPoint viewportBegin() const;
        QPoint viewportEnd() const;
        QPoint viewportEnd(const QSize& oldSize) const;

        const ItemT* firstViewportItem() const;
        const ItemT* lastViewportItem() const;

        bool isAtBegin() const;
        bool isAtEnd() const;
        int endItemEdge() const;

        void checkNewItemsNeeded();
        void informViewportUpdated();

        void scroll(int delta);

        void wheelEvent(QWheelEvent *event);
        void updatePageStep();

        void viewportUpdated();

        template <typename T>
        int oprop(const T& obj, OProp prop, bool other = false) const noexcept;

        template <typename T>
        void setOProp(T& obj, OProp prop, int value, bool other = false) noexcept;
        template <typename T>
        void setOProp(T& obj, OProp prop, int value, bool other = false) const noexcept;

    public:

        using ItemsContainer=boost::multi_index::multi_index_container
            <
                ItemT,
                boost::multi_index::indexed_by<
                    boost::multi_index::ordered_non_unique<boost::multi_index::const_mem_fun<
                                        ItemT,
                                        typename ItemT::SortValueType,
                                        &ItemT::sortValue
                                    >>
                    ,
                    boost::multi_index::ordered_unique<boost::multi_index::const_mem_fun<
                                        ItemT,
                                        typename ItemT::IdType,
                                        &ItemT::id
                                    >>
                >
            >;

        FlyweightListView<ItemT>* m_view;
        size_t m_prefetchItemCount;

        ItemsContainer m_items;

        typename FlyweightListView<ItemT>::RequestItemsCb m_requestItemsBeforeCb;
        typename FlyweightListView<ItemT>::RequestItemsCb m_requestItemsAfterCb;
        typename FlyweightListView<ItemT>::ViewportChangedCb m_viewportChangedCb;
        typename FlyweightListView<ItemT>::RequestJumpCb m_homeRequestCb;
        typename FlyweightListView<ItemT>::RequestJumpCb m_endRequestCb;

        LinkedListView* m_llist;

        FlyweightListView_q m_qobjectHelper;

        int m_scrollValue;

        size_t m_hiddenBefore, m_hiddenAfter;

        bool m_enableFlyweight;
        Direction m_stick;

        QSize m_listSize;
        QSize m_viewSize;

        typename ItemT::IdType m_firstViewportItemID;
        typename ItemT::SortValueType m_firstViewportSortValue;
        typename ItemT::IdType m_lastViewportItemID;
        typename ItemT::SortValueType m_lastViewportSortValue;

        size_t m_singleStep;
        size_t m_pageStep;
        size_t m_minPageStep;

        float m_wheelOffsetAccumulated;
};

} // namespace detail

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_EMD

#endif // UISE_DESKTOP_FLYWEIGHTLISTVIEW_P_HPP
