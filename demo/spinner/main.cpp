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
    }
    section2->setItems(items2);

    std::vector<QString> text2{"yes","no","maybe"};
    auto section3=std::make_shared<SpinnerSection>();
    section3->setItemsWidth(100);
    section3->setCircular(true);
    QList<QWidget*> items3;
    for (auto i=0;i<text2.size();i++)
    {
        auto item=new QLabel(text2[i]);
        item->setAlignment(Qt::AlignCenter);
        item->setFixedWidth(section3->itemsWidth());
        item->setFixedHeight(itemHeight);
        items3.append(item);
    }
    section3->setItems(items3);

    std::vector<QString> text3{"hi","hello","how are you"};
    auto section4=std::make_shared<SpinnerSection>();
    section4->setItemsWidth(200);
    QList<QWidget*> items4;
    for (auto i=0;i<text3.size();i++)
    {
        auto item=new QLabel(text3[i]);
        item->setAlignment(Qt::AlignCenter);
        item->setFixedWidth(section4->itemsWidth());
        item->setFixedHeight(itemHeight);
        items4.append(item);
    }
    section4->setItems(items4);

    std::vector<std::shared_ptr<SpinnerSection>> sections{section1, section2, section3, section4};
    spinner->setItemHeight(itemHeight);
    spinner->setSections(sections);

    QObject::connect(spinner,&Spinner::itemChanged,[spinner,section1,itemHeight](int section, int item){
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

//    spinner->selectItem(section2.get(),7);
//    spinner->selectItem(section1.get(),17);

    auto ll = new QLineEdit();
    ll->setObjectName("LineEdit");
    spinner->setStyleSample(ll);

    spinner->setFixedSize(500,500);

    QString darkTheme="* {color: white; font-size: 20px;} \n QLabel {background-color: transparent;} \n QLineEdit {background-color: #111111; selection-background-color: #444444;}";
    QString lightTheme="* {color: black; font-size: 20px;} \n QLabel {background-color: transparent;} \n QLineEdit {background-color: white; selection-background-color: lightgray;}";
//    qApp->setStyleSheet("* {color: black; font-size: 20px;} \n QLabel {background-color: transparent;} \n QLineEdit {background-color: white; selection-background-color: lightgray;}");
//    qApp->setStyleSheet("* {color: white; font-size: 20px;} \n QLabel {background-color: transparent;} \n QLineEdit {background-color: #111111; selection-background-color: #444444;}");

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
    w.resize(600,500);
    w.setWindowTitle("Spinner Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
