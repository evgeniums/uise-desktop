/**
@copyright Evgeny Sidorov 2021

frame software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file demo/editablepanel/demopanel.hpp
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DEMO_PANEL_HPP
#define UISE_DESKTOP_DEMO_PANEL_HPP

#include <QCalendarWidget>

#include <uise/desktop/editablelabel.hpp>
#include <uise/desktop/editablepanelgrid.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

class DemoPanel : public EditablePanelGrid
{
    Q_OBJECT

    public:

        DemoPanel(QWidget* parent=nullptr) : EditablePanelGrid(parent)
        {
            auto text = new EditableLabelText(this);
            text->setValue("Hello world Hello world Hello world");
            addWidget("HHHH",text);

            auto integer = new EditableLabelInt(this);
            integer->editorWidget()->setMinimum(-100);
            integer->editorWidget()->setMaximum(100);
            integer->setValue(70);
            addWidget("07",integer);

            auto dbl = new EditableLabelDouble(this);
            dbl->editorWidget()->setMinimum(-100.00);
            dbl->editorWidget()->setMaximum(100.00);
            dbl->editorWidget()->setSingleStep(0.01);
            dbl->editorWidget()->setDecimals(4);
            dbl->setValue(50.00);
            addWidget("05",dbl);

            auto dateTime = new EditableLabelDateTime(this);
            dateTime->setValue(QDateTime::currentDateTime());
            dateTime->editorWidget()->setCalendarPopup(true);
            dateTime->editorWidget()->setCalendarWidget(new QCalendarWidget());
            addWidget("Date and time",dateTime);

            auto date= new EditableLabelDate(this);
            date->setValue(QDate::currentDate());
            date->editorWidget()->setCalendarPopup(true);
            date->editorWidget()->setCalendarWidget(new QCalendarWidget());
            addWidget("Date",date);

            auto time= new EditableLabelTime(this);
            time->setValue(QTime::currentTime());
            addWidget("Time",time);

            auto list= new EditableLabelList(this);
            list->editorWidget()->addItems({"one","two","three","four","five"});
            list->setValue("three");
            addWidget("List",list);

            auto simpleLable1=new QLabel("Ordinary label");
            addWidget("Simple label",simpleLable1);
            auto simpleLable2=new QLabel("label");
            addWidget("Simple label",simpleLable2);
        }
};

UISE_DESKTOP_NAMESPACE_END

//--------------------------------------------------------------------------

#endif // UISE_DESKTOP_DEMO_PANEL_HPP
