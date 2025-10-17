/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-comboed licenses.

*/

/****************************************************************************/

/** @file demo/elidedcontainer/main.cpp
*
*  Demo application of ElidedCOntainer.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QSplitter>
#include <QLabel>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/elidedcontainer.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    QMainWindow w;
    auto mainFrame=new QFrame();
    auto l = Layout::vertical(mainFrame);
    l->addStretch(1);

    auto splitter=new QSplitter();
    splitter->setStyleSheet("uise--PushButton {margin-left:4px;margin-right:4px;min-width:28px;}\n uise--PushButton QPushButton{icon-size: 20px;}\n #el41 {margin-right:20px;} \n uise--ElidedContainer {min-height: 24px;}");
    l->addWidget(splitter);

    auto frame1=new QFrame();
    auto l1=Layout::vertical(frame1);

    auto c1=new ElidedContainer(frame1);
    l1->addWidget(c1);
    auto el1=new ElidedLabel("One two three four five six seven eight nine ten 1");
    c1->addElidedLabel(el1);
    auto pb1=new PushButton();
    pb1->setSvgIcon(Style::instance().svgIconLocator().icon("StatusDialog::info"));
    c1->addWidget(pb1);

    auto el2=new ElidedLabel("One two three 2");
    c1->addElidedLabel(el2);
    auto pb2=new PushButton();
    pb2->setSvgIcon(Style::instance().svgIconLocator().icon("EditableLabel::edit"));
    c1->addWidget(pb2);
    auto el3=new ElidedLabel("Let's hide this 3");
    c1->addElidedLabel(el3);
    auto el4=new ElidedLabel("Hi there 4");
    c1->addElidedLabel(el4);
    el3->setVisible(false);

#if 1

    auto c2=new ElidedContainer(frame1);
    l1->addWidget(c2);
    auto el21=new ElidedLabel("One two three four five six seven eight nine ten 21");
    c2->addElidedLabel(el21);
    auto el22=new ElidedLabel("One two three");
    c2->addElidedLabel(el22);

    auto frame2=new QFrame();
    auto l2=Layout::vertical(frame2);

    auto c3=new ElidedContainer(frame2);
    l2->addWidget(c3);
    auto el31=new ElidedLabel("One two three four five six seven eight nine ten 31");
    c3->addElidedLabel(el31);
    auto pb31=new PushButton();
    pb31->setSvgIcon(Style::instance().svgIconLocator().icon("StatusDialog::info"));
    c3->addWidget(pb31);
    auto el32=new ElidedLabel("One two three 32");
    c3->addElidedLabel(el32);
    auto pb32=new PushButton();
    pb32->setSvgIcon(Style::instance().svgIconLocator().icon("EditableLabel::edit"));
    c3->addWidget(pb32);

    auto c4=new ElidedContainer(frame2);
    l2->addWidget(c4);
    auto el41=new ElidedLabel("One two three four five six seven eight nine ten 41");
    el41->setObjectName("el41");
    el41->setMinimumWidth(150);
    c4->addElidedLabel(el41);
    auto el42=new ElidedLabel("One two three four five 42");
    el42->setObjectName("el42");
    c4->addElidedLabel(el42);

    splitter->addWidget(frame1);
    splitter->addWidget(frame2);
#else
    splitter->addWidget(frame1);
#endif

    l->addStretch(1);
    w.setCentralWidget(mainFrame);
    w.resize(1200,800);
    w.setWindowTitle("Elided Container Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
