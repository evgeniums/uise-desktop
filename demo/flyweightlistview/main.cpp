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
#include <QToolButton>

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

        size_t count=100000;

        QFrame* mainFrame=new QFrame();
        auto layout=new QGridLayout(mainFrame);

        int row=0;

        layout->addWidget(v,row,0,1,4);

        layout->addWidget(new QLabel("From item"),++row,0,Qt::AlignHCenter);
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
        QObject::connect(
            jumpToButton,
            &QPushButton::clicked,
            [&v,&jumpItem,&jumpMode,&jumpOffset]()
            {
                switch (static_cast<Direction>(jumpMode->currentData().toInt()))
                {
                    case Direction::HOME:
                    {
                        v->scrollToEdge(Direction::HOME);
                    }
                    break;

                    case Direction::END:
                    {
                        v->scrollToEdge(Direction::END);
                    }
                    break;

                    case Direction::NONE:
                    {
                        v->scrollToItem(jumpItem->value(),jumpOffset->value());
                    }
                    break;
                }
            }
        );

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

        QObject::connect(clearButton,&QPushButton::clicked,
                         [&v](){v->clear();});

        std::map<size_t,size_t> items;
        for (size_t i=0;i<count;i++)
        {
            items[i]=--HelloWorldItemId;
        }
        auto loadItems=[&v,&items]()
        {
            std::vector<HelloWorldItemWrapper> newItems;
            for (size_t i=50;i<53;i++)
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
                    newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,items[i])));
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
        };

        v->setRequestHomeCb(jumpHome);
        v->setRequestEndCb(jumpEnd);

        v->setFocus();

        w.resize(600,400);

        loadItems();

        w.setWindowTitle("FlyweightListView Demo");
        w.show();
        app.exec();
    }

    return 0;
}

//--------------------------------------------------------------------------
