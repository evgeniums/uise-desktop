/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/desktop/detail/flyweightlistview_p.hpp
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
#include <QScrollBar>
#include <QPointer>
#include <QEvent>
#include <QResizeEvent>

#include <uise/desktop/utils/pointerholder.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/utils/orientationinvariant.hpp>

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

        void setListResizeHandler(std::function<void ()>&& handler)
        {
            listResizeHandler=std::move(handler);
        }

    public slots:

        void onWidgetDestroyed(QObject* obj)
        {
            widgetDestroyedHandler(obj);
        }

    protected:

        bool eventFilter(QObject *watched, QEvent *event) override
        {
            if (watched!=this)
            {
                switch (event->type())
                {
                    case QEvent::Resize:
                    {
#if 0
                        auto e=dynamic_cast<QResizeEvent*>(event);
                        qDebug() << "Widget resized "<<e;
#endif
                    }
                    [[fallthrough]];
                    case QEvent::Show: [[fallthrough]];
                    case QEvent::Hide:
                    {
                        listResizeHandler();
                    }
                        break;

                    default:
                        break;
                }
            }
            return false;
        }

    private:

        std::function<void (QObject*)> widgetDestroyedHandler;
        std::function<void ()> listResizeHandler;
};

template <typename ItemT>
class FlyweightListView_p : public OrientationInvariant
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

        void configureWidget(const ItemT* item);

        size_t prefetchItemCount() noexcept;

        size_t autoPrefetchCount() noexcept;

        size_t maxHiddenItemsBeyondEdge() noexcept;

        size_t prefetchThreshold() noexcept;

        size_t itemsCount() const noexcept;

        size_t visibleCount() const noexcept;

        void insertItem(const ItemT& item);
        QWidget* insertItemToContainer(const ItemT& item, bool findAfterWidget=true);

        void setFlyweightEnabled(bool enable) noexcept;

        bool isFlyweightEnabled() const noexcept;

        void insertContinuousItems(const std::vector<ItemT>& items);

        void clear();

        void scrollToEdge(Direction direction);

        bool scrollToItem(const typename ItemT::IdType &id, size_t offset);

        bool hasItem(const typename ItemT::IdType& id) const noexcept;
        const ItemT* item(const typename ItemT::IdType& id) const noexcept;
        const ItemT* firstItem() const noexcept;
        const ItemT* lastItem() const noexcept;

        bool isHorizontal() const noexcept override;

        void onWidgetDestroyed(QObject* obj);

        void onListContentResized();

        void onViewportResized(QResizeEvent* event);
        void onResized();

        void keepCurrentConfiguration();

        const ItemT* itemAtPos(const QPoint& pos) const;
        QPoint viewportBegin() const;
        QPoint listEndInViewport() const;
        QPoint listEndInViewport(const QSize& oldSize) const;

        const ItemT* firstViewportItem() const;
        const ItemT* lastViewportItem() const;

        bool isAtBegin() const;
        bool isAtEnd() const;
        int endItemEdge() const;

        void checkItemCount();
        void informViewportUpdated();

        void scroll(int delta);

        void wheelEvent(QWheelEvent *event);
        void updatePageStep();

        void viewportUpdated();

        void scrollTo(const std::function<int (int, int, int)>& cb);

        void resizeList();

        void clearWidget(QWidget* widget);
        void removeItem(ItemT* item);
        void removeItem(const typename ItemT::IdType& id);

        void setOrientation(Qt::Orientation orientation);

        void compensateSizeChange();
        void updateStickingPositions();

        const auto& itemOrder() const noexcept;
        auto& itemOrder() noexcept;
        const auto& itemIdx() const noexcept;
        auto& itemIdx() noexcept;

        void beginItemRangeChange() noexcept;
        void endItemRangeChange();

        void removeExtraItemsFromBegin(size_t count);
        void removeExtraItemsFromEnd(size_t count);

        void updateScrollBars();

        void onMainSbarChanged(int value);
        void onOtherSbarChanged(int value);

        void updateScrollBarOrientation();

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

        QFrame* m_obj;
        QScrollBar* m_vbar;
        QScrollBar* m_hbar;

        QFrame* m_view;
        size_t m_prefetchItemCount;
        size_t m_prefetchItemCountHint;

        ItemsContainer m_items;

        typename FlyweightListView<ItemT>::RequestItemsCb m_requestItemsCb;
        typename FlyweightListView<ItemT>::ItemRangeCb m_viewportChangedCb;
        typename FlyweightListView<ItemT>::ItemRangeCb m_itemRangeChangedCb;

        typename FlyweightListView<ItemT>::RequestJumpCb m_homeRequestCb;
        typename FlyweightListView<ItemT>::RequestJumpCb m_endRequestCb;

        LinkedListView* m_llist;

        FlyweightListView_q m_qobjectHelper;

        bool m_enableFlyweight;
        Direction m_stick;

        QSize m_listSize;
        QSize m_viewSize;

        typename ItemT::IdType m_firstViewportItemID;
        typename ItemT::SortValueType m_firstViewportSortValue;
        typename ItemT::IdType m_lastViewportItemID;
        typename ItemT::SortValueType m_lastViewportSortValue;
        bool m_atBegin;
        bool m_atEnd;
        int m_firstWidgetPos;
        int m_scrollValue;

        const ItemT* m_firstItem;
        const ItemT* m_lastItem;

        size_t m_singleStep;
        size_t m_pageStep;
        size_t m_minPageStep;

        float m_wheelOffsetAccumulated;
        float m_wheelOffsetAccumulatedOther;

        bool m_ignoreUpdates;
        SingleShotTimer m_updateStickingPositionsTimer;
        SingleShotTimer m_resizeListTimer;
        SingleShotTimer m_checkItemCountTimer;

        bool m_cleared;

        typename ItemT::SortValueType m_maxSortValue;
        typename ItemT::SortValueType m_minSortValue;

        Qt::ScrollBarPolicy m_vbarPolicy;
        Qt::ScrollBarPolicy m_hbarPolicy;

        SingleShotTimer m_scrollBarsTimer;

        std::function<void (int)> m_vScrollCb;
        std::function<void (int)> m_hScrollCb;

        bool m_scrollWheelHorizontal;
};

} // namespace detail

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_FLYWEIGHTLISTVIEW_P_HPP
