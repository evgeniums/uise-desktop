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

/** @file demo/stackwithnavigationbar/main.cpp
*
*  Demo application of BusyWaiting.
*
*/

/****************************************************************************/

#include <QtMath>
#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QTextEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/htree.hpp>
#include <uise/desktop/htreenodefactory.hpp>
#include <uise/desktop/htreesidebar.hpp>
#include "dirlist.hpp"

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    QMainWindow w;
    auto mainFrame=new QFrame();
    mainFrame->setObjectName("mainFrame");

    auto l = Layout::vertical(mainFrame);

    auto topFrame=new QFrame();
    topFrame->setObjectName("topFrame");
    l->addWidget(topFrame,0);
    // auto tl=Layout::horizontal(topFrame);

    HTreeNodeFactory factory;
    factory.registerBuilder(DirListItem::Folder,std::make_shared<FolderNodeBuilder>());

    auto htree=new HTree(mainFrame);
    htree->setNodeFactory(&factory);
    l->addWidget(htree,1);
    auto rootPath=std::filesystem::current_path().root_path();
    auto rootPathName=rootPath.string();
    HTreePathElement root{DirListItem::Folder,rootPath,rootPathName};
    htree->openPath(root);
    htree->sidebar()->setVisible(false);

    QString qss=""
                  // "QFrame#mainFrame {margin:0;padding:0;}\n"
                  // "QFrame#topFrame {margin:0;}\n"
                  "uise--NavigationBarPanel {padding:0;margin:0;}"
                  "uise--NavigationBar {padding:0;margin:0;}\n"
                  "uise--NavigationBar QScrollArea {padding:0;margin:0;border:none;}\n"
                  "uise--NavigationBar QToolButton {padding:0;margin:0;border:none;font-size:12px;text-decoration:underline;}\n"
                  "uise--NavigationBar QToolButton:hover:!checked {color:#999999;}\n"
                  "uise--NavigationBar QToolButton:checked {text-decoration:none;}\n"
                  "uise--NavigationBar QScrollBar {margin:0;padding:0;}\n"
                  "#hTreeItemText {margin: 8px;}\n"
                  // "uise--HTree {background-color:green;}\n"
                  // "#DirList {background-color:blue;}\n"
                  // "#DirListView {background-color:white;}\n"
                  // "#DirListItem {background-color:orange;}\n"
                  // "uise--HTreeList {background-color:grey;}\n"
        ;
    qApp->setStyleSheet(qss);

    w.setCentralWidget(mainFrame);
    w.resize(300,400);
    w.setWindowTitle("HTree filesystem Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
