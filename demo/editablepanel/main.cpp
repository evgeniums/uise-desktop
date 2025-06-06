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

#include <iostream>

#include <QApplication>
#include <QMainWindow>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/editablepanel.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    QMainWindow w;
    auto mainFrame=new QFrame();

    auto l = Layout::vertical(mainFrame);

    auto panel= new EditablePanel();
    l->addWidget(panel);

    w.setCentralWidget(mainFrame);
    w.resize(600,500);
    w.setWindowTitle("EditableLabel Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
