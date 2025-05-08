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
    integer->editorWidget()->setMinimum(-100);
    integer->editorWidget()->setMaximum(100);
    integer->setValue(50);
    l->addWidget(integer);

    auto dbl = new EditableLabelDouble(mainFrame);
    dbl->editorWidget()->setMinimum(-100.00);
    dbl->editorWidget()->setMaximum(100.00);
    dbl->editorWidget()->setSingleStep(0.01);
    dbl->editorWidget()->setDecimals(4);
    dbl->setValue(50.00);
    l->addWidget(dbl);

    auto dateTime = new EditableLabelDateTime(mainFrame);
    dateTime->setValue(QDateTime::currentDateTime());
    dateTime->editorWidget()->setCalendarPopup(true);
    dateTime->editorWidget()->setCalendarWidget(new QCalendarWidget());
    l->addWidget(dateTime);

    auto date= new EditableLabelDate(mainFrame);
    date->setValue(QDate::currentDate());
    date->editorWidget()->setCalendarPopup(true);
    date->editorWidget()->setCalendarWidget(new QCalendarWidget());
    l->addWidget(date);

    auto time= new EditableLabelTime(mainFrame);
    time->setValue(QTime::currentTime());
    l->addWidget(time);

    auto list= new EditableLabelList(mainFrame);
    list->editorWidget()->addItems({"one","two","three","four","five"});
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
