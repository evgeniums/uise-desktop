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

/** @file uise/test/flyweightlistview/fwlvtestwidget.hpp
*
*  Test application of FlyweightListView.
*
*/

/****************************************************************************/

#include <fwlvtestwidget.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------
FwlvTestWidget::FwlvTestWidget(
        QWidget *parent
    ) : QFrame(parent),
        pimpl(std::make_unique<FwlvTestWidget_p>())
{
    HelloWorldItem::HelloWorldItemId=1000000;
    setup();
}

//--------------------------------------------------------------------------
void FwlvTestWidget::loadItems()
{
    pimpl->view->setMaxSortValue(pimpl->items.size()-1);
    pimpl->view->setMinSortValue(0);

    std::vector<HelloWorldItemWrapper> newItems;

    size_t from=0;
    auto to=initialItemCount;
    if (pimpl->stickMode->currentData().toInt()==static_cast<int>(Direction::END))
    {
        from=pimpl->items.size()-initialItemCount;
        to=pimpl->items.size();
    }

    for (size_t i=from;i<to;i++)
    {
        newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,pimpl->items[i])));
    }
    pimpl->view->loadItems(newItems);
}

//--------------------------------------------------------------------------
void FwlvTestWidget::setup()
{
    QFrame* mainFrame=this;
    auto layout=new QGridLayout(mainFrame);

    pimpl->view=new FlwListType(FlwListType::PrefetchItemCountHint,UISE_DESKTOP_NAMESPACE::Order::ASC);

    int row=0;
    layout->addWidget(pimpl->view,row,0,1,4);
    layout->addWidget(new QLabel("Item(s)"),++row,0,Qt::AlignHCenter);
    layout->addWidget(new QLabel("Number"),row,1,Qt::AlignHCenter);
    layout->addWidget(new QLabel("Mode"),row,2,Qt::AlignHCenter);
    layout->addWidget(new QLabel("Operation"),row,3,Qt::AlignHCenter);

    pimpl->insertFrom=new QSpinBox(mainFrame);
    layout->addWidget(pimpl->insertFrom,++row,0);
    pimpl->insertFrom->setMinimum(0);
    pimpl->insertFrom->setMaximum(count*2);
    pimpl->insertFrom->setValue(100);
    pimpl->insertNum=new QSpinBox(mainFrame);
    layout->addWidget(pimpl->insertNum,row,1);
    pimpl->insertNum->setMinimum(1);
    pimpl->insertNum->setMaximum(10000);
    pimpl->insertNum->setValue(10);
    pimpl->insertButton=new QPushButton("Insert item(s)",mainFrame);
    layout->addWidget(pimpl->insertButton,row,3);

    pimpl->delFrom=new QSpinBox(mainFrame);
    layout->addWidget(pimpl->delFrom,++row,0);
    pimpl->delFrom->setMinimum(0);
    pimpl->delFrom->setMaximum(HelloWorldItem::HelloWorldItemId*2);
    pimpl->delFrom->setValue(1);
    pimpl->delNum=new QSpinBox(mainFrame);
    layout->addWidget(pimpl->delNum,row,1);
    pimpl->delNum->setMinimum(0);
    pimpl->delNum->setMaximum(count*2);
    pimpl->delNum->setValue(1);
    pimpl->delButton=new QPushButton("Delete item(s)",mainFrame);
    layout->addWidget(pimpl->delButton,row,3);
    QObject::connect(pimpl->delButton,&QPushButton::clicked,
    [this]()
    {
        if (pimpl->delNum->value()==1)
        {
            auto it=pimpl->itemSeqs.find(pimpl->delFrom->value());
            if (it!=pimpl->itemSeqs.end())
            {
                pimpl->items.erase(it->second);
                pimpl->itemSeqs.erase(it);
            }

            pimpl->view->beginUpdate();
            pimpl->view->removeItem(pimpl->delFrom->value());
            pimpl->view->endUpdate();
        }
        else
        {
            std::vector<HelloWorldItemWrapper::IdType> ids;
            for (size_t i=pimpl->delFrom->value();i<pimpl->delFrom->value()+pimpl->delNum->value();i++)
            {
                auto it=pimpl->itemSeqs.find(i);
                if (it!=pimpl->itemSeqs.end())
                {
                    pimpl->items.erase(it->second);
                    pimpl->itemSeqs.erase(it);
                }

                ids.emplace_back(i);
            }
            pimpl->view->removeItems(ids);
        }
    });

    pimpl->insertRandom=new QLineEdit(mainFrame);
    pimpl->insertRandom->setPlaceholderText("Comma separated item seqnum");
    layout->addWidget(pimpl->insertRandom,++row,0);
    pimpl->insertRandomButton=new QPushButton("Insert random",mainFrame);
    layout->addWidget(pimpl->insertRandomButton,row,3);
    QObject::connect(pimpl->insertRandomButton,&QPushButton::clicked,
    [this]()
    {
        std::vector<HelloWorldItemWrapper> newItems;
        auto seqnums=pimpl->insertRandom->text().split(",");
        foreach (const QString& seqnum, seqnums)
        {
            size_t num=seqnum.toUInt();
            if (num<pimpl->items.size())
            {
                newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(num,pimpl->items[num])));
            }
        }
        pimpl->view->insertItems(newItems);
    });

    pimpl->delWidgets=new QLineEdit(mainFrame);
    pimpl->delWidgets->setPlaceholderText("Comma separated item IDs");
    layout->addWidget(pimpl->delWidgets,++row,0);
    pimpl->delWidgetsButton=new QPushButton("Delete widget(s)",mainFrame);
    layout->addWidget(pimpl->delWidgetsButton,row,3);
    QObject::connect(pimpl->delWidgetsButton,&QPushButton::clicked,
    [this]()
    {
        auto ids=pimpl->delWidgets->text().split(",");
        foreach (const QString& id, ids)
        {
            auto idInt=id.toInt();

            auto it=pimpl->itemSeqs.find(idInt);
            if (it!=pimpl->itemSeqs.end())
            {
                pimpl->items.erase(it->second);
                pimpl->itemSeqs.erase(it);
            }

            const auto* item=pimpl->view->item(idInt);
            if (item)
            {
                delete item->widget();
            }
        }
    });

    pimpl->resizeWidget=new QSpinBox(mainFrame);
    layout->addWidget(pimpl->resizeWidget,++row,0);
    pimpl->resizeWidget->setMinimum(0);
    pimpl->resizeWidget->setMaximum(HelloWorldItem::HelloWorldItemId*2);
    pimpl->resizeWidget->setValue(1);
    pimpl->resizeWidgetVal=new QSpinBox(mainFrame);
    layout->addWidget(pimpl->resizeWidgetVal,row,1);
    pimpl->resizeWidgetVal->setMinimum(0);
    pimpl->resizeWidgetVal->setMaximum(1024);
    pimpl->resizeWidgetVal->setValue(100);
    pimpl->resizeWidgetButton=new QPushButton("Resize widget",mainFrame);
    layout->addWidget(pimpl->resizeWidgetButton,row,3);
    QObject::connect(pimpl->resizeWidgetButton,&QPushButton::clicked,
    [this]()
    {
        const auto* item=pimpl->view->item(pimpl->resizeWidget->value());
        if (item)
        {
            auto widget=item->widget();
            if (pimpl->view->orientation()==Qt::Horizontal)
            {
                widget->setFixedWidth(pimpl->resizeWidgetVal->value());
            }
            else
            {
                widget->setFixedHeight(pimpl->resizeWidgetVal->value());
            }
        }
    });

    pimpl->jumpItem=new QSpinBox(mainFrame);
    layout->addWidget(pimpl->jumpItem,++row,0);
    pimpl->jumpItem->setMinimum(0);
    pimpl->jumpItem->setMaximum(HelloWorldItem::HelloWorldItemId*2);
    pimpl->jumpItem->setValue(1);
    pimpl->jumpOffset=new QSpinBox(mainFrame);
    layout->addWidget(pimpl->jumpOffset,row,1);
    pimpl->jumpOffset->setMinimum(-1000);
    pimpl->jumpOffset->setMaximum(1000);
    pimpl->jumpOffset->setValue(0);
    pimpl->jumpMode=new QComboBox(mainFrame);
    layout->addWidget(pimpl->jumpMode,row,2);
    pimpl->jumpMode->addItem("item",static_cast<int>(Direction::NONE));
    pimpl->jumpMode->addItem("home",static_cast<int>(Direction::HOME));
    pimpl->jumpMode->addItem("end",static_cast<int>(Direction::END));
    pimpl->jumpToButton=new QPushButton("Jump to",mainFrame);
    layout->addWidget(pimpl->jumpToButton,row,3);

    pimpl->updateSeqNumId=new QSpinBox(mainFrame);
    layout->addWidget(pimpl->updateSeqNumId,++row,0);
    pimpl->updateSeqNumId->setMinimum(0);
    pimpl->updateSeqNumId->setMaximum(HelloWorldItem::MaxHelloWorldItemId);
    pimpl->updateSeqNumId->setValue(900004);
    pimpl->updateNewSeqNum=new QSpinBox(mainFrame);
    layout->addWidget(pimpl->updateNewSeqNum,row,1);
    pimpl->updateNewSeqNum->setMinimum(0);
    pimpl->updateNewSeqNum->setMaximum(HelloWorldItem::MaxHelloWorldItemId);
    pimpl->updateNewSeqNum->setValue(99997);
    pimpl->updateSeqNumButton=new QPushButton("Update seqnum",mainFrame);
    layout->addWidget(pimpl->updateSeqNumButton,row,3);
    QObject::connect(pimpl->updateSeqNumButton,&QPushButton::clicked,
     [this]()
     {
         auto itemId=pimpl->updateSeqNumId->value();
         auto newSeqNum=pimpl->updateNewSeqNum->value();
         auto itOldSeq=pimpl->itemSeqs.find(itemId);
         int oldId=0;
         if (itOldSeq!=pimpl->itemSeqs.end())
         {
             auto oldSeqNum=itOldSeq->second;
             if (oldSeqNum==newSeqNum)
             {
                 return;
             }

             pimpl->itemSeqs[itemId]=newSeqNum;
             pimpl->items[newSeqNum]=itemId;
             pimpl->items.erase(oldSeqNum);
         }
         else
         {
             return;
         }

         if (newSeqNum>pimpl->view->maxSortValue())
         {
             pimpl->view->setMaxSortValue(newSeqNum);
         }

         const auto* item=pimpl->view->item(itemId);
         if (item)
         {
             pimpl->view->beginUpdate();
             item->item()->setSeqNum(newSeqNum);
             pimpl->view->reorderItem(*item);
             pimpl->view->endUpdate();
         }
     });

    pimpl->clearButton=new QPushButton("Clear",mainFrame);
    layout->addWidget(pimpl->clearButton,++row,3);

    pimpl->reloadButton=new QPushButton("Reload",mainFrame);
    layout->addWidget(pimpl->reloadButton,++row,3);

    pimpl->orientationButton=new QPushButton("Vertical",mainFrame);
    layout->addWidget(pimpl->orientationButton,++row,3);
    pimpl->orientationButton->setCheckable(true);

    pimpl->stickMode=new QComboBox(mainFrame);
    layout->addWidget(pimpl->stickMode,++row,3);
    pimpl->stickMode->addItem("Stick end",static_cast<int>(Direction::END));
    pimpl->stickMode->addItem("Stick home",static_cast<int>(Direction::HOME));
    pimpl->stickMode->addItem("Stick none",static_cast<int>(Direction::NONE));
    QObject::connect(pimpl->stickMode,&QComboBox::currentIndexChanged,
        [this](int index)
        {
            auto val=static_cast<Direction>(pimpl->stickMode->itemData(index).toInt());
            pimpl->view->setStickMode(val);
        }
    );

    pimpl->flyweightButton=new QPushButton("Flyweight list",mainFrame);
    layout->addWidget(pimpl->flyweightButton,++row,3);
    pimpl->flyweightButton->setCheckable(true);
    QObject::connect(pimpl->flyweightButton,&QPushButton::toggled,
     [this](bool enable)
     {
        pimpl->view->setFlyweightEnabled(!enable);
        if (enable)
        {
            pimpl->flyweightButton->setText("Fixed list");
        }
        else
        {
            pimpl->flyweightButton->setText("Flyweight list");
        }
     }
    );

    QObject::connect(pimpl->clearButton,&QPushButton::clicked,
                     [this](){pimpl->view->clear();});

    for (size_t i=0;i<count;i++)
    {
        pimpl->items[i]=--HelloWorldItem::HelloWorldItemId;
        pimpl->itemSeqs[HelloWorldItem::HelloWorldItemId]=i;
    }

    QObject::connect(pimpl->reloadButton,&QPushButton::clicked,
     [this]()
     {
        loadItems();
     }
    );

    QObject::connect(
        pimpl->insertButton,&QPushButton::clicked,
        [this]()
        {
            std::vector<HelloWorldItemWrapper> newItems;
            for (size_t i=pimpl->insertFrom->value();i<pimpl->insertFrom->value()+pimpl->insertNum->value();i++)
            {
                if (i>=pimpl->items.size())
                {
                    break;
                }
                auto item=pimpl->view->item(pimpl->items[i]);
                if (!item)
                {
                    newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,pimpl->items[i])));
                }
                else
                {
                    newItems.emplace_back(HelloWorldItemWrapper(item->item()));
                }
            }
            if (newItems.size()==1)
            {
                pimpl->view->beginUpdate();
                pimpl->view->insertItem(newItems[0]);
                pimpl->view->endUpdate();
            }
            else
            {
                pimpl->view->insertContinuousItems(newItems);
            }
        }
    );

    QObject::connect(pimpl->orientationButton,&QPushButton::toggled,
     [this](bool enable)
     {
        pimpl->view->setOrientation(enable?Qt::Horizontal:Qt::Vertical);
        if (enable)
        {
            pimpl->orientationButton->setText("Horizontal");
        }
        else
        {
            pimpl->orientationButton->setText("Vertical");
        }
     }
    );

    auto requestItems=[this](const HelloWorldItemWrapper* item, size_t itemCount, Direction direction)
    {
        size_t idx=0;
        if (item!=nullptr)
        {
            idx=item->sortValue();
        }
#if 0
        qDebug() << "request pimpl->items "<<idx<<", "<<itemCount<<", "<<static_cast<int>(direction);
#endif
        std::vector<HelloWorldItemWrapper> newItems;
        if (item==nullptr)
        {
            for (size_t i=0;i<itemCount;i++)
            {
                // if (i>=pimpl->items.size())
                // {
                //     continue;
                // }
                if (pimpl->items.find(i)!=pimpl->items.end())
                {
                    newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,pimpl->items[i])));
                }
            }
        }
        else
        {
            if (direction==Direction::END)
            {
                for (size_t i=idx+1;i<=idx+itemCount;i++)
                {
                    // if (i>=pimpl->items.size())
                    // {
                    //     continue;
                    // }
                    if (pimpl->items.find(i)!=pimpl->items.end())
                    {
                        newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,pimpl->items[i])));
                    }
                }
            }
            else
            {
                if (itemCount>idx)
                {
                    itemCount=idx;
                }
                for (size_t i=idx-itemCount;i<idx;i++)
                {
                    // if (i>=pimpl->items.size())
                    // {
                    //     continue;
                    // }
                    if (pimpl->items.find(i)!=pimpl->items.end())
                    {
                        newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,pimpl->items[i])));
                    }
                }
            }
        }

        pimpl->view->insertContinuousItems(newItems);
    };
    pimpl->view->setRequestItemsCb(requestItems);

    auto jumpHome=[this]()
    {
#if 0
        qDebug() << "jumpHome";
#endif
        size_t itemCount=pimpl->view->itemCount();

        if (itemCount==0)
        {
            return;
        }

        size_t prefetchCount=pimpl->view->prefetchItemCount();
        auto firstItem=pimpl->view->firstItem();
        size_t firstPos=firstItem->sortValue();
        if (firstPos==0)
        {
#if 0
            qDebug() << "jumpHome only jump";
#endif

            // only jump
        }
        else if (firstPos>prefetchCount)
        {
#if 0
            qDebug() << "jumpHome reload all";
#endif
            // reload all pimpl->items
            itemCount=std::max(itemCount,prefetchCount);
            std::vector<HelloWorldItemWrapper> newItems;
            for (size_t i=0;i<prefetchCount;i++)
            {
                newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,pimpl->items[i])));
            }
            pimpl->view->loadItems(newItems);
        }
        else
        {
#if 0
            qDebug() << "jumpHome prefetch some pimpl->items";
#endif
            // prefetch only some elements
            std::vector<HelloWorldItemWrapper> newItems;
            for (size_t i=0;i<firstPos;i++)
            {
                newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,pimpl->items[i])));
            }
            pimpl->view->insertContinuousItems(newItems);
        }
        pimpl->view->scrollToEdge(Direction::HOME);
    };
    auto jumpEnd=[this]()
    {
#if 0
        qDebug() << "jumpEnd";
#endif
        size_t itemCount=pimpl->view->itemCount();

        if (itemCount==0)
        {
            return;
        }

        size_t prefetchCount=pimpl->view->prefetchItemCount();
        auto lastItem=pimpl->view->lastItem();
        size_t lastPos=lastItem->sortValue();
        if (lastPos>=count-1)
        {
#if 0
            qDebug() << "jumpEnd only jump";
#endif
            // only jump
        }
        else if ((lastPos+prefetchCount)<count)
        {
#if 0
            qDebug() << "jumpEnd reload all";
#endif
            // reload all pimpl->items
            itemCount=std::max(itemCount,prefetchCount);
            std::vector<HelloWorldItemWrapper> newItems;
            for (size_t i=count-prefetchCount;i<count;i++)
            {
                newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,pimpl->items[i])));
            }
            pimpl->view->loadItems(newItems);
        }
        else
        {
#if 0
            qDebug() << "jumpEnd prefetch last pimpl->items";
#endif
            // prefetch only last elements
            std::vector<HelloWorldItemWrapper> newItems;
            for (size_t i=count-lastPos;i<count;i++)
            {
                newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,pimpl->items[i])));
            }
            pimpl->view->insertContinuousItems(newItems);
        }
        pimpl->view->scrollToEdge(Direction::END);
    };

    pimpl->view->setRequestHomeCb([jumpHome](Qt::KeyboardModifiers modifiers){if (modifiers&Qt::ControlModifier) {jumpHome();}});
    pimpl->view->setRequestEndCb([jumpEnd](Qt::KeyboardModifiers modifiers){if (modifiers&Qt::ControlModifier) {jumpEnd();}});

    QObject::connect(
        pimpl->jumpToButton,
        &QPushButton::clicked,
        [this,jumpHome,jumpEnd]()
        {
            switch (static_cast<Direction>(pimpl->jumpMode->currentData().toInt()))
            {
                case Direction::HOME:
                {
                    jumpHome();
                }
                break;

                case Direction::END:
                {
                    jumpEnd();
                }
                break;

                case Direction::NONE:
                {
                    auto exists=pimpl->view->scrollToItem(pimpl->jumpItem->value(),pimpl->jumpOffset->value());
                    if (!exists)
                    {
                        size_t prefetchCount=pimpl->view->prefetchItemCount();
                        size_t itemCount=pimpl->view->itemCount();
                        if (itemCount<prefetchCount)
                        {
                            itemCount=3*prefetchCount;
                        }
                        size_t jumpPos=HelloWorldItem::MaxHelloWorldItemId-pimpl->jumpItem->value()-1;
                        auto from=jumpPos;
                        if (from>=HelloWorldItem::MaxHelloWorldItemId)
                        {
                            return;
                        }
                        if (from<prefetchCount)
                        {
                            from=0;
                        }
                        else
                        {
                            from-=prefetchCount;
                        }
                        std::vector<HelloWorldItemWrapper> newItems;
                        for (size_t i=from;i<from+itemCount;i++)
                        {
                            if (i>=pimpl->items.size())
                            {
                                break;
                            }
                            auto item=pimpl->view->item(pimpl->items[i]);
                            if (!item)
                            {
                                newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,pimpl->items[i])));
                            }
                            else
                            {
                                newItems.emplace_back(HelloWorldItemWrapper(item->item()));
                            }
                        }
                        if (!newItems.empty())
                        {
                            pimpl->view->loadItems(newItems);
                            std::ignore=pimpl->view->scrollToItem(pimpl->jumpItem->value(),pimpl->jumpOffset->value());
                        }
                    }
                }
                break;
            }
        }
    );
}

//--------------------------------------------------------------------------
