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

/** @file uise/desktop/flyweightlistview.hpp
*
*  Defines FlyWeightListView.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FLYWEIGHTLISTVIEW_HPP
#define UISE_DESKTOP_FLYWEIGHTLISTVIEW_HPP

#include <vector>
#include <memory>
#include <functional>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/enums.hpp>

class QScrollBar;

UISE_DESKTOP_NAMESPACE_BEGIN

class JumpEdge;

namespace detail
{
    template <typename ItemT, typename OrderComparer, typename IdComparer>
    class FlyweightListView_p;
}

struct ComparerWithOrder
{
    ComparerWithOrder(Order order=Order::ASC) : order(order)
    {}

    template <typename T1, typename T2>
    bool operator()(const T1& l, const T2& r) const
    {
        if (order==Order::ASC)
        {
            return std::less<>{}(l,r);
        }
        if (order==Order::DESC)
        {
            return std::less<>{}(r,l);
        }

        return false;
    }

    Order order;
};

enum class FlyweightListViewAlignment : int
{
    Center,
    Begin,
    End
};

/**
 * @brief Flyweight list of widgets.
 *
 *  Flyweight list view is intended for use with large number of list elements. The view loads and presents only a limited subset of items at a moment.
 *  Thus, it consumes less memory and provides better performance compared to using ordinary Qt layouts.
 *  The list can handle any types of widges and does not require connection to a Qt model in contrast with Qt Model-View-Controller and Graphics View frameworks.
 *
 *  @note Widget items must be centered in layout.
 */
template <typename ItemT, typename OrderComparer=ComparerWithOrder, typename IdComparer=ComparerWithOrder>
class FlyweightListView : public QFrame
{
    public:

        inline static size_t PrefetchItemWindowHint=20;
        inline static double PrefetchScreensCountHint=2.0;
        inline static double PrefetchThresholdRatio=0.75;
        inline static size_t DefaultPageStep=10;

        inline static size_t DefaultJumpEdgeXOffset=10;
        inline static size_t DefaultJumpEdgeYOffset=10;
        inline static size_t DefaultJumpInvisibleItemCount=3;

        using RequestItemsCb=std::function<void (const ItemT*,size_t,Direction)>;
        using ItemRangeCb=std::function<void (const ItemT*,const ItemT*)>;
        using RequestJumpCb=std::function<void (bool,Qt::KeyboardModifiers)>;

        using InsertItemCb=std::function<void (ItemT*)>;
        using RemoveItemCb=std::function<void (typename ItemT::WidgetType*)>;

        using EachItemHandler=std::function<bool (const ItemT*)>;

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         * @param prefetchItemWindowHint Hint for number of items above/before the viewport to prefetch in advance. See also setPrefetchItemWindowHint().
         */
        explicit FlyweightListView(QWidget* parent, size_t prefetchItemWindowHint=PrefetchItemWindowHint, OrderComparer orderComparer={}, IdComparer idComparer={});

        /**
         * @brief Default constructor.
         * @param prefetchItemWindowHint Hint for number of items above/before the viewport to prefetch in advance. See also setPrefetchItemWindowHint().
         */
        explicit FlyweightListView(size_t prefetchItemWindow=PrefetchItemWindowHint, OrderComparer orderComparer={}, IdComparer idComparer={});

        explicit FlyweightListView(QWidget* parent, OrderComparer orderComparer, IdComparer idComparer={}, size_t prefetchItemWindowHint=PrefetchItemWindowHint)
            : FlyweightListView(parent,prefetchItemWindowHint,std::move(orderComparer),std::move(idComparer))
        {}

        //! Destructor.
        ~FlyweightListView();

        FlyweightListView(const FlyweightListView&)=delete;
        FlyweightListView(FlyweightListView&&)=delete;
        FlyweightListView& operator=(const FlyweightListView&)=delete;
        FlyweightListView& operator=(FlyweightListView&&)=delete;

        /**
         * @brief Insert an item into the list.
         * @param item Item to insert.
         *
         * This method must be called within beginUpdate() and endUpdate() section.
         * Inserted item will be auto sorted.
         */
        void insertItem(const ItemT& item);

        /**
         * @brief Insert items into the list.
         * @param item Items to insert.
         * @param autoUpdate The list it auto updated when items are inserted.
         *
         * Inserted items will be auto sorted.
         */
        void insertItems(const std::vector<ItemT>& items, bool autoUpdate);

