/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file demo/editablelabel/main.cpp
*
*  Demo application of EditableLabel.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QDateTime>
#include <QCalendarWidget>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/editablelabel.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    QMainWindow w;
    auto mainFrame=new QFrame();

    auto l = Layout::vertical(mainFrame);

    auto text = new EditableLabelText(mainFrame);
    text->setValue("Hello world Hello world Hello world");
    l->addWidget(text);

    auto integer = new EditableLabelInt(mainFrame);
    integer->widget()->setMinimum(-100);
    integer->widget()->setMaximum(100);
    integer->setValue(50);
    l->addWidget(integer);

    auto dbl = new EditableLabelFloat(mainFrame);
    dbl->widget()->setMinimum(-100.00);
    dbl->widget()->setMaximum(100.00);
    dbl->widget()->setSingleStep(0.01);
    dbl->widget()->setDecimals(4);
    dbl->setValue(50.00);
    l->addWidget(dbl);

    auto dateTime = new EditableLabelDateTime(mainFrame);
    dateTime->setValue(QDateTime::currentDateTime());
    dateTime->widget()->setCalendarPopup(true);
    dateTime->widget()->setCalendarWidget(new QCalendarWidget());
    l->addWidget(dateTime);

    auto date= new EditableLabelDate(mainFrame);
    date->setValue(QDate::currentDate());
    date->widget()->setCalendarPopup(true);
    date->widget()->setCalendarWidget(new QCalendarWidget());
    l->addWidget(date);

    auto time= new EditableLabelTime(mainFrame);
    time->setValue(QTime::currentTime());
    l->addWidget(time);

    auto list= new EditableLabelList(mainFrame);
    list->widget()->addItems({"one","two","three","four","five"});
    list->setValue("three");
    l->addWidget(list);

    l->addStretch(1);

    w.setCentralWidget(mainFrame);
    w.resize(600,500);
    w.setWindowTitle("EditableLabel Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
