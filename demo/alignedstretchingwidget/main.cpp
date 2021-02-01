/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

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

//    auto mainFrame=new QFrame();

//    auto vertical=new QVBoxLayout(mainFrame);

//    auto hCenterEd=new QTextEdit();
//    hCenterEd->setMaximumWidth(700);
//    hCenterEd->setMinimumWidth(300);
//    auto hCenter=new AlignedStretchingWidget();
//    hCenter->setWidget(hCenterEd,Qt::Horizontal,Qt::AlignHCenter);
//    vertical->addWidget(hCenter,1);

//    auto hRightEd=new QTextEdit();
//    hRightEd->setMaximumWidth(700);
//    hRightEd->setMinimumWidth(300);
//    auto hRight=new AlignedStretchingWidget();
//    hRight->setWidget(hRightEd,Qt::Horizontal,Qt::AlignRight);
//    vertical->addWidget(hRight,1);

//    auto hLeftEd=new QTextEdit();
//    hLeftEd->setMaximumWidth(700);
//    hLeftEd->setMinimumWidth(300);
//    auto hLeft=new AlignedStretchingWidget();
//    hLeft->setWidget(hLeftEd,Qt::Horizontal,Qt::AlignLeft);
//    vertical->addWidget(hLeft,1);

//    auto hFrame=new QFrame();
//    auto horizontal=new QHBoxLayout(hFrame);
//    vertical->addWidget(hFrame,2);

//    auto vCenterEd=new QTextEdit();
//    vCenterEd->setMinimumHeight(32);
//    vCenterEd->setMaximumHeight(300);
//    auto vCenter=new AlignedStretchingWidget();
//    vCenter->setWidget(vCenterEd,Qt::Vertical,Qt::AlignCenter);
//    horizontal->addWidget(vCenter,1);

    auto mainFrame=new AlignedStretchingWidgetDemo();

    w.setCentralWidget(mainFrame);
    w.resize(600,500);
    w.setWindowTitle("AlignedStretchingWidget Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
