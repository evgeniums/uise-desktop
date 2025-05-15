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

    HTreeNodeFactory factory;
    factory.registerBuilder(DirListItem::Folder,std::make_shared<FolderNodeBuilder>());
    factory.registerBuilder(DirListItem::File,std::make_shared<FileNodeBuilder>());

    auto htree=new HTree(mainFrame);
    htree->setNodeFactory(&factory);
    l->addWidget(htree,1);

    QObject::connect(
        htree,
        &HTree::newTreeRequested,
        htree,
        [&factory,&w](const UISE_DESKTOP_NAMESPACE::HTreePath& path)
        {
            auto htree=new HTree();
            htree->setNodeFactory(&factory);
            htree->openPath(path);
            htree->sidebar()->setVisible(false);
            htree->resize(800,600);
            htree->show();
            htree->raise();
            htree->move(w.pos().x()+50,w.pos().y()-50);
            htree->setWindowTitle("HTree filesystem Demo");
        }
    );

    auto rootPath=std::filesystem::current_path().root_path();
    auto rootPathName=rootPath.string();
    HTreePathElement root{DirListItem::Folder,rootPath.string(),rootPathName};
    htree->openPath(root);
    htree->sidebar()->setVisible(false);

    QString qss=""
                  // "QFrame#mainFrame {margin:0;padding:0;}\n"
                  // "QFrame#topFrame {margin:0;}\n"
                  "uise--NavigationBarPanel {padding:0;margin:0;}"
                  "uise--NavigationBar {padding:12px 0;margin:0px;}\n"
                  "uise--NavigationBar QScrollArea {padding:0;margin:0;border:none;}\n"
                  // "uise--NavigationBar QToolButton {padding:4;margin:4;border:none;font-size:16px;color: #ff8828;}\n"
                  // "uise--NavigationBar QToolButton:hover:!checked {color: #ff9939;}\n"

                  "uise--NavigationBarSeparator {color:#888888;}\n"
                  "uise--NavigationBar QToolButton {padding:4;margin:4;border:none;font-size:12px;color:#888888;}\n"
                  "uise--NavigationBar QToolButton:hover:!checked {color: #aaaaaa;}\n"

                  "uise--NavigationBar QToolButton:checked {text-decoration:none;}\n"
                  "uise--NavigationBar QScrollBar {margin:0;padding:0;}\n"
                  // "uise--ElidedLabel {background-color:#CC9988;}\n"
                  "#hTreeItemPixmap {margin-right: 8px;}\n"
                  // "#hTreeItemText {padding-left: 8px;}\n"
                  // "uise--HTreeStansardListItem {background-color:green;padding:0;margin:10;border: 1px solid black;}\n"
                  "uise--HTreeStansardListItem QLabel{padding:0;margin:4px 0;}\n"
                  // "uise--HTreeSplitterInternal {background-color:green;margin:16px;}\n"
                  // "#DirList {background-color:blue;}\n"
                  // "#DirListView {background-color:white;}\n"
                  // "#DirListItem {padding:0;margin:0;background:#a0a0a0;}\n"
                  "#DirListItem[hover=\"true\"] {background-color:#C4C4C4;}\n"
                  "#DirListItem[selected=\"true\"] {background-color:#B0B0B0;}\n"
                  // "uise--HTreeList {background-color:grey;}\n"
        ;
    qApp->setStyleSheet(qss);

    w.setCentralWidget(mainFrame);
    w.resize(800,600);
    w.setWindowTitle("HTree filesystem Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
