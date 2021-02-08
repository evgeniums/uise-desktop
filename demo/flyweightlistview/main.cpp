/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file demo/flyweightlistview/main.cpp
*
*  Demo application of FlyweightListView.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>

#include <fwlvtestwidget.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    {
        QMainWindow w;
        auto v=new FwlvTestWidget();
        w.setCentralWidget(v);
        v->setFocus();
        w.resize(1000,800);
        w.setWindowTitle("FlyweightListView Demo");
        w.show();

        SingleShotTimer load;
        load.shot(0,
            [v]
            {
                v->loadItems();
            }
        );

        app.exec();
    }

    return 0;
}

//--------------------------------------------------------------------------
