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

/** @file uise/desktop/Ratio.hpp
*
*  Declares item of flyweight list in htree node
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_FLYWEIGHT_LIST_ITEM_HPP
#define UISE_DESKTOP_HTREE_FLYWEIGHT_LIST_ITEM_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/flyweightlistview.ipp>
#include <uise/desktop/flyweightlistitem.hpp>

#include <uise/desktop/htreelist.hpp>
#include <uise/desktop/htreelistitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT HTreeFlyweightListItem : public HTreeListItem
{
    Q_OBJECT

    public:

        HTreeFlyweightListItem(HTreePathElement el, QWidget* parent=nullptr);

        HTreeFlyweightListItem(QWidget* parent=nullptr) : HTreeFlyweightListItem(HTreePathElement{},parent)
        {}

        void setItemWidgets(QWidget* icon, QWidget* content, int contentStretch=0, int nextStrech=0);

        std::string itemName() const
        {
            return pathElement().name();
        }

        auto itemSortValue() const
        {
            return pathElement().name();
        }

        std::string itemType() const
        {
            return pathElement().type();
        }

        auto itemId() const
        {
            return uniqueId();
        }

    private:

        QHBoxLayout* m_layout;
};

template <typename ItemT=HTreeFlyweightListItem>
struct HTreeFlyweightListItemTraits : public FlyweightListItemTraits<ItemT*,ItemT,std::string,std::string>
{
    static auto sortValue(const ItemT* item) noexcept -> decltype(auto)
    {
        return item->itemSortValue();
    }

    static ItemT* widget(ItemT* item) noexcept
    {
        return item;
    }

    static auto id(const HTreeFlyweightListItem* item) -> decltype(auto)
    {
        return item->itemId();
    }
};

template <typename ItemT=HTreeFlyweightListItem>
using HTreeFlyweightListItemWrapper=FlyweightListItem<HTreeFlyweightListItemTraits<ItemT>>;

template <typename ItemT=HTreeFlyweightListItem>
class HTreeFlyweightListItemView : public FlyweightListView<HTreeFlyweightListItemWrapper<ItemT>>
{
    public:

    HTreeFlyweightListItemView(QWidget* parent=nullptr);
};

template <typename ItemT=HTreeFlyweightListItem>
class HTreeFlyweightListView : public HTreeListFlyweightView<HTreeFlyweightListItemWrapper<ItemT>>
{
    public:

        constexpr static const int DefaultMinimumWidth=300;

        HTreeFlyweightListView(QWidget* parent=nullptr, int minimumWidth=DefaultMinimumWidth);

        HTreeFlyweightListView(int minimumWidth) : HTreeFlyweightListView(nullptr,minimumWidth)
        {}
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_FLYWEIGHT_LIST_ITEM_HPP
