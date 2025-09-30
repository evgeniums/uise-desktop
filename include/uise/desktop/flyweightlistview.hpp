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

class QScrollBar;

UISE_DESKTOP_NAMESPACE_BEGIN

enum class Direction : uint8_t
{
    NONE=0,
    HOME=1,
    END=2
};

namespace detail
{
template <typename ItemT>
class FlyweightListView_p;
}

/**
 * @brief Flyweight list of widgets.
 *
 *  Flyweight list view is intended for use with large number of list elements. The view loads and presents only a limited subset of items at a moment.
 *  Thus, it consumes less memory and provides better performance compared to using ordinary Qt layouts.
 *  The list can handle any types of widges and does not require connection to a Qt model in contrast with Qt Model-View-Controller and Graphics View frameworks.
 *
 *  @note Widget items must be centered in layout.
 */
template <typename ItemT>
class FlyweightListView : public QFrame
{
    public:

        inline static size_t PrefetchItemCountHint=20;
        inline static size_t DefaultPageStep=10;

        using RequestItemsCb=std::function<void (const ItemT*,size_t,Direction)>;
        using ItemRangeCb=std::function<void (const ItemT*,const ItemT*)>;
        using RequestJumpCb=std::function<void (Qt::KeyboardModifiers)>;

        using InsertItemCb=std::function<void (typename ItemT::WidgetType*)>;
        using RemoveItemCb=std::function<void (typename ItemT::WidgetType*)>;

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         * @param prefetchItemCountHint Hint for number of items above/before the viewport to prefetch in advance. See also setPrefetchItemCountHint().
         */
        explicit FlyweightListView(QWidget* parent, size_t prefetchItemCountHint=PrefetchItemCountHint);

        /**
         * @brief Default constructor.
         * @param prefetchItemCountHint Hint for number of items above/before the viewport to prefetch in advance. See also setPrefetchItemCountHint().
         */
        explicit FlyweightListView(size_t prefetchItemCount=PrefetchItemCountHint);

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
         *
         * Inserted items will be auto sorted.
         */
        void insertItems(const std::vector<ItemT>& items);

        void reorderItem(const ItemT& item);

        /**
         * @brief Insert continuous set of items into the list.
         * @param item Items to insert.
         *
         * Items must be pre-sorted before insertion.
         */
        void insertContinuousItems(const std::vector<ItemT>& items);

        /**
         * @brief Populate lists with the items.
         * @param items Items to load.
         *
         * The list is auto cleared before loading the items.
         */
        void loadItems(const std::vector<ItemT>& items);

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
        void setPrefetchItemCountHint(size_t val) noexcept;

        /**
         * @brief Get hint for number of items above/before the viewport to prefetch in advance.
         * @return Hint value.
         *
         * See also setPrefetchItemCountHint().
         */
        size_t prefetchItemCountHint() const noexcept;

        /**
         * @brief Get actual number of items above/before the viewport to prefetch in advance.
         * @return Actual value.
         *
         * See also setPrefetchItemCountHint().
         */
        size_t prefetchItemCount() const noexcept;

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
        void setRemoveItemCb(InsertItemCb cb) noexcept;

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

    protected:

        void resizeEvent(QResizeEvent *event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void wheelEvent(QWheelEvent *event) override;

        bool eventFilter(QObject *watched, QEvent *event) override;

    private:

        std::unique_ptr<detail::FlyweightListView_p<ItemT>> pimpl;
};

/**
 * @brief Widget containing FlyweightListView.
 */
template <typename ItemT, typename BaseT=QFrame>
class WithFlyweightListView : public BaseT
{
    public:

        using BaseT::BaseT;

        FlyweightListView<ItemT>* listView() const noexcept
        {
            return m_listView;
        }

        void reload()
        {
            doReload();
        }

    protected:

        void setListView(FlyweightListView<ItemT>* listView) noexcept
        {
            m_listView=listView;
        }

        virtual void doReload()=0;

    private:

        FlyweightListView<ItemT>* m_listView=nullptr;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_FLYWEIGHTLISTVIEW_HPP
