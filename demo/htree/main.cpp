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

    int winCount=1;
    QObject::connect(
        htree,
        &HTree::newTreeRequested,
        htree,
        [&factory,&w,&winCount](const UISE_DESKTOP_NAMESPACE::HTreePath& path)
        {
            auto htree=new HTree();
            htree->setNodeFactory(&factory);
            htree->openPath(path);
            htree->sidebar()->setVisible(false);
            htree->resize(800,600);
            htree->show();
            htree->raise();
            htree->move(w.pos().x()+50,w.pos().y()-50);
            htree->setWindowTitle(QString("HTree Filesystem Demo (%1)").arg(winCount));
        }
    );

    auto rootPath=std::filesystem::current_path().root_path();
    auto rootPathName=rootPath.string();
    HTreePathElement root{DirListItem::Folder,rootPath.string(),rootPathName};
    htree->openPath(root);
    htree->sidebar()->setVisible(false);

    QString qss=""
                  "uise--NavigationBarPanel {padding:0;margin:0;background-color:transparent;}"
                  "uise--NavigationBar {padding:12px 0;margin:0px;}\n"
                  "uise--NavigationBar QScrollArea {padding:0;margin:0;border:none;background-color:blue;}\n"

                  "uise--NavigationBarSeparator {color:#BBBB44;}\n"
                  "uise--NavigationBar QToolButton {padding:4;margin:4;border:none;font-size:12px;color:#BBBB44;background-color:transparent;}\n"
                  "uise--NavigationBar QToolButton:hover:!checked {color: #DDDD88;}\n"

                  "uise--NavigationBar QToolButton:checked {text-decoration:none;}\n"
                  "uise--NavigationBar QScrollBar {margin:0;padding:0;}\n"
                  "#hTreeItemPixmap {margin-right: 8px;}\n"
                  "uise--HTreeStansardListItem QLabel{padding:0;margin:4px 0;color: white;}\n"
                  "uise--HTreeSplitterLine {background-color:#B0B0B0;}"

                  "#DirList {background-color:blue;}\n"
                  "#DirListItem[hover=\"true\"] {background-color:#C4C4C4;}\n"
                  "#DirListItem[selected=\"true\"] {background-color:#B0B0B0;}\n"
                  "uise--HTree {background-color:blue;}\n"

                  "uise--HTreeSplitterInternal {border:none;padding: 0px; margin: 0px; background-color: blue;}"
                  // "uise--HTreeSplitter {border:none;background-color: orange; padding:0; margin:0;}\n"
                  "#hTreeSplitterScArea {border:none;}\n"
                  // "#hTreeSplitterWrapper {border:none;background-color: yellow; margin:0;padding:0;}\n"

                  // "#hTreeTabs:pane,#hTreeTabs {background-color: green;}\n"
        ;
    qApp->setStyleSheet(qss);

    w.setCentralWidget(mainFrame);
    w.resize(800,600);
    w.setWindowTitle("HTree Filesystem Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
