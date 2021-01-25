/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** \file demo/flyweightlistview/main.cpp
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

#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/flyweightlistview.ipp>

#include <uise/desktop/flyweightlistitem.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

static size_t HelloWorldItemId=1000000;

class HelloWorldItem : public QTextBrowser
{
    public:

        HelloWorldItem(size_t seqNum, size_t id, QWidget* parent=nullptr)
            : QTextBrowser(parent),
              m_id(id),
              m_seqNum(seqNum)
        {
            setText(QString("Hello world %1, %2").arg(seqNum).arg(m_id));
            setObjectName(QString("Label %1, %2").arg(seqNum).arg(m_id));

            setFixedHeight(64);
        }

        HelloWorldItem(size_t seqNum, QWidget* parent=nullptr)
            : QTextBrowser(parent),
              m_id(HelloWorldItemId--),
              m_seqNum(seqNum)
        {
            setText(QString("Hello world %1, %2").arg(seqNum).arg(m_id));
            setObjectName(QString("Label %1").arg(m_id));

            setFixedHeight(64);
        }

        size_t seqNum() const noexcept
        {
            return m_seqNum;
        }

        size_t id() const noexcept
        {
            return m_id;
        }

        void setSeqNum(size_t value)
        {
            m_seqNum=value;
        }

    private:

        size_t m_id;
        size_t m_seqNum;
};

struct HelloWorldItemTraits : public FlyweightListItemTraits<HelloWorldItem*,QTextBrowser,size_t,size_t>
{
    static size_t sortValue(const HelloWorldItem* item) noexcept
    {
        return item->seqNum();
    }

    static QTextBrowser* widget(HelloWorldItem* item) noexcept
    {
        return item;
    }

    static size_t id(const HelloWorldItem* item)
    {
        return item->id();
    }
};

