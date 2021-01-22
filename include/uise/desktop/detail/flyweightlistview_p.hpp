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
            std::function<void (QObject*)> widgetDestroyedHandler
        ) : widgetDestroyedHandler(std::move(widgetDestroyedHandler))
        {
        }

    public slots:

        void onWidgetDestroyed(QObject* obj)
        {
            widgetDestroyedHandler(obj);
        }

    private:

        std::function<void (QObject*)> widgetDestroyedHandler;
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
                m_maxCachedCount(prefetchItemCount*2),
                m_updating(false),
                m_scArea(nullptr),
                m_llist(nullptr),
                m_qobjectHelper([this](QObject* obj){onWidgetDestroyed(obj);}),
                m_scrollValue(0),
                m_scrolledToEnd(true),
                m_waitingForResize(false)
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
                1,
                [this]()
                {
                    updateBeginEndWidgets();
                }
            );
        }

        size_t configureWidget(const ItemT* item)
        {
            auto widget=item->widget();
            PointerHolder::keepProperty(item,widget,ItemT::Property);
            QObject::connect(widget,SIGNAL(destroyed(QObject*)),&m_qobjectHelper,SLOT(onWidgetDestroyed(QObject*)));

            return isHorizontal()?widget->sizeHint().width():widget->sizeHint().height();
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

        void updateScrollPosition()
        {

        }

        //-------------------------------------
        void insertContinuousItems(const std::vector<ItemT>& items)
        {
            if (items.empty())
            {
                return;
            }

            m_waitingForResize=true;
            auto oldSize=m_llist->size();

            auto& idx=m_items.template get<1>();
            auto& order=m_items.template get<0>();

            size_t insertedSize=0;
            std::vector<QWidget*> widgets;
            QPointer<QWidget> afterWidget;
            for (size_t i=0;i<items.size();i++)
            {
                const auto& item=items[i];
                auto result=idx.insert(item);
                if (!result.second)
                {
                    Q_ASSERT(idx.replace(result.first,item));
                }
                insertedSize+=configureWidget(&(*result.first));

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

            qDebug() << "Add "<<items.size()<< " " << afterWidget;

            m_llist->insertWidgetsAfter(widgets,afterWidget);

            size_t removedSize=0;

            if (m_scrolledToEnd)
            {
                QPoint p = QPoint(0,0) - m_scArea->widget()->pos();
                if (isHorizontal())
                {
                    p.setY(0);
                }
                else
                {
                    p.setX(0);
                }
                auto beginWidget=m_scArea->widget()->childAt(p);
                if (beginWidget)
                {
                    auto beginSeqPos=m_llist->widgetSeqPos(beginWidget);
                    if (beginSeqPos>m_maxCachedCount)
                    {
                        auto count=beginSeqPos-m_maxCachedCount;
                        if (count==items.size())
                        {
                            --count;
                        }

                        qDebug() << "Remove from begin "<<count<<"/"<<order.size();
                        removedSize=removeItemsFromBegin(count);
                    }
                }
            }
            else
            {
                QPoint p = QPoint(0,0) - m_scArea->widget()->pos();
                if (isHorizontal())
                {
                    p.setY(0);
                }
                else
                {
                    p.setX(0);
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
                if (endWidget)
                {
                    auto endSeqPos=m_llist->widgetSeqPos(endWidget);
                    size_t hiddenCount=idx.size()-endSeqPos;
                    if (hiddenCount>m_maxCachedCount)
                    {
                        auto count=hiddenCount-m_maxCachedCount;
                        if (count==items.size())
                        {
                            --count;
                        }

                        qDebug() << "Remove from end "<<count<<"/"<<order.size();
                        removeItemsFromEnd(count);
                    }
                }
            }

            m_resizeTimer.shot(2,
                [this,removedSize,insertedSize,oldSize]()
                {
                    auto newSize=m_llist->size();
                    auto sbar=bar();
                    int offset=0;
                    if (m_scrolledToEnd)
                    {
                        offset=removedSize;
                    }
                    else
                    {
                        offset=-insertedSize;
                    }
                    int newValue=sbar->value()-offset;

                    qDebug() << "removed size "<<removedSize<<" inserted size "<<insertedSize<< " offset "<<offset
                             << "old position "<<sbar->value() << "new position "<<newValue
                             << "old size "<<oldSize << " new size "<<newSize;

                    if (offset!=0)
                    {
                        sbar->setValue(newValue);
                    }

                    m_waitingForResize=false;
                }
            );
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
            qDebug() << "scrolled " <<value;

            if (!m_updating && !m_waitingForResize)
            {
                m_scrolledToEnd=value>m_scrollValue;
            }
            m_scrollValue=value;

            informUpdate();
        }

        void removeExtraItems(bool end, size_t count, size_t offset=0)
        {
            if (end)
            {
                const auto& order=m_items.template get<0>();
                qDebug() << "Remove from end "<<count<<"/"<<order.size();
                removeItemsFromEnd(count,offset);
            }
            else
            {
                const auto& order=m_items.template get<0>();
                qDebug() << "Remove from beginning "<<count<<"/"<<order.size();
                removeItemsFromBegin(count,offset);
            }
        }

        void processHiddenBefore(size_t hiddenBefore, size_t hiddenAfter)
        {
            if (m_scrolledToEnd)
            {
                return;
            }

            const auto& order=m_items.template get<0>();
            auto checkCount=m_prefetchItemCount/2;//+order.size()-hiddenAfter;

            qDebug() << "processHiddenAfter hiddenBegore="<<hiddenBefore
                     << " hiddenAfter="<<hiddenAfter
                     << " checkCount="<<checkCount;

            if (hiddenBefore<checkCount)
            {
                if (m_requestItemsBeforeCb)
                {
                    const ItemT* firstItem=nullptr;
                    auto it=order.begin();
                    if (it!=order.end())
                    {
                        firstItem=&(*it);
                    }

                    auto prefetchCount=m_prefetchItemCount;//+order.size()-hiddenAfter;
                    m_requestItemsBeforeCb(firstItem,prefetchCount);
                }
            }
        }

        void processHiddenAfter(size_t hiddenBefore, size_t hiddenAfter)
        {
            if (!m_scrolledToEnd)
            {
                return;
            }

            const auto& order=m_items.template get<0>();

            auto checkCount=m_prefetchItemCount/2;//+order.size()-hiddenBefore;
            qDebug() << "processHiddenAfter hiddenBegore="<<hiddenBefore
                     << " hiddenAfter="<<hiddenAfter
                     << " checkCount="<<checkCount;
            if (hiddenAfter<checkCount)
            {
                if (m_requestItemsAfterCb)
                {
                    const ItemT* lastItem=nullptr;
                    auto it=order.rbegin();
                    if (it!=order.rend())
                    {
                        lastItem=&(*it);
                    }

                    auto prefetchCount=m_prefetchItemCount;//+order.size()-hiddenBefore;
                    m_requestItemsAfterCb(lastItem,prefetchCount);
                }
            }
        }

        void processHiddenItems(size_t hiddenBefore, size_t hiddenAfter)
        {
            processHiddenBefore(hiddenBefore,hiddenAfter);
            processHiddenAfter(hiddenBefore,hiddenAfter);
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

            if (inform)
            {
                if (m_viewportChangedCb)
                {
                    m_viewportChangedCb(itemAtBegin,itemAtEnd);
                }

                size_t beginSeqPos=0;
                size_t endSeqPos=0;

                if (beginWidget)
                {
                    beginSeqPos=m_llist->widgetSeqPos(beginWidget);
                }
                if (endWidget)
                {
                    endSeqPos=m_llist->widgetSeqPos(endWidget);
                }

                if (m_lastBeginWidget && m_lastEndWidget)
                qDebug() << "updateBeginEndWidgets count="<<m_items.template get<0>().size()
                         << "m_lastBeginWidget->pos()="<<m_lastBeginWidget->pos()
                         << "m_llist->pos()=" << m_llist->pos() << "m_llist->size()=" << m_llist->size()
                         << "m_scArea->viewport()->pos()=" << m_scArea->viewport()->pos()
                         << "itemAtBegin=" << itemAtBegin->sortValue()
                         << "p="<<p
                         << "itemAtEnd=" << itemAtEnd->sortValue()
                         << "p2="<<p2;

                auto hiddenBefore=beginSeqPos;
                auto hiddenAfter=(endWidget==nullptr)?0:(m_items.template get<1>().size()-endSeqPos);
                processHiddenItems(hiddenBefore,hiddenAfter);
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

        void onWidgetDestroyed(QObject* obj)
        {
            auto item=PointerHolder::getProperty<ItemT*>(obj,ItemT::Property);
            if (item)
            {
                auto& idx=m_items.template get<1>();
                idx.erase(item->id());
            }
        }

        void clearWidget(QWidget* widget)
        {
            PointerHolder::clearProperty(widget,ItemT::Property);
            m_llist->takeWidget(widget);
            ItemT::dropWidget(widget);
        }

        void removeItem(ItemT* item)
        {
            clearWidget(item->widget());

            auto& idx=m_items.template get<1>();
            idx.erase(item->id());
        }

        size_t removeItemsFromBegin(size_t count, size_t offset=0)
        {
            if (count==0)
            {
                return 0;
            }

            size_t removedSize=0;

            auto& order=m_items.template get<0>();
            for (auto it=order.begin();it!=order.end();)
            {
                if (offset>0)
                {
                    --offset;
                    continue;
                }

                removedSize+=isHorizontal()?it->widget()->sizeHint().width() : it->widget()->sizeHint().height();

                clearWidget(it->widget());
                it=order.erase(it);

                if (--count==0)
                {
                    break;
                }
            }

            return removedSize;
        }

        void removeItemsFromEnd(size_t count, size_t offset=0)
        {
            if (count==0)
            {
                return;
            }

            auto& order=m_items.template get<0>();
            for (auto it=order.rbegin(), nit=it;it!=order.rend(); it=nit)
            {
                if (offset>0)
                {
                    --offset;
                    continue;
                }

                nit=std::next(it);

                clearWidget(it->widget());

                nit = decltype(it){order.erase(std::next(it).base())};

                if (--count==0)
                {
                    break;
                }
            }
        }

        void onListResize()
        {
            return ;

            QPoint pos(0,0);
            QPoint lastPos(0,0);

            if (m_lastBeginWidget && m_lastBeginWidget->parent()==m_llist)
            {
                pos=m_lastBeginWidget->pos();
                lastPos=m_lastBeginWidget->property(LastWidgetPosProperty).toPoint();

                qDebug() << "onListResize m_lastBeginWidget->pos()="<<pos
                         << "lastPos="<<lastPos
                         << "m_llist->pos()=" << m_llist->pos() << "m_llist->size()=" << m_llist->size()
                         << "m_scArea->viewport()->pos()=" << m_scArea->viewport()->pos();
            }

            if (lastPos!=QPoint(0,0))
            {
//                int offset=isHorizontal()?(lastPos.x()-pos.x()):(lastPos.y()-pos.y());
//                auto sbar=bar();
//                offset=sbar->value()-offset;
//                sbar->setValue(offset);

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
        size_t m_maxCachedCount;

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
        SingleShotTimer m_resizeTimer;

        QPointer<QWidget> m_lastBeginWidget;
        QPointer<QWidget> m_lastEndWidget;

        int m_scrollValue;
        bool m_scrolledToEnd;

        bool m_waitingForResize;
};

} // namespace detail

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_EMD