        void reorderItem(const ItemT& item);

        void reorderItem(const typename ItemT::IdType& id)
        {
            const auto* itm=item(id);
            if (itm!=nullptr)
            {
                reorderItem(*itm);
            }
        }

        /**
         * @brief Insert continuous set of items into the list.
         * @param item Items to insert.
         * @param autoUpdate The list it auto updated when items are inserted.
         *
         * Items must be pre-sorted before insertion.
         */
        void insertContinuousItems(const std::vector<ItemT>& items, bool autoUpdate=true);

        /**
         * @brief Populate lists with the items.
         * @param items Items to load.
         * @param autoUpdate The list it auto updated when items are loaded.
         *
         * The list is auto cleared before loading the items.
         */
        void loadItems(const std::vector<ItemT>& items, bool autoUpdate=true);

        /**
         * @brief Begin list updating.
         *
         * This method must be called before single item operations such as insertItem() and removeItem().
         */
        void beginUpdate();

        /**
         * @brief End list updating.
         *
         * This method must close section opened with beginUpdate().
         */
        void endUpdate();

        /**
         * @brief Remove single item from the list.
         * @param id Item ID.
         *
         *  This method must be called within beginUpdate() and endUpdate() section.
         */
        void removeItem(const typename ItemT::IdType& id);

        /**
         * @brief Remove multiple items from the list.
         * @param ids IDs of the items to remove.
         */
        void removeItems(const std::vector<typename ItemT::IdType>& ids);

        /**
         * @brief Set hint for number of items above/before the viewport to prefetch in advance.
         * @param val Value to set.
         *
         * This is a hint for number of items to prefetch. Hint is used as a default value for number of prefetched items.
         * Actual number of prefetched number is calculated dynamically and depends on sizes of item widgets and the view.
         */
        void setPrefetchItemWindowHint(size_t val) noexcept;

        /**
         * @brief Get hint for number of items above/before the viewport to prefetch in advance.
         * @return Hint value.
         *
         * See also setPrefetchItemWindowHint().
         */
        size_t prefetchItemWindowHint() const noexcept;

        /**
         * @brief Get actual number of items above/before the viewport to prefetch in advance.
         * @return Actual value.
         *
         * See also setPrefetchItemWindowHint().
         */
        size_t prefetchItemWindow() const noexcept;

        /**
         * @brief Get number of items in viewport.
         * @return Actual value.
         */
        size_t visibleItemCount() const noexcept;

        /**
         * @brief Get maximum number of hidden items before/after the viewport.
         * @return Actual value.
         */
        size_t maxHiddenItemCount() const noexcept;

        /**
         * @brief Set callback function used to request items for inserting into the view.
         * @param cb Callback function.
         */
        void setRequestItemsCb(RequestItemsCb cb) noexcept;

        /**
         * @brief Set callback function used to report that viewport state was changed.
         * @param cb Callback function.
         */
        void setViewportChangedCb(ItemRangeCb cb) noexcept;

        /**
         * @brief Set callback function used to report that range of items loaded to the view was changed.
         * @param cb Callback function.
         */
        void setItemRangeChangedCb(ItemRangeCb cb) noexcept;

        /**
         * @brief Set callback function used to report that user requested jumping to beginning of the view.
         * @param cb Callback function.
         */
        void setRequestHomeCb(RequestJumpCb cb) noexcept;

        /**
         * @brief Set callback function used to report that user requested jumping to end of the view.
         * @param cb Callback function.
         */
        void setRequestEndCb(RequestJumpCb cb) noexcept;

        /**
         * @brief Set callback function used to notify listener that an item was inserted into the list.
         * @param cb Callback function.
         */
        void setInsertItemCb(InsertItemCb cb) noexcept;

        /**
         * @brief Set callback function used to notify listener that an item was removed from the list.
         * @param cb Callback function.
         */
        void setRemoveItemCb(RemoveItemCb cb) noexcept;

        /**
         * @brief Scroll to an item.
         * @param id Id of the item.
         * @param offset Offest in pixels from the item's edge to scroll to.
         * @return True if item exists in the view, false otherwise.
         */
        bool scrollToItem(const typename ItemT::IdType& id, int offset=0);

        /**
         * @brief Scroll to beginning or end of the view.
         * @param direction Dericetion to scroll.
         */
        void scrollToEdge(Direction direction);

