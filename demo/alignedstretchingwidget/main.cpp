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

/** @file demo/alignedstretchingwidget/main.cpp
*
*  Demo application of AlignedStretchingWidget.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>

#include <QVBoxLayout>
#include <QTextBrowser>

#include <uise/desktop/alignedstretchingwidget.hpp>
#include "alignedstretchingwidgetdemo.hpp"

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    QMainWindow w;
    auto mainFrame=new AlignedStretchingWidgetDemo();

    w.setCentralWidget(mainFrame);
    w.resize(600,500);
    w.setWindowTitle("AlignedStretchingWidget Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
