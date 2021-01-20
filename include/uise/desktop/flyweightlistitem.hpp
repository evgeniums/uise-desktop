/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** \file uise/desktop/flyweightlistitem.hpp
*
*  Defines FlyweightListItem.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FLYWEIGHTLISTITEM_HPP
#define UISE_DESKTOP_FLYWEIGHTLISTITEM_HPP

#include <type_traits>
#include <string>
#include <functional>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/destroywidget.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Helper template for FlyweightListItem traits.
 */
template <typename ItemT, typename WidgetT, typename SortValueT=size_t, typename IdT=std::string>
struct FlyweightListItemTraits
{
    using ItemType=ItemT;
    using WidgetType=WidgetT;
    using SortValueType=SortValueT;
    using IdType=IdT;
};

namespace detail
{

template <typename T, typename Enable=void>
struct HasDropWidget
{
    constexpr static const bool value=false;
};

template <typename T>
struct HasDropWidget<T,
            std::enable_if_t<std::is_same<decltype(&T::dropWidget),decltype(&T::dropWidget)>::value>
        >
{
    constexpr static const bool value=true;
};

}

/**
 * @brief Base template class for items of FlyweightListView.
 *
 * This class wraps actual widget object. Rules how to handle wrapped object must be defined
 * in TraitsT class. At leaset, the following static methods must be defined:
 *
 *  - WidgetType* TraitsT::widget(ItemType&);
 *  - SortValueType TraitsT::sortValue(const ItemType&);
 *  - IdType TraitsT::id(const ItemType&);
 */
template <typename TraitsT>
class FlyweightListItem
{
    public:

        using ItemType=typename TraitsT::ItemType;
        using WidgetType=typename TraitsT::WidgetType;
        using SortValueType=typename TraitsT::SortValueType;
        using IdType=typename TraitsT::IdType;

        constexpr static const char* Property="uise_dt_FlyweightListItem";

        using ItemDeletionHandler=std::function<void ()>;

        /**
         * @brief Constructor.
         * @param item Item that will be copied into this object.
         */
        FlyweightListItem(
                const ItemType& item
            ) : m_item(item)
        {}

        /**
         * @brief Constructor.
         * @param item Item that will be moved into this object.
         */
        FlyweightListItem(
                ItemType&& item
            ) noexcept : m_item(std::move(item))
        {}

        /**
         * @brief Get item's widget.
         * @return Item's widget.
         */
        WidgetType* widget() noexcept
        {
            auto self=const_cast<const FlyweightListItem*>(this);
            return self->widget();
        }

        /**
         * @brief Get item's widget.
         * @return Item's widget.
         */
        WidgetType* widget() const noexcept
        {
            return TraitsT::widget(m_item);
        }

        /**
         * @brief Get item's sort value to be used for items ordering in list view.
         * @return Item's sort value.
         */
        SortValueType sortValue() const noexcept
        {
            return TraitsT::sortValue(m_item);
        }

        /**
         * @brief Get item's ID.
         * @return Item's ID.
         */
        IdType id() const
        {
            return TraitsT::id(m_item);
        }

        /**
         * @brief Get constant reference to wrapped item.
         * @return Wrapped item.
         */
        const ItemType& item() const noexcept
        {
            return m_item;
        }

        /**
         * @brief Get reference to wrapped item.
         * @return Wrapped item.
         */
        ItemType& item() noexcept
        {
            return m_item;
        }

        /**
         * @brief Drop item's widget.
         */
        void dropWidget()
        {
            dropWidgetHandler()(widget());
        }

        /**
         * @brief Drop some widget.
         * @param widget Widget to drop.
         */
        static void dropWidget(QWidget* widget)
        {
            dropWidgetHandler()(widget);
        }

        /**
         * @brief Get handler to be used for dropping widget drom view.
         * @return Handler.
         */
        constexpr static std::function<void (QWidget*)> dropWidgetHandler() noexcept
        {
            if constexpr (detail::HasDropWidget<TraitsT>::value)
            {
                return &TraitsT::dropWidget;
            }
            else
            {
                return destroyWidget;
            }
        }

        /**
         * @brief operator < to compare items for sorting.
         * @param left Left item.
         * @param right Right item.
         * @return Comparation result.
         */
        friend bool operator< (const FlyweightListItem& left, const FlyweightListItem& right) noexcept
        {
            return left.sortValue()<right.sortValue();
        }

    private:

        ItemType m_item;
};

UISE_DESKTOP_NAMESPACE_EMD

#endif // UISE_DESKTOP_FLYWEIGHTLISTITEM_HPP
