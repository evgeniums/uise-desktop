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

#include <set>

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
        void insertItem(ItemT item)
        {
            // remove item by ID
            // insert by sort value
            // get after/before neighbour from iterator
            // insert to linked list view
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

        FlyweightListView<ItemT>* m_view;
        size_t m_prefetchItemCount;

        std::multiset<ItemT> m_items;

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
