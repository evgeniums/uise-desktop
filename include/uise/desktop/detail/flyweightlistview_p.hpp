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

#include <functional>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>

#include <QDebug>
#include <QScrollArea>
#include <QScrollBar>
#include <QPointer>

#include <uise/desktop/utils/pointerholder.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/linkedlistview.hpp>
#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/flyweightlistitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

namespace detail {

class UISE_DESKTOP_EXPORT FlyweightListView_q : public QObject
{
    Q_OBJECT

    public:

        explicit FlyweightListView_q(
            std::function<void (QObject*)> itemDestroyedHandler
        ) : itemDestroyedHandler(std::move(itemDestroyedHandler))
        {
        }

    public slots:

        void onItemDestroyed(QObject* obj)
        {
            itemDestroyedHandler(obj);
        }

    private:

        std::function<void (QObject*)> itemDestroyedHandler;
};

template <typename ItemT>
class FlyweightListView_p
{
    public:

        constexpr static const char* LastWidgetPosProperty="uise_dt_FlyweightListItemPos";

        //-------------------------------------
        FlyweightListView_p(
                FlyweightListView<ItemT>* view,
                size_t prefetchItemCount
            ) : m_view(view),
                m_prefetchItemCount(prefetchItemCount),
                m_updating(false),
                m_scArea(nullptr),
                m_llist(nullptr),
                m_qobjectHelper([this](QObject* obj){onItemDestroyed(obj);})
        {
        }


        void beginUpdate()
        {
            m_updating=true;
        }

        void endUpdate()
        {
            if (m_updating)
            {
                m_updating=false;
                informUpdate();
            }
        }

        void informUpdate()
        {
            m_updateTimer.shot(
                0,
                [this]()
                {
                    updateBeginEndWidgets();
                }
            );
        }

