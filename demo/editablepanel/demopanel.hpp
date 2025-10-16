/**
@copyright Evgeny Sidorov 2021

frame software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-comboed licenses.

*/

/****************************************************************************/

/** @file demo/editablem_panel/demom_panel.hpp
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DEMO_PANEL_HPP
#define UISE_DESKTOP_DEMO_PANEL_HPP

#include <QCalendarWidget>

#include <uise/desktop/editablelabel.hpp>
#include <uise/desktop/editablepanelgrid.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

class DemoPanel : public WidgetQFrame
{
    Q_OBJECT

    public:

        DemoPanel(QWidget* parent=nullptr) : WidgetQFrame(parent)
        {
            auto l=Layout::vertical(this);

            m_panel=makeWidget<AbstractEditablePanel>(this);
            Q_ASSERT(m_panel);
            l->addWidget(m_panel);

            auto text = new EditableLabelText(m_panel);
            text->setValue("Hello world Hello world Hello world");
            m_panel->addRow("HHHH",text);

            auto integer = new EditableLabelInt(m_panel);
            integer->editorWidget()->setMinimum(-100);
            integer->editorWidget()->setMaximum(100);
            integer->setValue(70);
            m_panel->addRow("07",integer);

            auto dbl = new EditableLabelDouble(m_panel);
            dbl->editorWidget()->setMinimum(-100.00);
            dbl->editorWidget()->setMaximum(100.00);
            dbl->editorWidget()->setSingleStep(0.01);
            dbl->editorWidget()->setDecimals(4);
            dbl->setValue(50.00);
            m_panel->addRow("05",dbl);

            auto dateTime = new EditableLabelDateTime(m_panel);
            dateTime->setValue(QDateTime::currentDateTime());
            dateTime->editorWidget()->setCalendarPopup(true);
            dateTime->editorWidget()->setCalendarWidget(new QCalendarWidget());
            m_panel->addRow("Date and time",dateTime);

            auto date= new EditableLabelDate(m_panel);
            date->setValue(QDate::currentDate());
            date->editorWidget()->setCalendarPopup(true);
            date->editorWidget()->setCalendarWidget(new QCalendarWidget());
            m_panel->addRow("Date",date);

            auto time= new EditableLabelTime(m_panel);
            time->setValue(QTime::currentTime());
            m_panel->addRow("Time",time);

            auto combo= new EditableLabelCombo(m_panel);
            combo->editorWidget()->addItems({"one","two","three","four","five"});
            combo->setValue("three");
            m_panel->addRow("Combo",combo);

            auto simpleLable1=new QLabel("Ordinary label");
            m_panel->addRow("Simple label",simpleLable1);
            auto simpleLable2=new QLabel("label");
            m_panel->addRow("Simple label",simpleLable2);
        }

        void setTitle(const QString& title)
        {
            m_panel->setTitle(title);
        }

        void setButtonsMode(AbstractEditablePanel::ButtonsMode mode)
        {
            m_panel->setButtonsMode(mode);
        }

        void edit()
        {
            m_panel->edit();
        }

    private:

        AbstractEditablePanel* m_panel;
};

UISE_DESKTOP_NAMESPACE_END

//--------------------------------------------------------------------------

#endif // UISE_DESKTOP_DEMO_PANEL_HPP
