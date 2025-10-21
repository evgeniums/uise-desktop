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

#include <QFrame>
#include <QPushButton>
#include <QTextEdit>
#include <QScrollArea>
#include <QApplication>
#include <QMainWindow>
#include <QTimer>

#include <uise/desktop/style.hpp>
#include <uise/desktop/widgetfactory.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/editablepanel.hpp>
#include <uise/desktop/editablepanels.hpp>

#include "demopanel.hpp"

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    QMainWindow w;
    auto scArea=new QScrollArea();
    auto mainFrame=new QFrame();
    scArea->setWidget(mainFrame);
    scArea->setWidgetResizable(true);

    auto l = Layout::vertical(mainFrame);

    auto panels=Style::instance().widgetFactory()->makeWidget<AbstractEditablePanels>(mainFrame);
    l->addWidget(panels);

    auto panel1= new DemoPanel();
    panels->addPanel(panel1);
    QObject::connect(
        panel1->panel(),
        &AbstractEditablePanel::applyRequested,
        mainFrame,
        [panel1]()
        {
            panel1->panel()->setBusyWaiting(true);
            QTimer::singleShot(3000,
               panel1,
               [panel1]()
               {
                    panel1->panel()->commitApply();
               }
            );
        }
    );

    auto panel2= new DemoPanel();
    panels->addPanel(panel2);
    panel2->setTitle("Panel 2");
    QObject::connect(
        panel2->panel(),
        &AbstractEditablePanel::applyRequested,
        mainFrame,
        [panel2]()
        {
            panel2->panel()->setBusyWaiting(true);
            QTimer::singleShot(2000,
               panel2,
               [panel2]()
               {
                   panel2->panel()->showError("This is error message");
               }
            );
        }
    );

    auto panel3= new DemoPanel();
    panels->addPanel(panel3);
    panel3->setTitle("Panel 3");
    panel3->setButtonsMode(EditablePanel::ButtonsMode::BottomAlwaysVisible);
    panel3->edit();
    QObject::connect(
        panel3->panel(),
        &AbstractEditablePanel::applyRequested,
        mainFrame,
        [panel3]()
        {
            panel3->panel()->commitApply();
        }
    );

    auto panel4= new DemoPanel();
    panels->addPanel(panel4);
    panel4->setTitle("Panel 4");
    panel4->setButtonsMode(EditablePanel::ButtonsMode::TopOnHoverVisible);

    auto style=new QTextEdit();
    style->setMinimumHeight(200);
    l->addWidget(style);
    auto applyStyle=new QPushButton("Apply style");
    l->addWidget(applyStyle);
    applyStyle->connect(
        applyStyle,
        &QPushButton::clicked,
        mainFrame,
        [mainFrame,style]()
        {
            mainFrame->setStyleSheet(style->toPlainText());
        }
    );

    w.setCentralWidget(scArea);
    w.resize(1000,600);
    w.setWindowTitle("Editable Panel Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
