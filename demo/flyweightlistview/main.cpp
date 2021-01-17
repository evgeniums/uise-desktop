/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** \file demo/flyweightlistview/main.cpp
*
*  Demo application of FlyweightListView.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>

#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/flyweightlistview.ipp>

#include <uise/desktop/flyweightlistitem.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

struct ItemTraits : public FlyweightListItemTraits<QWidget*,QWidget*,QWidget*,std::string>
{
};

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    {
        QMainWindow w;
        auto v=new FlyweightListView<FlyweightListItem<ItemTraits>>();
        w.setCentralWidget(v);
        w.show();
        app.exec();
    }

    return 0;
}

//--------------------------------------------------------------------------
