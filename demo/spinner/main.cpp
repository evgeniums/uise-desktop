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

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/spinner.hpp>

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

    auto section1=std::make_shared<SpinnerSection>();
    section1->itemsWidth = 50;
    section1->circular = true;
    for (auto i=0;i<20;i++)
    {
        auto item=new QLabel(QString::number(i));
        item->setAlignment(Qt::AlignCenter);
        item->setFixedWidth(section1->itemsWidth);
        section1->items.append(item);
    }

    std::vector<QString> text{"one","two","three","four","five","six","seven","eight","nine","ten","eleven","twelve","thirteen","fourteen","fifteen"};
    auto section2=std::make_shared<SpinnerSection>();
    section2->itemsWidth = 70;
    for (auto i=0;i<text.size();i++)
    {
        auto item=new QLabel(text[i]);
        item->setAlignment(Qt::AlignCenter);
        item->setFixedWidth(section2->itemsWidth);
        section2->items.append(item);
    }

    std::vector<std::shared_ptr<SpinnerSection>> sections{section1, section2};
    spinner->setSections(sections);
    auto ll = new QLineEdit();
    ll->setObjectName("LineEdit");
    spinner->setStyleSample(ll);
    qApp->setStyleSheet("* {color: black; font-size: 20px;} \n QLabel {background-color: transparent;} \n QLineEdit {background-color: white; selection-background-color: lightgray;}");
//    qApp->setStyleSheet("* {color: white; font-size: 20px;} \n QLabel {background-color: transparent;} \n QLineEdit {background-color: black; selection-background-color: darkgray;}");

    spinner->setFixedSize(500,500);

    l->addStretch(1);

    w.setCentralWidget(mainFrame);
    w.resize(600,500);
    w.setWindowTitle("Spinner Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
