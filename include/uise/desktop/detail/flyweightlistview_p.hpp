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
                size_t prefetchItemCount
            ) : m_prefetchItemCount(prefetchItemCount),
                m_inserting(false),
                m_collapsed(false),
                m_singleScrollStep(1),
                m_pageScrollStep(100),
                m_orientation(Qt::Vertical)
        {
        }

        //-------------------------------------
        void insertItem(ItemT item)
        {

        }

    public:

        size_t m_prefetchItemCount;

        std::multiset<ItemT> m_items;

        bool m_inserting;
        bool m_collapsed;
        typename FlyweightListView<ItemT>::RequestItemsCb m_requestItemsBeforeCb;
        typename FlyweightListView<ItemT>::RequestItemsCb m_requestItemsAfterCb;

        size_t m_singleScrollStep;
        size_t m_pageScrollStep;

        Qt::Orientation m_orientation;
};

} // namespace detail

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_EMD
