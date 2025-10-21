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

template <typename ContentWidgetT,typename BaseT>
HTreeFlyweightListItem<ContentWidgetT,BaseT>::HTreeFlyweightListItem(HTreePathElement el, QWidget* parent)
    : HTreeListItemT<BaseT>(std::move(el),parent)
{    
    this->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

template <typename ContentWidgetT,typename BaseT>
void HTreeFlyweightListItem<ContentWidgetT,BaseT>::setItemWidgets(QWidget* icon, ContentWidgetT* content, int contentStretch, int nextStrech)
{
    if (m_contentWrapper)
    {
        destroyWidget(m_contentWrapper);
    }

    m_contentWrapper=new QFrame(this);
    this->setWidget(m_contentWrapper);
    m_layout=Layout::horizontal(m_contentWrapper);
    if (icon!=nullptr)
    {
        m_layout->addWidget(icon);
    }
    if (content!=nullptr)
    {
        m_content=content;
        if constexpr (std::is_base_of_v<WidgetBase,ContentWidgetT>)
        {
            m_layout->addWidget(content->qWidget(),contentStretch);
        }
        else
        {
            m_layout->addWidget(content,contentStretch);
        }
    }
    if (nextStrech!=0)
    {
        m_layout->addStretch(nextStrech);
    }
}

/************************* HTreeFlyweightListItemView ***********************/

template <typename ItemT, typename OrderComparer, typename IdComparer>
HTreeFlyweightListItemView<ItemT,OrderComparer,IdComparer>::HTreeFlyweightListItemView(
        QWidget* parent, OrderComparer orderComparer, IdComparer idComparer
    ) : Base(parent,std::move(orderComparer),std::move(idComparer))
{
    this->setSingleScrollStep(10);
}

/************************* HTreeStandardListView ***********************/

template <typename ItemT, typename BaseT, typename OrderComparer, typename IdComparer>
HTreeFlyweightListView<ItemT,BaseT,OrderComparer,IdComparer>::HTreeFlyweightListView(QWidget* parent,
                                                      OrderComparer orderComparer,
                                                      IdComparer idComparer,
                                                      int minimumWidth)
    : HTreeListFlyweightView<HTreeFlyweightListItemWrapper<ItemT>,BaseT,OrderComparer,IdComparer>()
{
    auto l=Layout::vertical(this);
    auto listView=new HTreeFlyweightListItemView<ItemT,OrderComparer,IdComparer>(this,std::move(orderComparer),std::move(idComparer));
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
