/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

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

    pimpl->view=new FlwListType();

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
            pimpl->view->beginUpdate();
            pimpl->view->removeItem(pimpl->delFrom->value());
            pimpl->view->endUpdate();
        }
        else
        {
            std::vector<HelloWorldItemWrapper::IdType> ids;
            for (size_t i=pimpl->delFrom->value();i>pimpl->delFrom->value()-pimpl->delNum->value();i--)
            {
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
    pimpl->jumpItem=new QSpinBox(mainFrame);
    layout->addWidget(pimpl->jumpItem,row,1);
    pimpl->jumpItem->setMinimum(-1000);
    pimpl->jumpItem->setMaximum(1000);
    pimpl->jumpItem->setValue(0);
    pimpl->jumpMode=new QComboBox(mainFrame);
    layout->addWidget(pimpl->jumpMode,row,2);
    pimpl->jumpMode->addItem("item",static_cast<int>(Direction::NONE));
    pimpl->jumpMode->addItem("home",static_cast<int>(Direction::HOME));
    pimpl->jumpMode->addItem("end",static_cast<int>(Direction::END));
    pimpl->jumpToButton=new QPushButton("Jump to",mainFrame);
    layout->addWidget(pimpl->jumpToButton,row,3);

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
#if 1
        qDebug() << "request pimpl->items "<<idx<<", "<<itemCount<<", "<<static_cast<int>(direction);
#endif
        std::vector<HelloWorldItemWrapper> newItems;
        if (item==nullptr)
        {
            for (size_t i=0;i<itemCount;i++)
            {
                if (i>=pimpl->items.size())
                {
                    continue;
                }
                newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,pimpl->items[i])));
            }
        }
        else
        {
            if (direction==Direction::END)
            {
                for (size_t i=idx+1;i<=idx+itemCount;i++)
                {
                    if (i>=pimpl->items.size())
                    {
                        continue;
                    }
                    newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,pimpl->items[i])));
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
                    if (i>=pimpl->items.size())
                    {
                        continue;
                    }
                    newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,pimpl->items[i])));
                }
            }
        }

        pimpl->view->insertContinuousItems(newItems);
    };
    pimpl->view->setRequestItemsCb(requestItems);

    auto jumpHome=[this]()
    {
        qDebug() << "jumpHome";

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
            qDebug() << "jumpHome only jump";

            // only jump
        }
        else if (firstPos>prefetchCount)
        {
            qDebug() << "jumpHome reload all";

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
            qDebug() << "jumpHome prefetch some pimpl->items";

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
        qDebug() << "jumpEnd";

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
            qDebug() << "jumpEnd only jump";

            // only jump
        }
        else if ((lastPos+prefetchCount)<count)
        {
            qDebug() << "jumpEnd reload all";

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
            qDebug() << "jumpEnd prefetch last pimpl->items";

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

    pimpl->view->setRequestHomeCb(jumpHome);
    pimpl->view->setRequestEndCb(jumpEnd);

    QObject::connect(
        pimpl->jumpToButton,
        &QPushButton::clicked,
        [this,&jumpHome,&jumpEnd]()
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
                    auto exists=pimpl->view->scrollToItem(pimpl->jumpItem->value(),pimpl->jumpItem->value());
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
                            std::ignore=pimpl->view->scrollToItem(pimpl->jumpItem->value(),pimpl->jumpItem->value());
                        }
                    }
                }
                break;
            }
        }
    );
}

//--------------------------------------------------------------------------
