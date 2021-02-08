/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

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
#include <string>
#include <functional>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/flyweightlistframe.hpp>

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
 *  @note Widgets must be centered in layout.
 */
template <typename ItemT>
class FlyweightListView : public QFrame
{
    public:

        constexpr static const size_t PrefetchItemCountHint=20;
        constexpr static const size_t DefaultPageStep=10;

        using RequestItemsCb=std::function<void (const ItemT*,size_t,Direction)>;
        using ItemRangeCb=std::function<void (const ItemT*,const ItemT*)>;
        using RequestJumpCb=std::function<void ()>;
        using ScrolledCb=std::function<void (int,int)>;
        using MinOrthogonalSizeChangedCb=std::function<void (int)>;

        explicit FlyweightListView(QWidget* parent, size_t prefetchItemCount=PrefetchItemCountHint);
        explicit FlyweightListView(size_t prefetchItemCount=PrefetchItemCountHint);

        ~FlyweightListView();

        FlyweightListView(const FlyweightListView&)=delete;
        FlyweightListView(FlyweightListView&&)=delete;
        FlyweightListView& operator=(const FlyweightListView&)=delete;
        FlyweightListView& operator=(FlyweightListView&&)=delete;

        void insertItem(ItemT item);
        void insertItems(const std::vector<ItemT>& items);
        void insertContinuousItems(const std::vector<ItemT>& items);
        void loadItems(const std::vector<ItemT>& items);

        void beginUpdate();
        void endUpdate();

        void removeItem(const typename ItemT::IdType& id);
        void removeItems(const std::vector<typename ItemT::IdType>& ids);

        void setPrefetchItemCountHint(size_t val) noexcept;
        size_t prefetchItemCount() const noexcept;
        size_t visibleItemCount() const noexcept;

        void setRequestItemsCb(RequestItemsCb cb) noexcept;
        void setViewportChangedCb(ItemRangeCb cb) noexcept;
        void setItemRangeChangedCb(ItemRangeCb cb) noexcept;

        void setRequestHomeCb(RequestJumpCb cb) noexcept;
        void setRequestEndCb(RequestJumpCb cb) noexcept;
        void setScrolledCb(ScrolledCb cb) noexcept;

        void setMinOrthogonalSizeChangedCb(MinOrthogonalSizeChangedCb cb) noexcept;

        bool scrollToItem(const typename ItemT::IdType& id, size_t offset=0);
        void scrollToEdge(Direction direction);
        bool isScrollAtEdge(Direction direction) const noexcept;

        void clear();

        size_t itemCount() const noexcept;
        bool hasItem(const typename ItemT::IdType& id) const noexcept;
        const ItemT* item(const typename ItemT::IdType& id) const noexcept;
        const ItemT* firstItem() const noexcept;
        const ItemT* lastItem() const noexcept;

        Qt::Orientation orientation() const noexcept;
        void setOrientation(Qt::Orientation orientation);

        void setFlyweightEnabled(bool enable) noexcept;
        bool isFlyweightEnabled() const noexcept;

        void setStickMode(Direction mode) noexcept;
        Direction stickMode() const noexcept;

        void scroll(int delta);

        void setSingleScrollStep(size_t value) noexcept;
        size_t singleScrollStep() const noexcept;

        void setMinPageScrollStep(size_t value) noexcept;
        size_t minPageScrollStep() const noexcept;

        size_t pageScrollStep() const noexcept;

        int minOrthogonalSize() const noexcept;
        int orthogonalPos() const noexcept;
        void setOrthogonalPos(int value);

        typename ItemT::SortValueType maxSortValue() const noexcept;
        void setMaxSortValue(const typename ItemT::SortValueType& value) noexcept;
        typename ItemT::SortValueType minSortValue() const noexcept;
        void setMinSortValue(const typename ItemT::SortValueType& value) noexcept;

        QScrollBar* verticalScrollBar() const noexcept;
        QScrollBar* horizontalScrollBar() const noexcept;

        void setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy policy);
        Qt::ScrollBarPolicy horizontalScrollBarPolicy() const noexcept;
        void setVerticalScrollBarPolicy(Qt::ScrollBarPolicy policy);
        Qt::ScrollBarPolicy verticalScrollBarPolicy() const noexcept;

    protected:

        void resizeEvent(QResizeEvent *event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void wheelEvent(QWheelEvent *event) override;

        bool eventFilter(QObject *watched, QEvent *event) override;

    private:

        std::unique_ptr<detail::FlyweightListView_p<ItemT>> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_FLYWEIGHTLISTVIEW_HPP
