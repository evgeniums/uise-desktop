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
#include <uise/desktop/framewithrefresh.hpp>
#include <uise/desktop/stackwithnavigationbar.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

FrameWithRefresh* createPage(QWidget* parent, const QString& text)
{
    auto page=new FrameWithRefresh(parent);
    auto l=Layout::vertical(page);

    auto editor=new QTextEdit(page);
    editor->setText(text);
    l->addWidget(editor,1);

    return page;
}

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
    l->addWidget(topFrame);
    auto tl=Layout::horizontal(topFrame);
    auto testFrame=new QFrame();
    testFrame->setObjectName("testFrame");
    testFrame->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    tl->addWidget(testFrame);
    auto tfl=Layout::horizontal(testFrame);
    auto testWidget=new StackWithNavigationBar(testFrame);
    tfl->addWidget(testWidget);

    auto p1=createPage(testWidget,"Text for Page 1");
    testWidget->addWidget(p1,"Page 1","Tooltip for page 1");

    auto p2=createPage(testWidget,"Text for Page 2");
    testWidget->addWidget(p2,"Page 2","Tooltip for page 2");

    auto p3=createPage(testWidget,"Text for Page 3");
    testWidget->addWidget(p3,"Page 3","Tooltip for page 3");

    auto p4=createPage(testWidget,"Text for Page 4");
    testWidget->addWidget(p4,"Page 4","Tooltip for page 4");

    auto p5=createPage(testWidget,"Text for Page 5");
    testWidget->addWidget(p5,"Page 5","Tooltip for page 5");

    auto p6=createPage(testWidget,"Text for Page 6");
    testWidget->addWidget(p6,"Page 6","Tooltip for page 6");

    auto p7=createPage(testWidget,"Text for Page 7");
    testWidget->addWidget(p7,"Page 7","Tooltip for page 7");

    auto p8=createPage(testWidget,"Text for Page 8");
    testWidget->addWidget(p8,"Page 8","Tooltip for page 8");

    auto p9=createPage(testWidget,"Text for Page 9");
    testWidget->addWidget(p9,"Page 9","Tooltip for page 9");

    auto p10=createPage(testWidget,"Text for Page 10");
    testWidget->addWidget(p10,"Page 10","Tooltip for page 10");

    auto bottomFrame=new QFrame();
    bottomFrame->setObjectName("bottomFrame");
    l->addWidget(bottomFrame);
    auto bl=Layout::grid(bottomFrame,false);

    auto text=new QLineEdit(bottomFrame);
    text->setText("new page");
    bl->addWidget(new QLabel("Name:"),0,0,Qt::AlignRight);
    bl->addWidget(text,0,1,1,2);

    bl->addWidget(new QLabel("Index:"),0,3,Qt::AlignRight);
    auto index=new QSpinBox(bottomFrame);
    index->setRange(0,100);
    index->setValue(3);
    bl->addWidget(index,0,4);

    int addCount=1;
    auto add=new QPushButton("Add");
    bl->addWidget(add,1,0);
    QObject::connect(
        add,
        &QPushButton::clicked,
        testWidget,
        [testWidget,text,&addCount]()
        {
            auto name=QString("Added %1 ").arg(addCount)+text->text();
            auto p10=createPage(testWidget,QString("Text for added page %1").arg(text->text()));
            testWidget->addWidget(p10,name,QString("Tooltip for page %1").arg(text->text()));
            addCount++;
        }
    );

    int replaceCount=1;
    auto replace=new QPushButton("Replace");
    bl->addWidget(replace,1,1);
    QObject::connect(
        replace,
        &QPushButton::clicked,
        testWidget,
        [testWidget,text,index,&replaceCount]()
        {
            auto name=QString("Replaced %1 ").arg(replaceCount)+text->text();
            auto p10=createPage(testWidget,QString("Text for replaced page %1").arg(text->text()));
            testWidget->replaceWidget(p10,name,index->value(),QString("Tooltip for page %1").arg(text->text()));
            replaceCount++;
        }
    );

    auto truncate=new QPushButton("Truncate");
    bl->addWidget(truncate,1,2);
    QObject::connect(
        truncate,
        &QPushButton::clicked,
        testWidget,
        [testWidget,index]()
        {
            testWidget->closeWidget(index->value());
        }
    );

    auto clear=new QPushButton("Clear");
    bl->addWidget(clear,1,3);
    QObject::connect(
        clear,
        &QPushButton::clicked,
        testWidget,
        &StackWithNavigationBar::clear
    );

    QString qss="*{color: #FFFFFF;} \n"
                  "QFrame {background-color: gray;} \n"
                  "QFrame#mainFrame {background-color:blue;margin:0;padding:0;}\n"
                  "QFrame#topFrame {background-color:green;margin:0;}\n"
                  "uise--NavigationBarPanel {padding:0;margin:0;}"
                  "uise--NavigationBar {background-color:white;padding:0;margin:0;}\n"
                  "uise--NavigationBar QScrollArea {padding:0;margin:0;border:none;}\n"
                  "uise--NavigationBar QToolButton {padding:0;margin:0;border:none;font-size:12px;text-decoration:underline;}\n"
                  "uise--NavigationBar QToolButton:hover:!checked {color:#EEEEEE;}\n"
                  "uise--NavigationBar QToolButton:checked {text-decoration:none;}\n"
                  "uise--NavigationBar QScrollBar {margin:0;padding:0;}\n"
        ;
    qApp->setStyleSheet(qss);

    //! @todo Test signals widgetSelected(), currentChanged(), refreshRequested()

    w.setCentralWidget(mainFrame);
    w.resize(300,400);
    w.setWindowTitle("Stack with navigation bar Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