        void configureWidget(const ItemT* item)
        {
            auto widget=item->widget();
            PointerHolder::keepProperty(item,widget,ItemT::Property);
            QObject::connect(widget,SIGNAL(destroyed(QObject*)),&m_qobjectHelper,SLOT(onItemDestroyed(QObject*)));
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
            configureWidget(&(*result.first));

            QWidget* afterWidget=nullptr;
            auto it=m_items.template project<0>(result.first);
            if (it!=order.begin())
            {
                afterWidget=(--it)->widget();
            }

            m_llist->insertWidgetAfter(widget,afterWidget);

            informUpdate();
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
                if (!result.second)
                {
                    Q_ASSERT(idx.replace(result.first,item));
                }
                configureWidget(&(*result.first));

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

        //-------------------------------------
        void setupUi()
        {
            auto layout=Layout::vertical(m_view);

            m_scArea=new QScrollArea(m_view);
            m_scArea->setObjectName("FlyweightListViewScArea");
            layout->addWidget(m_scArea,1);

            m_llist=new LinkedListView(m_scArea);
            m_llist->setObjectName("FlyweightListViewLList");
            m_llist->installEventFilter(m_view);

            m_scArea->setWidget(m_llist);
            m_scArea->setWidgetResizable(true);
            m_scArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            m_scArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            m_scArea->installEventFilter(m_view);
            m_scArea->viewport()->installEventFilter(m_view);

            QObject::connect(m_scArea->verticalScrollBar(),&QScrollBar::valueChanged,
                             [this](int value)
                             {
                                if (isHorizontal())
                                {
                                    return;
                                }

                                onScrolled(value);
                            });
            QObject::connect(m_scArea->horizontalScrollBar(),&QScrollBar::valueChanged,
                             [this](int value)
                             {
                                if (isVertical())
                                {
                                    return;
                                }

                                onScrolled(value);
                            });
        }

        //-------------------------------------
        void removeAllItems()
        {
            beginUpdate();

            m_llist->blockSignals(true);
            m_llist->clear(ItemT::dropWidgetHandler());
            m_llist->blockSignals(false);

            m_items.clear();

            endUpdate();
        }

        void onScrolled(int value)
        {
            std::ignore=value;
            informUpdate();
        }

        void updateBeginEndWidgets()
        {
            if (m_updating)
            {
                return;
            }

            QPoint p = QPoint(0,0) - m_scArea->widget()->pos();
            p.setX(0);
            auto beginWidget=m_scArea->widget()->childAt(p);
            auto itemAtBegin=PointerHolder::getProperty<ItemT*>(beginWidget,ItemT::Property);
            if (beginWidget)
            {
                beginWidget->setProperty(LastWidgetPosProperty,beginWidget->pos());
            }

            QSize s = m_scArea->viewport()->size();
            QPoint p2(p);
            if (isHorizontal())
            {
                p2.setX(p2.x()+s.width());
                p2.setY(0);
            }
            else
            {
                p2.setX(0);
                p2.setY(p2.y()+s.height());
            }
            auto endWidget=m_scArea->widget()->childAt(p2);
            auto itemAtEnd=PointerHolder::getProperty<ItemT*>(endWidget,ItemT::Property);

            bool inform=m_lastBeginWidget.get()!=beginWidget || m_lastEndWidget!=endWidget;
            m_lastBeginWidget=beginWidget;
            m_lastEndWidget=endWidget;

            if (inform && m_viewportChangedCb)
            {
                m_viewportChangedCb(itemAtBegin,itemAtEnd);
            }
        }

        QScrollBar* bar() const
        {
            return isHorizontal()?m_scArea->horizontalScrollBar():m_scArea->verticalScrollBar();
        }

        void scrollToEdge(Direction offsetDirection, bool wasAtEdge)
        {
            if (!wasAtEdge && (offsetDirection==Direction::STAY_AT_END_EDGE || offsetDirection==Direction::STAY_AT_HOME_EDGE))
            {
                return;
            }

            m_scrollToItemTimer.shot(
                0,
                [this,offsetDirection]
                {
                    switch (offsetDirection)
                    {
                        case Direction::END:
                        case Direction::STAY_AT_END_EDGE:
                            bar()->triggerAction(QScrollBar::SliderToMaximum);
                            break;

                        case Direction::HOME:
                        case Direction::STAY_AT_HOME_EDGE:
                            bar()->triggerAction(QScrollBar::SliderToMinimum);
                            break;

                        default:
                            break;
                    }
                }
            );
        }

        bool scrollToItem(const typename ItemT::IdType &id, size_t offset)
        {
            auto& idx=m_items.template get<1>();
            auto it=idx.find(id);
            if (it==idx.end())
            {
                return false;
            }

            m_scrollToItemTimer.shot(0,
                [this,offset,widget{QPointer<QWidget>(it->widget())}]()
                {
                    if (widget && widget->parent()==m_llist)
                    {
                        auto offs=offset;
                        auto pos=widget->pos();
                        if (isHorizontal())
                        {
                            offs+=pos.x();
                        }
                        else
                        {
                            offs+=pos.y();
                        }

                        bar()->setValue(offs);
                    }
                }
            );

            return true;
        }

        bool hasItem(const typename ItemT::IdType& id) const noexcept
        {
            const auto& idx=m_items.template get<1>();
            auto it=idx.find(id);
            return it!=idx.end();
        }

        bool isHorizontal() const noexcept
        {
            return m_llist->orientation()==Qt::Horizontal;
        }

        bool isVertical() const noexcept
        {
            return !isHorizontal();
        }

        void onItemDestroyed(QObject* obj)
        {
            auto item=PointerHolder::getProperty<ItemT*>(obj,ItemT::Property);
            if (item)
            {
                auto& idx=m_items.template get<1>();
                idx.erase(item->id());
            }
        }

        void onListResize()
        {
            QPoint pos(0,0);
            QPoint lastPos(0,0);

            if (m_lastBeginWidget && m_lastBeginWidget->parent()==m_llist)
            {
                pos=m_lastBeginWidget->pos();
                lastPos=m_lastBeginWidget->property(LastWidgetPosProperty).toPoint();
            }
            if (lastPos!=QPoint(0,0))
            {
                int offset=isHorizontal()?(lastPos.x()-pos.x()):(lastPos.y()-pos.y());

                auto sbar=bar();
                offset=sbar->value()-offset;
                if (offset>0)
                {
                    sbar->setValue(offset);
                }

                informUpdate();
            }
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

        bool m_updating;
        typename FlyweightListView<ItemT>::RequestItemsCb m_requestItemsBeforeCb;
        typename FlyweightListView<ItemT>::RequestItemsCb m_requestItemsAfterCb;
        typename FlyweightListView<ItemT>::ViewportChangedCb m_viewportChangedCb;

        QScrollArea* m_scArea;
        LinkedListView* m_llist;

        FlyweightListView_q m_qobjectHelper;
        SingleShotTimer m_scrollToItemTimer;
        SingleShotTimer m_updateTimer;

        QPointer<QWidget> m_lastBeginWidget;
        QPointer<QWidget> m_lastEndWidget;
};

} // namespace detail

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_EMD
