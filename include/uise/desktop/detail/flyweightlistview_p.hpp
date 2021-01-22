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
                size_t prefetchItemCountHint
            ) : m_view(view),
                m_prefetchItemCount(prefetchItemCountHint),
                m_updating(false),
                m_scArea(nullptr),
                m_llist(nullptr),
                m_qobjectHelper([this](QObject* obj){onWidgetDestroyed(obj);}),
                m_scrollValue(0),
                m_scrolledToEnd(true),
                m_waitingForResize(false),
                m_autoLoad(false),
                m_requestPending(false)
        {
        }

        //-------------------------------------
        void beginUpdate()
        {
            m_updating=true;
        }

        //-------------------------------------
        void endUpdate()
        {
            m_updating=false;
            clearRequestPending();
            handleUpdate();
        }

        //------------------------------------
        void handleUpdate()
        {
            m_updateTimer.shot(
                1,
                [this]()
                {
                    checkNeedsMoreItems();
                }
            );
        }

        //-------------------------------------
        size_t configureWidget(const ItemT* item)
        {
            auto widget=item->widget();
            PointerHolder::keepProperty(item,widget,ItemT::Property);
            QObject::connect(widget,SIGNAL(destroyed(QObject*)),&m_qobjectHelper,SLOT(onWidgetDestroyed(QObject*)));

            return isHorizontal()?widget->sizeHint().width():widget->sizeHint().height();
        }

        //-------------------------------------
        size_t prefetchItemCount() noexcept
        {
            return autoPrefetchCount();
        }

        //-------------------------------------
        size_t autoPrefetchCount() noexcept
        {
            m_prefetchItemCount=std::max(m_prefetchItemCount,visibleCount()*2);
            return m_prefetchItemCount;
        }

        //-------------------------------------
        size_t maxHiddenItemsBeyondEdge() noexcept
        {
            return prefetchItemCount()*2;
        }

        //-------------------------------------
        size_t prefetchThreshold() noexcept
        {
            return maxHiddenItemsBeyondEdge()/2;
        }

        //-------------------------------------
        void processPositions()
        {
            // find begin widget in viewport
            QPoint beginPos = QPoint(0,0) - m_scArea->widget()->pos();
            if (isHorizontal())
            {
                beginPos.setY(0);
            }
            else
            {
                beginPos.setX(0);
            }
            m_lastBeginWidget=m_scArea->widget()->childAt(beginPos);
            if (m_lastBeginWidget)
            {
                m_lastBeginWidget->setProperty(LastWidgetPosProperty,m_lastBeginWidget->pos());
            }

            // find end widget in viewport
            QSize viewportSize = m_scArea->viewport()->size();
            QPoint endPos(beginPos);
            if (isHorizontal())
            {
                endPos.setX(endPos.x()+viewportSize.width());
                endPos.setY(0);
            }
            else
            {
                endPos.setX(0);
                endPos.setY(endPos.y()+viewportSize.height());
            }
            m_lastEndWidget=m_scArea->widget()->childAt(endPos);

            // calculate numbers of hidden items out of the viewport
            size_t beginSeqPos=0;
            size_t endSeqPos=0;
            if (m_lastBeginWidget)
            {
                beginSeqPos=m_llist->widgetSeqPos(m_lastBeginWidget);
            }
            if (m_lastEndWidget)
            {
                endSeqPos=m_llist->widgetSeqPos(m_lastEndWidget);
            }
            m_hiddenBefore=beginSeqPos;
            m_hiddenAfter=(m_lastEndWidget==nullptr)?0:(m_items.template get<1>().size()-endSeqPos);

            qDebug() << "count ="<<count()
                     << "prefetchItemCount ="<<prefetchItemCount()
                     << "visibleCount ="<<visibleCount()
                     << "maxHiddenItemsBeyondEdge ="<<maxHiddenItemsBeyondEdge()
                     << "prefetchThreshold ="<<prefetchThreshold()
                     << "hiddenBefore ="<<m_hiddenBefore
                     << "hiddenAfter ="<<m_hiddenAfter
            ;
        }

        //-------------------------------------
        void onViewportUpdated()
        {
            // skip if in updating progress
            if (m_updating)
            {
                return;
            }

            // process begin/end positions
            processPositions();

            // notify application that viewport is updated
            if (m_viewportChangedCb)
            {
                auto itemAtBegin=PointerHolder::getProperty<ItemT*>(m_lastBeginWidget,ItemT::Property);
                auto itemAtEnd=PointerHolder::getProperty<ItemT*>(m_lastEndWidget,ItemT::Property);
                m_viewportChangedCb(itemAtBegin,itemAtEnd);
            }
        }

        //-------------------------------------
        size_t count() const noexcept
        {
            return m_items.template get<1>().size();
        }

        //-------------------------------------
        size_t visibleCount() const noexcept
        {
            auto all=count();
            auto lastHidden=m_hiddenBefore+m_hiddenAfter;

            auto visible=(lastHidden<all)?(all-lastHidden):0;

            return visible;
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

            handleUpdate();
        }

        size_t removeExtraHiddenItems(bool fromBegin)
        {
            size_t removedSize=0;
            if (fromBegin)
            {
                if (m_lastBeginWidget)
                {
                    auto maxHiddenCount=maxHiddenItemsBeyondEdge();
                    if (m_hiddenBefore>maxHiddenCount)
                    {
                        auto removeCount=m_hiddenBefore-maxHiddenCount;
                        removedSize=removeItemsFromBegin(removeCount);
                    }
                }
            }
            else
            {
                if (m_lastEndWidget)
                {
                    auto maxHiddenCount=maxHiddenItemsBeyondEdge();
                    if (m_hiddenAfter>maxHiddenCount)
                    {
                        auto removeCount=m_hiddenAfter-maxHiddenCount;
                        removedSize=removeItemsFromEnd(removeCount);
                    }
                }
            }

            return removedSize;
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

            processPositions();
            auto removedSize=removeExtraHiddenItems(m_scrolledToEnd);

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

        //-------------------------------------
        void onScrolled(int value)
        {
            qDebug() << "scrolled " <<value;

            if (!m_updating && !m_waitingForResize)
            {
                m_scrolledToEnd=value>m_scrollValue;
            }
            m_scrollValue=value;

            handleUpdate();
        }

        //-------------------------------------
        void checkNeedsItemsBefore()
        {
            if (m_scrolledToEnd)
            {
                return;
            }

            if ((m_hiddenBefore<prefetchThreshold() || count()==0) && m_requestItemsBeforeCb)
            {
                const ItemT* firstItem=nullptr;
                const auto& order=m_items.template get<0>();
                auto it=order.begin();
                if (it!=order.end())
                {
                    firstItem=&(*it);
                }

                m_requestPending=true;
                m_requestItemsBeforeCb(firstItem,prefetchItemCount());
            }
        }

        //-------------------------------------
        void checkNeedsItemsAfter()
        {
            if (!m_scrolledToEnd && !m_lastEndWidget)
            {
                return;
            }

            if ((m_hiddenAfter<prefetchThreshold() || !m_lastEndWidget ) && m_requestItemsAfterCb)
            {
                const ItemT* lastItem=nullptr;
                const auto& order=m_items.template get<0>();
                auto it=order.rbegin();
                if (it!=order.rend())
                {
                    lastItem=&(*it);
                }

                m_requestPending=true;
                m_requestItemsAfterCb(lastItem,prefetchItemCount());
            }
        }

        //-------------------------------------
        void checkNeedsMoreItems()
        {
            auto beginWidget=m_lastBeginWidget;
            auto endWidget=m_lastEndWidget;

            onViewportUpdated();

            bool updated=m_autoLoad || m_lastBeginWidget.get()!=beginWidget || m_lastEndWidget!=endWidget
                        || count()>0&&!m_lastEndWidget;
            if (updated && !m_requestPending)
            {
                checkNeedsItemsBefore();
                checkNeedsItemsAfter();
            }
        }

        //-------------------------------------
        QScrollBar* bar() const
        {
            return isHorizontal()?m_scArea->horizontalScrollBar():m_scArea->verticalScrollBar();
        }

        //-------------------------------------
        void scrollToEdge(Direction offsetDirection, bool wasAtEdge)
        {
            if (!wasAtEdge && (offsetDirection==Direction::STAY_AT_END_EDGE || offsetDirection==Direction::STAY_AT_HOME_EDGE))
            {
                return;
            }

            m_scrollToItemTimer.shot(
                10,
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

        //-------------------------------------
        bool scrollToItem(const typename ItemT::IdType &id, size_t offset)
        {
            auto& idx=m_items.template get<1>();
            auto it=idx.find(id);
            if (it==idx.end())
            {
                return false;
            }

            m_scrollToItemTimer.shot(10,
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

        //-------------------------------------
        bool hasItem(const typename ItemT::IdType& id) const noexcept
        {
            const auto& idx=m_items.template get<1>();
            auto it=idx.find(id);
            return it!=idx.end();
        }

        //-------------------------------------
        bool isHorizontal() const noexcept
        {
            return m_llist->orientation()==Qt::Horizontal;
        }

        //-------------------------------------
        bool isVertical() const noexcept
        {
            return !isHorizontal();
        }

        //-------------------------------------
        void onWidgetDestroyed(QObject* obj)
        {
            auto item=PointerHolder::getProperty<ItemT*>(obj,ItemT::Property);
            if (item)
            {
                auto& idx=m_items.template get<1>();
                idx.erase(item->id());
            }
        }

        //-------------------------------------
        void clearWidget(QWidget* widget)
        {
            PointerHolder::clearProperty(widget,ItemT::Property);
            m_llist->takeWidget(widget);
            ItemT::dropWidget(widget);
        }

        //-------------------------------------
        void removeItem(ItemT* item)
        {
            clearWidget(item->widget());

            auto& idx=m_items.template get<1>();
            idx.erase(item->id());
        }

        //-------------------------------------
        size_t removeItemsFromBegin(size_t count, size_t offset=0)
        {
            qDebug() << "Remove from begin "<<count<<"/"<<this->count();

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

        //-------------------------------------
        size_t removeItemsFromEnd(size_t count, size_t offset=0)
        {
            qDebug() << "Remove from end "<<count<<"/"<<this->count();

            if (count==0)
            {
                return 0;
            }

            size_t removedSize=0;

            auto& order=m_items.template get<0>();
            for (auto it=order.rbegin(), nit=it;it!=order.rend(); it=nit)
            {
                if (offset>0)
                {
                    --offset;
                    continue;
                }

                nit=std::next(it);

                removedSize+=isHorizontal()?it->widget()->sizeHint().width() : it->widget()->sizeHint().height();
                clearWidget(it->widget());

                nit = decltype(it){order.erase(std::next(it).base())};

                if (--count==0)
                {
                    break;
                }
            }

            return removedSize;
        }

        //-------------------------------------
        void onListResize()
        {
            if (m_updating)
            {
                return;
            }

            handleUpdate();
        }

        //-------------------------------------
        void clearRequestPending()
        {
            m_requestPendingClearTimer.shot(10,
                [this]()
                {
                    m_requestPending=false;
                    handleUpdate();
                }
            );
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
        SingleShotTimer m_resizeTimer;
        SingleShotTimer m_requestPendingClearTimer;

        QPointer<QWidget> m_lastBeginWidget;
        QPointer<QWidget> m_lastEndWidget;

        int m_scrollValue;
        bool m_scrolledToEnd;

        bool m_waitingForResize;

        size_t m_hiddenBefore, m_hiddenAfter;
        bool m_autoLoad;

        bool m_requestPending;
};

} // namespace detail

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_EMD
