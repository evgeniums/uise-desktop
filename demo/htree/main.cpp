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
#include <uise/desktop/style.hpp>
#include <uise/desktop/htree.hpp>
#include <uise/desktop/htreenodelocator.hpp>
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

    auto factory=std::make_shared<HTreeNodeFactory>();
    factory->registerBuilder(DirListItem::Folder,std::make_shared<FolderNodeBuilder>());
    factory->registerBuilder(DirListItem::File,std::make_shared<FileNodeBuilder>());

    auto nodeLocator=std::make_shared<HTreeNodeLocator>(factory);

    auto htree=new HTree(nodeLocator.get(),mainFrame);
    l->addWidget(htree,1);

    int winCount=1;
    QObject::connect(
        htree,
        &HTree::newTreeRequested,
        htree,
        [nodeLocator,&w,&winCount](const UISE_DESKTOP_NAMESPACE::HTreePath& path)
        {
            auto htree=new HTree(nodeLocator.get());
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

    Style::instance().reloadStyleSheet();
    Style::instance().applyQss();

    w.setCentralWidget(mainFrame);
    w.resize(800,600);
    w.setWindowTitle("HTree Filesystem Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
