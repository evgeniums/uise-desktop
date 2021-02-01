/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file demo/flyweightlistview/main.cpp
*
*  Demo application of FlyweightListView.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QTimer>
#include <QPushButton>
#include <QTextBrowser>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>

#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/flyweightlistview.ipp>

#include <uise/desktop/flyweightlistitem.hpp>

#include "helloworlditem.hpp"

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    {
        QMainWindow w;

#if 1

        auto v=new FlyweightListView<HelloWorldItemWrapper>();

        size_t count=100000;
        std::map<size_t,size_t> items;

        QFrame* mainFrame=new QFrame();
        auto layout=new QGridLayout(mainFrame);

        int row=0;

        layout->addWidget(v,row,0,1,4);

        layout->addWidget(new QLabel("Item(s)"),++row,0,Qt::AlignHCenter);
        layout->addWidget(new QLabel("Number"),row,1,Qt::AlignHCenter);
        layout->addWidget(new QLabel("Mode"),row,2,Qt::AlignHCenter);
        layout->addWidget(new QLabel("Operation"),row,3,Qt::AlignHCenter);

        auto insertFrom=new QSpinBox(mainFrame);
        layout->addWidget(insertFrom,++row,0);
        insertFrom->setMinimum(0);
        insertFrom->setMaximum(count*2);
        insertFrom->setValue(100);
        auto insertNum=new QSpinBox(mainFrame);
        layout->addWidget(insertNum,row,1);
        insertNum->setMinimum(1);
        insertNum->setMaximum(10000);
        insertNum->setValue(10);
        auto insertButton=new QPushButton("Insert item(s)",mainFrame);
        layout->addWidget(insertButton,row,3);

        auto delFrom=new QSpinBox(mainFrame);
        layout->addWidget(delFrom,++row,0);
        delFrom->setMinimum(0);
        delFrom->setMaximum(HelloWorldItemId*2);
        delFrom->setValue(1);
        auto delNum=new QSpinBox(mainFrame);
        layout->addWidget(delNum,row,1);
        delNum->setMinimum(0);
        delNum->setMaximum(count*2);
        delNum->setValue(1);
        auto delButton=new QPushButton("Delete item(s)",mainFrame);
        layout->addWidget(delButton,row,3);
        QObject::connect(delButton,&QPushButton::clicked,
        [&v,&delFrom,&delNum]()
        {
            if (delNum->value()==1)
            {
                v->beginUpdate();
                v->removeItem(delFrom->value());
                v->endUpdate();
            }
            else
            {
                std::vector<HelloWorldItemWrapper::IdType> ids;
                for (size_t i=delFrom->value();i>delFrom->value()-delNum->value();i--)
                {
                    ids.emplace_back(i);
                }
                v->removeItems(ids);
            }
        });

        auto insertRandom=new QLineEdit(mainFrame);
        insertRandom->setPlaceholderText("Comma separated item seqnum");
        layout->addWidget(insertRandom,++row,0);
        auto insertRandomButton=new QPushButton("Insert random",mainFrame);
        layout->addWidget(insertRandomButton,row,3);
        QObject::connect(insertRandomButton,&QPushButton::clicked,
        [&v,&items,&insertRandom]()
        {
            std::vector<HelloWorldItemWrapper> newItems;
            auto seqnums=insertRandom->text().split(",");
            foreach (const QString& seqnum, seqnums)
            {
                size_t num=seqnum.toUInt();
                if (num<items.size())
                {
                    newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(num,items[num])));
                }
            }
            v->insertItems(newItems);
        });

        auto delWidgets=new QLineEdit(mainFrame);
        delWidgets->setPlaceholderText("Comma separated item IDs");
        layout->addWidget(delWidgets,++row,0);
        auto delWidgetsButton=new QPushButton("Delete widget(s)",mainFrame);
        layout->addWidget(delWidgetsButton,row,3);
        QObject::connect(delWidgetsButton,&QPushButton::clicked,
        [&v,&delWidgets]()
        {
            auto ids=delWidgets->text().split(",");
            foreach (const QString& id, ids)
            {
                auto idInt=id.toInt();
                const auto* item=v->item(idInt);
                if (item)
                {
                    delete item->widget();
                }
            }
        });

        auto resizeWidget=new QSpinBox(mainFrame);
        layout->addWidget(resizeWidget,++row,0);
        resizeWidget->setMinimum(0);
        resizeWidget->setMaximum(HelloWorldItemId*2);
        resizeWidget->setValue(1);
        auto resizeWidgetVal=new QSpinBox(mainFrame);
        layout->addWidget(resizeWidgetVal,row,1);
        resizeWidgetVal->setMinimum(0);
        resizeWidgetVal->setMaximum(1024);
        resizeWidgetVal->setValue(100);
        auto resizeWidgetButton=new QPushButton("Resize widget",mainFrame);
        layout->addWidget(resizeWidgetButton,row,3);
        QObject::connect(resizeWidgetButton,&QPushButton::clicked,
        [&v,&resizeWidget,&resizeWidgetVal]()
        {
            const auto* item=v->item(resizeWidget->value());
            if (item)
            {
                auto widget=item->widget();
                if (v->orientation()==Qt::Horizontal)
                {
                    widget->setFixedWidth(resizeWidgetVal->value());
                }
                else
                {
                    widget->setFixedHeight(resizeWidgetVal->value());
                }
            }
        });

        auto jumpItem=new QSpinBox(mainFrame);
        layout->addWidget(jumpItem,++row,0);
        jumpItem->setMinimum(0);
        jumpItem->setMaximum(HelloWorldItemId*2);
        jumpItem->setValue(1);
        auto jumpOffset=new QSpinBox(mainFrame);
        layout->addWidget(jumpOffset,row,1);
        jumpOffset->setMinimum(-1000);
        jumpOffset->setMaximum(1000);
        jumpOffset->setValue(0);
        auto jumpMode=new QComboBox(mainFrame);
        layout->addWidget(jumpMode,row,2);
        jumpMode->addItem("item",static_cast<int>(Direction::NONE));
        jumpMode->addItem("home",static_cast<int>(Direction::HOME));
        jumpMode->addItem("end",static_cast<int>(Direction::END));
        auto jumpToButton=new QPushButton("Jump to",mainFrame);
        layout->addWidget(jumpToButton,row,3);

        auto clearButton=new QPushButton("Clear",mainFrame);
        layout->addWidget(clearButton,++row,3);

        auto reloadButton=new QPushButton("Reload",mainFrame);
        layout->addWidget(reloadButton,++row,3);

        auto orientationButton=new QPushButton("Vertical",mainFrame);
        layout->addWidget(orientationButton,++row,3);
        orientationButton->setCheckable(true);

        auto stickMode=new QComboBox(mainFrame);
        layout->addWidget(stickMode,++row,3);
        stickMode->addItem("Stick end",static_cast<int>(Direction::END));
        stickMode->addItem("Stick home",static_cast<int>(Direction::HOME));
        stickMode->addItem("Stick none",static_cast<int>(Direction::NONE));
        QObject::connect(stickMode,&QComboBox::currentIndexChanged,
            [&v,&stickMode](int index)
            {
                auto val=static_cast<Direction>(stickMode->itemData(index).toInt());
                v->setStickMode(val);
            }
        );

        auto flyweightButton=new QPushButton("Flyweight list",mainFrame);
        layout->addWidget(flyweightButton,++row,3);
        flyweightButton->setCheckable(true);
        QObject::connect(flyweightButton,&QPushButton::toggled,
         [&v,&flyweightButton](bool enable)
         {
            v->setFlyweightEnabled(!enable);
            if (enable)
            {
                flyweightButton->setText("Fixed list");
            }
            else
            {
                flyweightButton->setText("Flyweight list");
            }
         }
        );

        QObject::connect(clearButton,&QPushButton::clicked,
                         [&v](){v->clear();});

        for (size_t i=0;i<count;i++)
        {
            items[i]=--HelloWorldItemId;
        }
        auto loadItems=[&v,&items,&stickMode]()
        {
            v->setMaxSortValue(items.size()-1);
            v->setMinSortValue(0);

            std::vector<HelloWorldItemWrapper> newItems;

            size_t count=10;

            auto from=0;
            auto to=count;
            if (stickMode->currentData().toInt()==static_cast<int>(Direction::END))
            {
                from=items.size()-count;
                to=items.size();
            }

            for (size_t i=from;i<to;i++)
            {
                newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,items[i])));
            }
            v->loadItems(newItems);
        };

        QObject::connect(reloadButton,&QPushButton::clicked,
         [&v,&items,&loadItems]()
         {
            loadItems();
         }
        );

        QObject::connect(
            insertButton,&QPushButton::clicked,
            [&v,&items,&insertFrom,&insertNum]()
            {
                std::vector<HelloWorldItemWrapper> newItems;
                for (size_t i=insertFrom->value();i<insertFrom->value()+insertNum->value();i++)
                {
                    if (i>=items.size())
                    {
                        break;
                    }
                    auto item=v->item(items[i]);
                    if (!item)
                    {
                        newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,items[i])));
                    }
                    else
                    {
                        newItems.emplace_back(HelloWorldItemWrapper(item->item()));
                    }
                }
                if (newItems.size()==1)
                {
                    v->beginUpdate();
                    v->insertItem(newItems[0]);
                    v->endUpdate();
                }
                else
                {
                    v->insertContinuousItems(newItems);
                }
            }
        );

        QObject::connect(orientationButton,&QPushButton::toggled,
         [&v,&orientationButton](bool enable)
         {
            v->setOrientation(enable?Qt::Horizontal:Qt::Vertical);
            if (enable)
            {
                orientationButton->setText("Horizontal");
            }
            else
            {
                orientationButton->setText("Vertical");
            }
         }
        );

        auto requestItems=[&v,&items](const HelloWorldItemWrapper* item, size_t itemCount, Direction direction)
        {
            size_t idx=0;
            if (item!=nullptr)
            {
                idx=item->sortValue();
            }

            qDebug() << "request items "<<idx<<", "<<itemCount<<", "<<static_cast<int>(direction);

            std::vector<HelloWorldItemWrapper> newItems;
            if (item==nullptr)
            {
                for (size_t i=0;i<itemCount;i++)
                {
                    if (i>=items.size())
                    {
                        continue;
                    }
                    newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,items[i])));
                }
            }
            else
            {
                if (direction==Direction::END)
                {
                    for (size_t i=idx+1;i<=idx+itemCount;i++)
                    {
                        if (i>=items.size())
                        {
                            continue;
                        }
                        newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,items[i])));
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
                        if (i>=items.size())
                        {
                            continue;
                        }
                        newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,items[i])));
                    }
                }
            }

            v->insertContinuousItems(newItems);
        };

        v->setRequestItemsCb(requestItems);

        w.setCentralWidget(mainFrame);

        auto jumpHome=[v,&items]()
        {
            qDebug() << "jumpHome";

            size_t itemCount=v->itemCount();

            if (itemCount==0)
            {
                return;
            }

            size_t prefetchCount=v->prefetchItemCount();
            auto firstItem=v->firstItem();
            size_t firstPos=firstItem->sortValue();
            if (firstPos==0)
            {
                qDebug() << "jumpHome only jump";

                // only jump
            }
            else if (firstPos>prefetchCount)
            {
                qDebug() << "jumpHome reload all";

                // reload all items
                itemCount=std::max(itemCount,prefetchCount);
                std::vector<HelloWorldItemWrapper> newItems;
                for (size_t i=0;i<prefetchCount;i++)
                {
                    newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,items[i])));
                }
                v->loadItems(newItems);
            }
            else
            {
                qDebug() << "jumpHome prefetch some items";

                // prefetch only some elements
                std::vector<HelloWorldItemWrapper> newItems;
                for (size_t i=0;i<firstPos;i++)
                {
                    newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,items[i])));
                }
                v->insertContinuousItems(newItems);
            }
            v->scrollToEdge(Direction::HOME);
        };
        auto jumpEnd=[v,&items,&count]()
        {
            qDebug() << "jumpEnd";

            size_t itemCount=v->itemCount();

            if (itemCount==0)
            {
                return;
            }

            size_t prefetchCount=v->prefetchItemCount();
            auto lastItem=v->lastItem();
            size_t lastPos=lastItem->sortValue();
            if (lastPos>=count-1)
            {
                qDebug() << "jumpEnd only jump";

                // only jump
            }
            else if ((lastPos+prefetchCount)<count)
            {
                qDebug() << "jumpEnd reload all";

                // reload all items
                itemCount=std::max(itemCount,prefetchCount);
                std::vector<HelloWorldItemWrapper> newItems;
                for (size_t i=count-prefetchCount;i<count;i++)
                {
                    newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,items[i])));
                }
                v->loadItems(newItems);
            }
            else
            {
                qDebug() << "jumpEnd prefetch last items";

                // prefetch only last elements
                std::vector<HelloWorldItemWrapper> newItems;
                for (size_t i=count-lastPos;i<count;i++)
                {
                    newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,items[i])));
                }
                v->insertContinuousItems(newItems);
            }
            v->scrollToEdge(Direction::END);
        };

        v->setRequestHomeCb(jumpHome);
        v->setRequestEndCb(jumpEnd);

        QObject::connect(
            jumpToButton,
            &QPushButton::clicked,
            [&v,&items,&jumpItem,&jumpMode,&jumpOffset,&jumpHome,&jumpEnd]()
            {
                switch (static_cast<Direction>(jumpMode->currentData().toInt()))
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
                        auto exists=v->scrollToItem(jumpItem->value(),jumpOffset->value());
                        if (!exists)
                        {
                            size_t prefetchCount=v->prefetchItemCount();
                            size_t itemCount=v->itemCount();
                            if (itemCount<prefetchCount)
                            {
                                itemCount=3*prefetchCount;
                            }
                            size_t jumpPos=MaxHelloWorldItemId-jumpItem->value()-1;
                            auto from=jumpPos;
                            if (from>=MaxHelloWorldItemId)
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
                                if (i>=items.size())
                                {
                                    break;
                                }
                                auto item=v->item(items[i]);
                                if (!item)
                                {
                                    newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,items[i])));
                                }
                                else
                                {
                                    newItems.emplace_back(HelloWorldItemWrapper(item->item()));
                                }
                            }
                            if (!newItems.empty())
                            {
                                v->loadItems(newItems);
                                std::ignore=v->scrollToItem(jumpItem->value(),jumpOffset->value());
                            }
                        }
                    }
                    break;
                }
            }
        );


        v->setFocus();
        SingleShotTimer load;
        load.shot(0,
            [&loadItems]
            {
                loadItems();
            }
        );
#else

        auto f=new QFrame();

//        qApp->setStyleSheet("QFrame {background: blue;} \n HelloWorldItem {background: red; border: 1px solid white;} \n QTextBrowser {background: yellow}");
        qApp->setStyleSheet("HelloWorldItem { background: red; border: 1px solid white; }");

        w.setCentralWidget(f);
        auto l=Layout::vertical(f);
        l->addWidget(new HelloWorldItem(1));
//        l->addWidget(new HelloWorldItem(2),0,Qt::AlignRight);
//        l->addWidget(new HelloWorldItem(3),0,Qt::AlignRight);
        l->addStretch(1);

#endif

        w.resize(600,500);

        w.setWindowTitle("FlyweightListView Demo");
        w.show();
        app.exec();
    }

    return 0;
}

//--------------------------------------------------------------------------
