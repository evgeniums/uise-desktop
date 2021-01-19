/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** \file uise/desktop/detail/flyweightlistview_p.hpp
*
*  Contains detail implementation of FlyweightListView_p.
*
*/

/****************************************************************************/

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>

#include <QScrollArea>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/linkedlistview.hpp>
#include <uise/desktop/flyweightlistview.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

namespace detail {

template <typename ItemT>
class FlyweightListView_p
{
    public:

        //-------------------------------------
        FlyweightListView_p(
                FlyweightListView<ItemT>* view,
                size_t prefetchItemCount
            ) : m_view(view),
                m_prefetchItemCount(prefetchItemCount),
                m_inserting(false),
                m_scArea(nullptr),
                m_llist(nullptr),
                m_blockUpdates(false)
        {
        }

        //-------------------------------------
        void insertItem(ItemT&& item)
        {
            auto& idx=m_items.template get<1>();
            auto& order=m_items.template get<0>();

            auto widget=item.widget();
            auto result=idx.insert(item);
            if (!result.second)
            {
                Q_ASSERT(idx.replace(result.first,item));
            }
            QWidget* afterWidget=nullptr;
            auto it=m_items.template project<0>(result.first);
            if (it!=order.begin())
            {
                afterWidget=(--it)->widget();
            }

            m_llist->insertWidgetAfter(widget,afterWidget);
        }

        //-------------------------------------
        void insertContinuousItems(const std::vector<ItemT>& items)
        {
            auto& idx=m_items.template get<1>();
            auto& order=m_items.template get<0>();

            std::vector<QWidget*> widgets;
            QWidget* afterWidget=nullptr;
            for (size_t i=0;i<items.size();i++)
            {
                const auto& item=items[i];
                auto result=idx.insert(item);
                if (i==0)
                {
                    auto it=m_items.template project<0>(result.first);
                    if (it!=order.begin())
                    {
                        afterWidget=(--it)->widget();
                    }
                }
                widgets.push_back(item.widget());
            }

            m_llist->insertWidgetsAfter(widgets,afterWidget);
        }

        void setupUi()
        {
            auto layout=Layout::vertical(m_view);

            m_scArea=new QScrollArea(m_view);
            m_scArea->setObjectName("FlyweightListViewScArea");
            layout->addWidget(m_scArea,1);

            m_llist=new LinkedListView(m_scArea);
            m_llist->setObjectName("FlyweightListViewLList");
            m_scArea->setWidget(m_llist);
            m_scArea->setWidgetResizable(true);
            m_scArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            m_scArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        }

        void removeAllItems()
        {
            m_blockUpdates=true;

            m_llist->blockSignals(true);
            m_llist->clear();
            m_llist->blockSignals(false);

            for (auto&& item : m_items)
            {
                auto deletionhandler=item.deletionHandler();
                if (deletionhandler)
                {
                    deletionhandler();
                }
            }
            m_items.clear();

            m_blockUpdates=false;
        }

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

        FlyweightListView<ItemT>* m_view;
        size_t m_prefetchItemCount;

        ItemsContainer m_items;

        bool m_inserting;
        typename FlyweightListView<ItemT>::RequestItemsCb m_requestItemsBeforeCb;
        typename FlyweightListView<ItemT>::RequestItemsCb m_requestItemsAfterCb;

        QScrollArea* m_scArea;
        LinkedListView* m_llist;

        bool m_blockUpdates;
};

} // namespace detail

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_EMD