        /**
         * @brief Check if the view is currently at at the view's edge.
         * @param direction Direction to check.
         * @return True if the view is at the requested edge.
         */
        bool isScrollAtEdge(Direction direction) const noexcept;

        /**
         * @brief Enable or disable horisontal scrolling with the mouse wheel.
         * @param enable Enable id true, disabke otherwise.
         */
        void setWheelHorizontalScrollEnabled(bool enable) noexcept;

        /**
         * @brief Check if horisontal scrolling with the mouse wheel is enabled.
         * @return Treu if enabled, false otherwise.
         */
        bool isWheelHorizontaScrollEnabled() const noexcept;

        /**
         * @brief Clear the view.
         */
        void clear();

        /**
         * @brief Get number of items currently loaded into the view.
         * @return Actual value.
         */
        size_t itemCount() const noexcept;

        /**
         * @brief Check if the view contains an item with certain ID.
         * @param id ID of the item.
         * @return Query result.
         */
        bool hasItem(const typename ItemT::IdType& id) const noexcept;

        /**
         * @brief Find item by ID.
         * @param id ID of the item.
         * @return Found item or nullptr if the view does not contain the item.
         */
        const ItemT* item(const typename ItemT::IdType& id) const noexcept;

        /**
         * @brief Get the first item in the view.
         * @return First item or nullptr if the view is empty.
         */
        const ItemT* firstItem() const noexcept;

        /**
         * @brief Get the last item in the view.
         * @return Last item or nullptr if the view is empty.
         */
        const ItemT* lastItem() const noexcept;

        /**
         * @brief Get the first visible item in the viewport.
         * @return First visible item or nullptr if the view is empty.
         */
        const ItemT* firstViewportItem() const noexcept;

        /**
         * @brief Get the lastvisible item in the viewport.
         * @return Last visible item or nullptr if the view is empty.
         */
        const ItemT* lastViewportItem() const noexcept;

        /**
         * @brief Get orientation of the view.
         * @return Query result.
         */
        Qt::Orientation orientation() const noexcept;

        /**
         * @brief Set orientation of the view.
         * @param orientation Value to set.
         */
        void setOrientation(Qt::Orientation orientation);

        /**
         * @brief Check if view is of horizontal orientation.
         * @return Query result.
         */
        bool isHorizontal() const noexcept
        {
            return orientation()==Qt::Horizontal;
        }

        /**
         * @brief Check if view is of vertical orientation.
         * @return Query result.
         */
        bool isVertical() const noexcept
        {
            return orientation()==Qt::Vertical;
        }

        /**
         * @brief Set flyweight mode enabled (default).
         * @param enable Mode to set.
         *
         * If flyweight mode is disabled then the view keeps all loaded items "as is" and dynamical items requesting/removing is disabled.
         */
        void setFlyweightEnabled(bool enable) noexcept;

        /**
         * @brief Check if flyweight mode is enabled.
         * @return Query result.
         */
        bool isFlyweightEnabled() const noexcept;

        /**
         * @brief Set stick mode.
         * @param mode Direction for sticking. If Direction::NONE then sticking is disabled.
         *
         * Sticking means that after items inserting or removing the viewport will be automatically scrolled to the first/last item loaded to the view.
         * Note that automatic scrolling is invoked only if the view was already scrolled to the corresponding edge before updating.
         */
        void setStickMode(Direction mode) noexcept;

        /**
         * @brief Get stick mode.
         * @return Query mode.
         */
        Direction stickMode() const noexcept;

        /**
         * @brief Scroll the viewport by delta pixels.
         * @param delta Pixels to scroll by.
         */
        void scroll(int delta);

        /**
         * @brief Set number of pixels to scroll by as single scrolling step when arrow keys are pressed by user.
         * @param value Value to set.
         */
        void setSingleScrollStep(size_t value) noexcept;

        /**
         * @brief Get single scrolling step.
         * @return Query result.
         *
         * See elso setSingleScrollStep().
         */
        size_t singleScrollStep() const noexcept;

        /**
         * @brief Set minimal number of pixels to scroll by as page scrolling step when PgUp/PgDown keys are pressed by user.
         * @param value Value to set.
         *
         * Actual page scrolling step is calculated dynamically depending on size of the view and item widgets.
         */
        void setMinPageScrollStep(size_t value) noexcept;

        /**
         * @brief Get page scrolling step.
         * @return Query value.
         *
         * See also setMinPageScrollStep().
         */
        size_t minPageScrollStep() const noexcept;

