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

    Style::instance().svgIconTheme().addNameMapping("HTreeNodeTitleBar::close","x");
    Style::instance().svgIconTheme().addNameMapping("HTreeNodeTitleBar::collapse","minus");
    // Style::instance().svgIconTheme().addNameMapping("HTreeNodeTitleBar::refresh","refresh");
    Style::instance().svgIconTheme().addNamePath("HTreeNodePlaceHolder::dots",":/uise/tabler-icons/outline/dots-vertical.svg");
    Style::instance().svgIconTheme().addIconDir(":/uise/tabler-icons/outline");

    Style::instance().svgIconTheme().addColorMap(
            SvgIcon::ColorMap{
                {{"currentColor","#CCCCCC"}},
                {{"currentColor","#999999"}}
            },
            "HTreeNodeTitleBar"
        );

    Style::instance().svgIconTheme().addColorMap(
        SvgIcon::ColorMap{
            {{"currentColor","#FFFFFF"}}
        },
        "HTreeNodeTitleBar",
        IconMode::Hovered
    );

    Style::instance().svgIconTheme().loadIcons(
        {SvgIconTheme::IconConfig{"HTreeNodePlaceHolder::dots",
                                  {
                                    {IconMode::Normal,SvgIcon::ColorMap{
                                                          {{"currentColor","#777777"}}
                                                      }
                                    },
                                    {IconMode::Hovered,SvgIcon::ColorMap{
                                                             {{"currentColor","#444444"}}
                                                         }
                                   }
                                  }
                                }}
    );

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

    QString colorBBB="#BBBBBB";
    QString color888="#888888";
    QString colorSplitterLine="#888888";
    if (!Style::instance().checkDarkTheme())
    {
        colorBBB="#444444";
        color888="#888888";
        colorSplitterLine="#DDDDDD";
    }

    QString qss=""
                  "uise--NavigationBarPanel {padding:0;margin:0;background-color:transparent;}"
                  "uise--NavigationBar {padding:12px 0;margin:0px;}\n"
                  // "uise--NavigationBar QScrollArea {padding:0;margin:0;border:none;background-color:blue;}\n"

                  "uise--NavigationBarSeparator {color:%2;}\n"
                  "uise--NavigationBarSeparator[hover=\"true\"] {color:%1;}\n"
                  "uise--NavigationBar QToolButton {padding:4;margin:4;border:none;font-size:12px;color:%2;background-color:transparent;text-decoration:underline;}\n"
                  "uise--NavigationBar QToolButton:hover:!checked {color: %1;}\n"
                  "uise--NavigationBar QToolButton:checked {color: %1;text-decoration:none;}\n"

                  "uise--NavigationBar QScrollBar {margin:0;padding:0;}\n"
                  "#hTreeItemPixmap {margin-right: 8px;}\n"
                  "uise--HTreeStansardListItem {padding-left:4px;}\n"
                  "uise--HTreeStansardListItem QLabel{padding:0;margin:4px 0;}\n"
                  "uise--HTreeSplitterLine {background-color:%3;border:none;}"

                  "uise--HTreeSplitterSection {padding:0;}"
                  "uise--HTreeList {padding:0;margin:0;}"
                  "uise--HTreeNode {padding:0;margin:0;}"

                  // "#DirList {padding-left:4px;}\n"
                  "#DirListItem[hover=\"true\"] {background-color:#C4C4C4;}\n"
                  "#DirListItem[selected=\"true\"] {background-color:#B0B0B0;}\n"
                  // "uise--ElidedLabel {background-color:blue;}\n"

                  // "uise--HTreeSplitterInternal {border:none;padding: 0px; margin: 0px; background-color: blue;}"
                  "#hTreeSplitterScArea {border:none;}\n"

                  "uise--HTreeNodeTitleBar QPushButton {margin:0;padding:4px 0;border:none;icon-size:12px;}\n"
                  "uise--HTreeNodeTitleBar uise--ElidedLabel {margin:0; margin-left: 8px;}\n"
                  "uise--HTreeNodeTitleBar uise--ElidedLabel QLabel {color:white;}\n"
                  "uise--HTreeNodeTitleBar {border: 1px solid #999999; border-right:none; border-left:none; background-color: #888888; padding:0;}\n"
                  "uise--HTreeNodeTitleBar {padding:0;margin:0;}"

                  "uise--HTreeNodePlaceHolder {min-width:7px;max-width:7px;padding-right: 1px; background-color: #C8C8C0;}"
                  "uise--HTreeNodePlaceHolder QPushButton {border:none;background-color: #C8C8C0;icon-size: 20px 8px;}"
        ;    
    qss=qss.arg(colorBBB,color888,colorSplitterLine);
    qApp->setStyleSheet(qss);

    w.setCentralWidget(mainFrame);
    w.resize(800,600);
    w.setWindowTitle("HTree Filesystem Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
