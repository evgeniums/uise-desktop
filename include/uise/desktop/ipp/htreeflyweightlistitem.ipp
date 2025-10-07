/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/ipp/htreeflyweightlistitem.ipp
*
*  Defines HTreeFlyWeighListItem.
*
*/

/****************************************************************************/

#include <uise/desktop/htreeflyweightlistitem.hpp>
#include <uise/desktop/ipp/flyweightlistview.ipp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************* HTreeFlyweightListItem ******************************/

//--------------------------------------------------------------------------

template <typename BaseT>
HTreeFlyweightListItem<BaseT>::HTreeFlyweightListItem(HTreePathElement el, QWidget* parent)
    : HTreeListItemT<BaseT>(std::move(el),parent)
{
    m_layout=Layout::horizontal(this);
    this->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

template <typename BaseT>
void HTreeFlyweightListItem<BaseT>::setItemWidgets(QWidget* icon, QWidget* content, int contentStretch, int nextStrech)
{
    m_layout->addWidget(icon);
    m_layout->addWidget(content,contentStretch);
    if (nextStrech!=0)
    {
        m_layout->addStretch(nextStrech);
    }
}

/************************* HTreeFlyweightListItemView ***********************/

template <typename ItemT, typename OrderComparer, typename IdComparer>
HTreeFlyweightListItemView<ItemT,OrderComparer,IdComparer>::HTreeFlyweightListItemView(
        QWidget* parent, OrderComparer orderComparer, IdComparer idComparer
    ) : Base(parent,orderComparer,idComparer)
{
    this->setSingleScrollStep(10);
}

/************************* HTreeStandardListView ***********************/

template <typename ItemT>
HTreeFlyweightListView<ItemT>::HTreeFlyweightListView(QWidget* parent, int minimumWidth)
    : HTreeListFlyweightView<HTreeFlyweightListItemWrapper<ItemT>>()
{
    auto l=Layout::vertical(this);
    auto listView=new HTreeFlyweightListItemView<ItemT>(this);
    l->addWidget(listView);

    listView->setFlyweightEnabled(false);
    listView->setStickMode(Direction::HOME);
    listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listView->setWheelHorizontalScrollEnabled(false);
    this->setListView(listView);

    this->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

    this->setMinimumWidth(minimumWidth);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