        /**
         * @brief Get actual page scroll step.
         * @return Query value.
         *
         * See also setMinPageScrollStep().
         */
        size_t pageScrollStep() const noexcept;

        /**
         * @brief Set maximal item's sorting value.
         * @param value Value to set.
         *
         * Item's sorting value ItemT::sortValue() is used for automatical items sorting after inserting to the view.
         * For correct sorting the min and max sorting values must be set before populating the view with items.
         */
        void setMaxSortValue(const typename ItemT::SortValueType& value) noexcept;

        /**
         * @brief Get maximal item's sorting value.
         * @return Query result.
         *
         * See also setMaxSortValue().
         */
        typename ItemT::SortValueType maxSortValue() const noexcept;

        /**
         * @brief Set minimal item's sorting value.
         * @param value Value to set.
         *
         * See also setMaxSortValue().
         */
        void setMinSortValue(const typename ItemT::SortValueType& value) noexcept;

        /**
         * @brief Get minimal item's sorting value.
         * @return Query result.
         *
         * See also setMaxSortValue().
         */
        typename ItemT::SortValueType minSortValue() const noexcept;

        /**
         * @brief Get vertical scrollbar.
         * @return Query result.
         */
        QScrollBar* verticalScrollBar() const noexcept;

        /**
         * @brief Get horizontal scrollbar.
         * @return Query result.
         */
        QScrollBar* horizontalScrollBar() const noexcept;

        /**
         * @brief Set policy for horizontal scrollbar.
         * @param policy Value to set.
         */
        void setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy policy);

        /**
         * @brief Get policy for horizontal scrollbar.
         * @return Query result.
         */
        Qt::ScrollBarPolicy horizontalScrollBarPolicy() const noexcept;

        /**
         * @brief Set policy for vertical scrollbar.
         * @param policy Value to set.
         */
        void setVerticalScrollBarPolicy(Qt::ScrollBarPolicy policy);

        /**
         * @brief Get policy for vertical scrollbar.
         * @return Query result.
         */
        Qt::ScrollBarPolicy verticalScrollBarPolicy() const noexcept;

        /**
         * @brief Get viewport size.
         * @return Query value.
         */
        QSize viewportSize() const noexcept;

        void resetCallbacks();

        void setSortOrder(Order order) noexcept;
        Order sortOrder() const noexcept;

        void setPrefetchScreensCount(double value);
        double prefetchScreensCount() const noexcept;

        void setPrefetchThresholdRatio(double value);
        double prefetchThresholdRatio() const noexcept;

        void setPrefetchItemCount(size_t value);
        void resetPrefetchItemCount();
        size_t prefetchItemCount() const noexcept;
        size_t prefetchItemCountAuto() noexcept;
        size_t prefetchItemCountEffective() noexcept;

        void setJumpEdgeControlEnabled(bool value);
        bool isJumpEdgeControlEnabled() const;

        void setJumpEdgeInvisibleItemCount(size_t value);
        size_t jumpEdgeInvisibleItemCount() const;

        JumpEdge* jumpEdgeControl() const;

        bool eachItem(EachItemHandler handler);
        bool rEachItem(EachItemHandler handler);

        void setItemsAlignment(FlyweightListViewAlignment value) noexcept;
        FlyweightListViewAlignment itemsAlignment() const noexcept;

    protected:

        void resizeEvent(QResizeEvent *event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void wheelEvent(QWheelEvent *event) override;

        bool eventFilter(QObject *watched, QEvent *event) override;

    private:

        std::unique_ptr<detail::FlyweightListView_p<ItemT,OrderComparer,IdComparer>> pimpl;
};

/**
 * @brief Widget containing FlyweightListView.
 */
template <typename ItemT, typename BaseT=QFrame, typename OrderComparer=ComparerWithOrder, typename IdComparer=ComparerWithOrder>
class WithFlyweightListView : public BaseT
{
    public:

        using ListView=FlyweightListView<ItemT,OrderComparer,IdComparer>;

        using BaseT::BaseT;

        ListView* listView() const noexcept
        {
            return m_listView;
        }

        void reload()
        {
            doReload();
        }

    protected:

        void setListView(ListView* listView) noexcept
        {
            m_listView=listView;
        }

        virtual void doReload()=0;

    private:

        ListView* m_listView=nullptr;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_FLYWEIGHTLISTVIEW_HPP
