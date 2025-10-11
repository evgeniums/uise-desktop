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
#include <uise/desktop/ipp/flyweightlistview.ipp>
#include <uise/desktop/flyweightlistitem.hpp>

#include <uise/desktop/htreelist.hpp>
#include <uise/desktop/htreelistitemtemplate.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

template <typename ContentWidgetT=QWidget, typename BaseT=QFrame>
class HTreeFlyweightListItem : public HTreeListItemT<BaseT>
{
    public:

        using Content=ContentWidgetT;

        HTreeFlyweightListItem(HTreePathElement el, QWidget* parent=nullptr);

        HTreeFlyweightListItem(QWidget* parent=nullptr) : HTreeFlyweightListItem(HTreePathElement{},parent)
        {}

        void setItemWidgets(QWidget* icon, ContentWidgetT* content, int contentStretch=0, int nextStrech=0);

        std::string itemName() const
        {
            return this->pathElement().name();
        }

        auto itemSortValue() const
        {
            return this->pathElement().name();
        }

        std::string itemType() const
        {
            return this->pathElement().type();
        }

        auto itemId() const
        {
            return this->uniqueId();
        }

    private:

        QFrame* m_contentWrapper=nullptr;
        QHBoxLayout* m_layout=nullptr;
};

template <typename ItemT=HTreeFlyweightListItem<>>
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

    static auto id(const ItemT* item) -> decltype(auto)
    {
        return item->itemId();
    }
};

template <typename ItemT=HTreeFlyweightListItem<>>
using HTreeFlyweightListItemWrapper=FlyweightListItem<HTreeFlyweightListItemTraits<ItemT>>;

template <typename ItemT=HTreeFlyweightListItem<>, typename OrderComparer=ComparerWithOrder, typename IdComparer=ComparerWithOrder>
class HTreeFlyweightListItemView : public FlyweightListView<HTreeFlyweightListItemWrapper<ItemT>,OrderComparer,IdComparer>
{
    public:

        using Base=FlyweightListView<HTreeFlyweightListItemWrapper<ItemT>,OrderComparer,IdComparer>;

        HTreeFlyweightListItemView(QWidget* parent=nullptr, OrderComparer orderComparer=OrderComparer{}, IdComparer idComparer=IdComparer{});
};

template <typename ItemT=HTreeFlyweightListItem<>, typename BaseT=QFrame, typename OrderComparer=ComparerWithOrder, typename IdComparer=ComparerWithOrder>
class HTreeFlyweightListView : public HTreeListFlyweightView<HTreeFlyweightListItemWrapper<ItemT>,BaseT,OrderComparer,IdComparer>
{
    public:

        constexpr static const int DefaultMinimumWidth=300;

        HTreeFlyweightListView(QWidget* parent=nullptr,
                               OrderComparer orderComparer=OrderComparer{},
                               IdComparer idComparer=IdComparer{},
                               int minimumWidth=DefaultMinimumWidth);

        HTreeFlyweightListView(QWidget* parent, int minimumWidth)
            : HTreeFlyweightListView(parent,OrderComparer{},IdComparer{},minimumWidth)
        {}

        HTreeFlyweightListView(int minimumWidth) : HTreeFlyweightListView(nullptr,minimumWidth)
        {}
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_FLYWEIGHT_LIST_ITEM_HPP
