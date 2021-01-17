/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** \file uise/desktop/flyweightlistview.hpp
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

UISE_DESKTOP_NAMESPACE_BEGIN

enum class Direction : uint8_t
{
    NONE=0,
    HOME=1,
    END=2,
    STAY_AT_HOME_EDGE=3,
    STAY_AT_END_EDGE=4
};

class FlyweightListFrame : public QFrame
{
    Q_OBJECT

    public:

        using QFrame::QFrame;

    signals:

        void homeRequested();
        void endRequested();
};

namespace detail
{
template <typename ItemT>
class FlyweightListView_p;
}

/**
 * @brief Flyweight list of widgets.
 */
template <typename ItemT>
class FlyweightListView : public FlyweightListFrame
{
    public:

        constexpr static const size_t PrefetchItemCount=32;

        using RequestItemsCb=std::function<void (const ItemT*,size_t)>;

        explicit FlyweightListView(QWidget* parent, size_t prefetchItemCount=PrefetchItemCount);
        explicit FlyweightListView(size_t prefetchItemCount=PrefetchItemCount);

        void loadItems(const std::vector<ItemT>& items, Direction scrollTo=Direction::NONE);
        void insertItems(const std::vector<ItemT>& items, Direction scrollTo=Direction::NONE);
        void insertItem(ItemT item, Direction scrollTo=Direction::NONE);

        void beginInserting() noexcept;
        void endInserting();

        void setPrefetchItemCount(size_t val) noexcept;
        size_t prefetchItemCount() const noexcept;

        void setRequestItemsBeforeCb(RequestItemsCb cb) noexcept;
        void setRequestItemsAfterCb(RequestItemsCb cb) noexcept;

        void scroll(int64_t delta);
        void scrollTo(const typename ItemT::IdType& id, size_t offset=0, Direction offsetDirection=Direction::END);
        void scrollToEdge(Direction offsetDirection);

        void collapse(size_t animationDurationMs=0, Direction offsetDirection=Direction::HOME);
        void expand(size_t animationDurationMs=0);
        bool isCollapsed() const noexcept;

        void clear();
        void resort();
        void reload();

        size_t itemCount() const noexcept;

        Qt::Orientation orientation() const noexcept;
        void setOrientation(Qt::Orientation orientation) noexcept;

        void setSingleScrollStep(size_t step) noexcept;
        size_t singleScrollStep() const noexcept;
        void setPageScrollStep(size_t step) noexcept;
        size_t pageScrollStep() const noexcept;

    protected:

//        bool eventFilter(QObject *watched, QEvent *event) override;
//        void keyPressEvent(QKeyEvent *event) override;
//        void keyReleaseEvent(QKeyEvent *event) override;
//        void resizeEvent(QResizeEvent *event) override;
//        void wheelEvent(QWheelEvent *event) override;

    private:

        std::unique_ptr<detail::FlyweightListView_p<ItemT>> pimpl;
};

UISE_DESKTOP_NAMESPACE_EMD

#endif // UISE_DESKTOP_FLYWEIGHTLISTVIEW_HPP