using HelloWorldItemWrapper=FlyweightListItem<HelloWorldItemTraits>;

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    {
        QMainWindow w;

        auto v=new FlyweightListView<HelloWorldItemWrapper>();

#if 0
        using ItemT=HelloWorldItemWrapper;
        auto beginEndChanged=[](const ItemT* begin,const ItemT* end)
        {
            if (begin && begin->item())
            {
                qDebug() << "Begin ID=" << begin->id();
            }

            if (end && end->item())
            {
                qDebug() << "End ID="<<end->id();
            }
        };
        v->setViewportChangedCb(beginEndChanged);

        auto item2=HelloWorldItemWrapper(new HelloWorldItem(102));
        v->insertItem(std::move(item2));
        auto item1=HelloWorldItemWrapper(new HelloWorldItem(101));
        v->insertItem(std::move(item1));

        for (size_t i=150;i<170;i++)
        {
            auto item=HelloWorldItemWrapper(new HelloWorldItem(i));
            v->insertItem(std::move(item));
        }
        for (size_t i=103;i<130;i++)
        {
            auto item=HelloWorldItemWrapper(new HelloWorldItem(i));
            v->insertItem(std::move(item));
        }

        for (size_t i=170;i<200;i++)
        {
            auto item=HelloWorldItemWrapper(new HelloWorldItem(i));
            v->insertItem(std::move(item));
        }

        std::vector<HelloWorldItemWrapper> items;
        for (size_t i=130;i<150;i++)
        {
            auto item=HelloWorldItemWrapper(new HelloWorldItem(i));
            items.push_back(std::move(item));
        }
        v->insertContinuousItems(items);

//        items[0].item()->setSeqNum(80);
//        v->insertItem(items[0]);

//        size_t start=200;
//        for (size_t i=start;i<start+100;i++)
//        {
//            auto item=HelloWorldItemWrapper(new HelloWorldItem(i));
//            v->insertItem(std::move(item));
//        }

//        qDebug() << "Has item start=" << v->hasItem(100000)<<", has some item="<<v->hasItem(0x123456);
//        qDebug() << "Scrolled to item " << (HelloWorldItemId+1) << v->scrollToItem(HelloWorldItemId+1);
//        qDebug() << "Scrolled to item " << (HelloWorldItemId+10) << v->scrollToItem(HelloWorldItemId+10);
//        qDebug() << "Scrolled to item " << (HelloWorldItemId+3) << v->scrollToItem(HelloWorldItemId+3);

//        items[10].widget()->deleteLater();


        QFrame* mainFrame=new QFrame();
        auto layout=Layout::vertical(mainFrame);
        layout->addWidget(v);

        size_t currentPos=0;

        auto addButton=new QPushButton("Add items",mainFrame);
        layout->addWidget(addButton);
        QObject::connect(addButton,&QPushButton::clicked,
            [&v,&currentPos]()
            {
                for (size_t i=currentPos;i<currentPos+10;i++)
                {
                    auto item=HelloWorldItemWrapper(new HelloWorldItem(i));
                    v->insertItem(std::move(item));
                }
                currentPos+=10;
                qDebug() << "Inserted 10 elements";
            }
        );

        w.setCentralWidget(mainFrame);

#else

        size_t count=100000;

        QFrame* mainFrame=new QFrame();
        auto layout=new QGridLayout(mainFrame);

        int row=0;

        layout->addWidget(v,row,0,1,4);

        layout->addWidget(new QLabel("From item"),++row,0,Qt::AlignHCenter);
        layout->addWidget(new QLabel("Number of items"),row,1,Qt::AlignHCenter);
        layout->addWidget(new QLabel("Mode"),row,2,Qt::AlignHCenter);
        layout->addWidget(new QLabel("Operation"),row,3,Qt::AlignHCenter);

        auto insertFrom=new QSpinBox(mainFrame);
        layout->addWidget(insertFrom,++row,0);
        insertFrom->setMinimum(0);
        insertFrom->setMaximum(count*2);
        insertFrom->setValue(1);
        auto insertNum=new QSpinBox(mainFrame);
        layout->addWidget(insertNum,row,1);
        insertNum->setMinimum(0);
        insertNum->setMinimum(10000);
        auto scrollMode=new QComboBox(mainFrame);
        layout->addWidget(scrollMode,row,2);
        scrollMode->addItem("none",static_cast<int>(Direction::NONE));
        scrollMode->addItem("home",static_cast<int>(Direction::HOME));
        scrollMode->addItem("end",static_cast<int>(Direction::END));
        scrollMode->addItem("stay at end",static_cast<int>(Direction::STAY_AT_END_EDGE));
        scrollMode->addItem("stay at home",static_cast<int>(Direction::STAY_AT_HOME_EDGE));
        auto insertButton=new QPushButton("Insert item(s)",mainFrame);
        layout->addWidget(insertButton,row,3);

        auto delFrom=new QSpinBox(mainFrame);
        layout->addWidget(delFrom,++row,0);
        delFrom->setMinimum(0);
        delFrom->setMaximum(count*2);
        delFrom->setValue(1);
        auto delNum=new QSpinBox(mainFrame);
        layout->addWidget(delNum,row,1);
        delNum->setMinimum(0);
        delNum->setMaximum(count*2);
        delNum->setValue(1);
        auto delButton=new QPushButton("Delete item(s)",mainFrame);
        layout->addWidget(delButton,row,3);

        auto jumpItem=new QSpinBox(mainFrame);
        layout->addWidget(jumpItem,++row,0);
        jumpItem->setMinimum(0);
        jumpItem->setMaximum(count*2);
        jumpItem->setValue(1);
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

        QObject::connect(clearButton,&QPushButton::clicked,
                         [&v](){v->clear();});

        std::map<size_t,size_t> items;
        for (size_t i=0;i<count;i++)
        {
            items[i]=--HelloWorldItemId;
        }

        QObject::connect(reloadButton,&QPushButton::clicked,
         [&v,&items](){
            std::vector<HelloWorldItemWrapper> newItems;
            for (size_t i=0;i<v->prefetchItemCount()*10;i++)
            {
                newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,items[i])));
            }
            v->loadItems(newItems);
        });

        auto requestBefore=[&v,&items](const HelloWorldItemWrapper* item, size_t itemCount)
        {
            size_t idx=0;
            if (item!=nullptr)
            {
                idx=item->sortValue();
            }

            qDebug() << "requestBefore "<<idx<<", "<<itemCount;

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

            v->insertContinuousItems(newItems);
        };

        auto requestAfter=[&v,&items](const HelloWorldItemWrapper* item, size_t itemCount)
        {
            size_t idx=0;
            if (item!=nullptr)
            {
                idx=item->sortValue();
            }

            qDebug() << "requestAfter "<<idx<<", "<<itemCount;

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
                for (size_t i=idx+1;i<=idx+itemCount;i++)
                {
                    if (i>=items.size())
                    {
                        continue;
                    }
                    newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,items[i])));
                }
            }

            v->insertContinuousItems(newItems);
        };

        v->setRequestItemsBeforeCb(requestBefore);
        v->setRequestItemsAfterCb(requestAfter);

        v->setFlyweightEnabled(false);
//        v->setOrientation(Qt::Horizontal);

        std::vector<HelloWorldItemWrapper> newItems;
        for (size_t i=0;i<v->prefetchItemCount();i++)
        {
            newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,items[i])));
        }
        v->loadItems(newItems);

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
                v->scrollToEdge(Direction::HOME);
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
                v->loadItems(newItems,Direction::HOME);
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
                v->insertContinuousItems(newItems,Direction::HOME);
            }

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
                v->scrollToEdge(Direction::END);
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
                v->loadItems(newItems,Direction::END);
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
                v->insertContinuousItems(newItems,Direction::END);
            }
        };

        v->setRequestHomeCb(jumpHome);
        v->setRequestEndCb(jumpEnd);

#endif
//        auto w1=new QMainWindow();

//        auto f1=new QFrame(w1);
//        w1->setCentralWidget(f1);

//        auto te1=new QTextEdit(f1);

//        te1->setFixedSize(1024,1024);
//        te1->move(0,-800);

//        te1->show();
//        w1->show();

        w.show();
        app.exec();
    }

    return 0;
}

//--------------------------------------------------------------------------
