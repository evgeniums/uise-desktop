/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file demo/spinner/main.cpp
*
*  Demo application of Spinner.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QTextEdit>
#include <QSpinBox>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/spinner.hpp>
#include <uise/desktop/spinnersection.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    QMainWindow w;
    auto mainFrame=new QFrame();

    auto l = Layout::vertical(mainFrame);

    auto spinner = new Spinner(mainFrame);
    l->addWidget(spinner);

    int itemHeight=50;

    std::vector<QList<QLabel*>> labels;
    labels.resize(4);

    auto section1=std::make_shared<SpinnerSection>();
    section1->setItemsWidth(30);
    section1->setCircular(true);
    auto leftLb = new QLabel(" $");
    leftLb->setAlignment(Qt::AlignCenter);
    leftLb->setFixedHeight(itemHeight);
    section1->setLeftBarLabel(leftLb);
    section1->setLeftBarWidth(leftLb->sizeHint().width());
    QList<QWidget*> items1;
    for (auto i=1;i<=31;i++)
    {
        auto item=new QLabel(QString::number(i));
        item->setAlignment(Qt::AlignCenter);
        item->setFixedWidth(section1->itemsWidth());
        item->setFixedHeight(itemHeight);
        items1.append(item);
        labels[0].append(item);
    }
    section1->setItems(items1);

    std::vector<QString> text{"zero","one","two","three","four","five","six","seven","eight","nine","ten","eleven","twelve","thirteen","fourteen","fifteen",
                             "sixteen","seventeen","eightteen","nineteen","twenty"};
    auto section2=std::make_shared<SpinnerSection>();
    section2->setItemsWidth(120);
    auto rightLb = new QLabel(" km ");
    rightLb->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    rightLb->setFixedHeight(itemHeight);
    section2->setRightBarLabel(rightLb);
    section2->setRightBarWidth(rightLb->sizeHint().width());
    QList<QWidget*> items2;
    for (auto i=0;i<text.size();i++)
    {
        auto item=new QLabel(text[i]);
        item->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        item->setFixedWidth(section2->itemsWidth());
        item->setFixedHeight(itemHeight);
        items2.append(item);
        labels[1].append(item);
    }
    section2->setItems(items2);

    std::vector<QString> text2{"yes","no","maybe"};
    auto section3=std::make_shared<SpinnerSection>();
    section3->setItemsWidth(100);
    section3->setCircular(true);
    QList<QWidget*> items3;
    QList<QLabel*> labels3;
    for (auto i=0;i<text2.size();i++)
    {
        auto item=new QLabel(text2[i]);
        item->setAlignment(Qt::AlignCenter);
        item->setFixedWidth(section3->itemsWidth());
        item->setFixedHeight(itemHeight);
        items3.append(item);
        labels[2].append(item);
    }
    section3->setItems(items3);

    std::vector<QString> text3{"hi","hello","how are you"};
    auto section4=std::make_shared<SpinnerSection>();
    section4->setItemsWidth(200);
    QList<QWidget*> items4;
    QList<QLabel*> labels4;
    for (auto i=0;i<text3.size();i++)
    {
        auto item=new QLabel(text3[i]);
        item->setAlignment(Qt::AlignCenter);
        item->setFixedWidth(section4->itemsWidth());
        item->setFixedHeight(itemHeight);
        items4.append(item);
        labels[3].append(item);
    }
    section4->setItems(items4);

    std::vector<std::shared_ptr<SpinnerSection>> sections{section1, section2, section3, section4};
    spinner->setItemHeight(itemHeight);
    spinner->setSections(sections);

    auto messagesView=new QTextEdit();
    messagesView->setReadOnly(true);
    l->addWidget(messagesView);

    QObject::connect(spinner,&Spinner::itemChanged,[spinner,section1,itemHeight,messagesView,labels](int section, int item){

        QString val=labels[section].at(item)->text();
        auto msg=QString("Section %1, item %2, value %3 \n").arg(section).arg(item).arg(val);
        messagesView->insertPlainText(msg);
        messagesView->ensureCursorVisible();

        qDebug() << "Item changed: section "<<section<<" item "<<item;

        if (section==1)
        {
            if (item==2)
            {
                qDebug() << "Items size before remove "<<section1->itemsCount();
                if (section1->itemsCount()==31)
                {
                    qDebug() << "Remove 3 items";
                    spinner->removeLastItems(0,3);
                }
                qDebug() << "Items size after remove "<<section1->itemsCount();
            }
            else if (item==0)
            {
                if (section1->itemsCount()!=31)
                {
                    qDebug() << "Append 3 items";

                    QList<QWidget*> items;
                    for (auto i=29;i<=31;i++)
                    {
                        auto item=new QLabel(QString::number(i));
                        item->setAlignment(Qt::AlignCenter);
                        item->setFixedWidth(section1->itemsWidth());
                        item->setFixedHeight(itemHeight);
                        items.append(item);
                    }
                    spinner->appendItems(0,items);
                }
            }
        }

    });

    auto jump=new QFrame();
    jump->setObjectName("JumpFrame");
    auto jl = Layout::grid(jump);

    auto jump2_1=new QLabel(" section ");
    jl->addWidget(jump2_1,0,1);
    auto jump2=new QSpinBox();
    jump2->setRange(0,3);
    jl->addWidget(jump2,0,2);

    auto jump3_1=new QLabel(" item ");
    jl->addWidget(jump3_1,0,3);
    auto jump3=new QSpinBox();
    jl->addWidget(jump3,0,4);

    auto jump4=new QPushButton();
    jump4->setText("Jump");
    jl->addWidget(jump4,0,5);

    auto stretch=new QFrame();
    stretch->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
    jl->addWidget(stretch,0,6);

    QObject::connect(jump2,&QSpinBox::valueChanged,[jump3,labels](int val){
        jump3->setRange(0,labels[val].size()-1);
    });

    QObject::connect(jump4,&QPushButton::clicked,[spinner,jump2,jump3](){
        spinner->selectItem(jump2->value(),jump3->value());
    });

    l->addWidget(jump);

    auto ll = new QLineEdit();
    ll->setObjectName("LineEdit");
    spinner->setStyleSample(ll);

    spinner->setFixedSize(500,500);

    QString darkTheme="*{font-size: 20px;} \n uise--desktop--Spinner *{color: white;} \n uise--desktop--Spinner QLabel {background-color: transparent;} \n uise--desktop--Spinner *[style-sample=\"true\"] {background-color: #111111; selection-background-color: #444444;}";
    QString lightTheme="*{font-size: 20px;} \n uise--desktop--Spinner *{color: black;} \n uise--desktop--Spinner QLabel {background-color: transparent;} \n uise--desktop--Spinner *[style-sample=\"true\"] {background-color: white; selection-background-color: lightgray;}";

    auto styleButton=new QPushButton();
    styleButton->setText("Dark theme");
    styleButton->setCheckable(true);
    QObject::connect(styleButton,&QPushButton::toggled,spinner,[&darkTheme,&lightTheme,spinner,styleButton](bool enable){
        if (enable)
        {
            qApp->setStyleSheet(darkTheme);
            styleButton->setText("Light theme");
        }
        else
        {
            qApp->setStyleSheet(lightTheme);
            styleButton->setText("Dark theme");
        }
        spinner->style()->unpolish(spinner);
        spinner->style()->polish(spinner);
    });
    l->addWidget(styleButton);

    l->addStretch(1);

    qApp->setStyleSheet(lightTheme);

    w.setCentralWidget(mainFrame);
    w.resize(600,800);
    w.setWindowTitle("Spinner Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
