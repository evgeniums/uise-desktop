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

/** @file demo/loadingframe/main.cpp
*
*  Demo application of FrameWithModalStatus.
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
#include <QComboBox>
#include <QMenu>
#include <QMessageBox>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/loadingframe.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    QMainWindow w;
    auto mainFrame=new QFrame();

    QString darkTheme="QTextEdit,QLineEdit {background-color: black;color:white;} \n uise--FrameWithModalPopup {background-color: black;}";
    QString lightTheme="QTextEdit,QLineEdit {background-color: white;color:black;} \n uise--FrameWithModalPopup {background-color: lightgrey;}";

    Style::instance().applyStyleSheet();

    auto l = Layout::vertical(mainFrame,false);

    auto topFrame=new QFrame();
    l->addWidget(topFrame);
    auto tl=Layout::horizontal(topFrame,false);
    auto testFrame=new QFrame();
    testFrame->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    tl->addWidget(testFrame);
    auto tfl=Layout::horizontal(testFrame,false);
    auto testWidget=new LoadingFrame(testFrame);
    testWidget->construct();
    tfl->addWidget(testWidget);
    auto wl=Layout::vertical(testWidget,false);
    auto editor1=new QTextEdit();
    wl->addWidget(editor1);
    editor1->setText("Lorem ipsum dolor sit amet. Id enim repudiandae quo nulla quia id quisquam tempore et nostrum illum. Ex inventore repellendus et nobis molestiae aut facere dolores eos quod totam qui galisum quaerat ex illo aspernatur in quaerat quaerat. Ex unde galisum est omnis iure et ducimus suscipit in maiores officiis aut voluptas molestiae et fuga quia?");

    auto configFrame=new QFrame();
    tl->addWidget(configFrame,0,Qt::AlignTop);
    auto confl=Layout::grid(configFrame,false);

    int i=0;

    auto styleButton=new QPushButton();
    styleButton->setText("Light theme");
    styleButton->setCheckable(true);
    if (Style::instance().checkDarkTheme())
    {
        styleButton->setChecked(true);
        styleButton->setText("Dark theme");
    }
    QObject::connect(styleButton,&QPushButton::toggled,testFrame,[&darkTheme,&lightTheme,testFrame,styleButton](bool enable){
        if (enable)
        {
            Style::instance().setBaseQss(darkTheme);
            Style::instance().setColorTheme(Style::DarkTheme);
            Style::instance().applyStyleSheet(true);
            styleButton->setText("Dark theme");
        }
        else
        {
            Style::instance().setBaseQss(lightTheme);
            Style::instance().setColorTheme(Style::LightTheme);
            Style::instance().applyStyleSheet(true);
            styleButton->setText("Light theme");
        }
        testFrame->style()->unpolish(testFrame);
        testFrame->style()->polish(testFrame);
    });
    confl->addWidget(styleButton,i,0,1,2);

    auto bottomFrame=new QFrame();
    l->addWidget(bottomFrame);
    auto bl=Layout::horizontal(bottomFrame,false);

    auto start=new QPushButton("Show busy");
    bl->addWidget(start);
    QObject::connect(start,&QPushButton::clicked,testWidget,
    [testWidget](){
            testWidget->popupBusyWaiting();
        }
    );

    auto hide=new QPushButton("Finish busy");
    bl->addWidget(hide);
    QObject::connect(hide,&QPushButton::clicked,testWidget,&LoadingFrame::finish);

    auto cancellableButton=new QPushButton();
    cancellableButton->setText("Uncancellable");
    cancellableButton->setCheckable(true);
    QObject::connect(cancellableButton,&QPushButton::toggled,testWidget,[testWidget,cancellableButton](bool enable){
        if (enable)
        {
            cancellableButton->setText("Cancellable");
        }
        else
        {
            cancellableButton->setText("Uncancellable");
        }
        testWidget->setCancellableBusyWaiting(enable);
    });
    bl->addWidget(cancellableButton);

    testWidget->setAutoColor(false);

    w.setCentralWidget(mainFrame);
    w.resize(1200,800);
    w.setWindowTitle("Frame With Loading Spinner Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
