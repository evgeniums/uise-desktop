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
                m_orientation(Qt::Vertical),
                m_canvas(nullptr),
                m_scArea(nullptr),
                m_canvasLayout(nullptr),
                m_blockUpdates(false)
        {
        }

        //-------------------------------------
        void insertItem(ItemT item)
        {

        }

        void setupLayout()
        {
            Q_ASSERT_X(m_items.empty(),"setup layout of flyweight list","layout can be changed only for empty lists");
            m_canvasLayout=Layout::box(m_canvas,m_orientation);
        }

        void setupUi()
        {
            auto layout=Layout::vertical(m_view);

            m_scArea=new QScrollArea(m_view);
            m_scArea->setObjectName("FlyweightListViewScArea");
            layout->addWidget(m_scArea,1);

            m_canvas=new QFrame(m_scArea);
            m_canvas->setObjectName("FlyweightListViewCanvas");
            m_scArea->setWidget(m_canvas);
            m_scArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            m_scArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

            setupLayout();
        }

        void removeAllItems()
        {
            m_blockUpdates=true;

            for (auto&& item : m_items)
            {
                auto widget=item.widget();
                if (widget)
                {
                    widget->hide();
                    widget->deleteLater();
                    auto deletionhandler=item.deletionHandler();
                    if (deletionhandler)
                    {
                        deletionhandler();
                    }
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

        Qt::Orientation m_orientation;

        QScrollArea* m_scArea;
        QFrame* m_canvas;
        QBoxLayout* m_canvasLayout;

        bool m_blockUpdates;
};

} // namespace detail

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_EMD
