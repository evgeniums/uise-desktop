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

/** @file demo/modalpopup/main.cpp
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
#include <uise/desktop/framewithmodalstatus.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    QMainWindow w;
    auto mainFrame=new QFrame();

    auto l = Layout::vertical(mainFrame,false);

    auto topFrame=new QFrame();
    l->addWidget(topFrame);
    auto tl=Layout::horizontal(topFrame,false);
    auto testFrame=new QFrame();
    testFrame->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    tl->addWidget(testFrame);
    auto tfl=Layout::horizontal(testFrame,false);
    auto testWidget=new FrameWithModalStatus(testFrame);
    tfl->addWidget(testWidget);
    auto wl=Layout::vertical(testWidget,false);
    auto editor1=new QTextEdit();
    wl->addWidget(editor1);
    editor1->setText("Lorem ipsum dolor sit amet. Id enim repudiandae quo nulla quia id quisquam tempore et nostrum illum. Ex inventore repellendus et nobis molestiae aut facere dolores eos quod totam qui galisum quaerat ex illo aspernatur in quaerat quaerat. Ex unde galisum est omnis iure et ducimus suscipit in maiores officiis aut voluptas molestiae et fuga quia?");

    auto configFrame=new QFrame();
    tl->addWidget(configFrame,0,Qt::AlignTop);
    auto confl=Layout::grid(configFrame,false);

    int i=0;

    auto alpha=new QSpinBox();
    alpha->setRange(0,255);
    alpha->setValue(FrameWithModalPopup::DefaultPopupAlpha);
    confl->addWidget(new QLabel("Alpha"),i,0);
    confl->addWidget(alpha,i,1);
    QObject::connect(alpha,&QSpinBox::valueChanged,testWidget,&FrameWithModalPopup::setPopupAlpha);
    i++;

    auto height=new QSpinBox();
    height->setRange(0,100);
    height->setValue(FrameWithModalPopup::DefaultMaxHeightPercent);
    confl->addWidget(new QLabel("Height percent"),i,0);
    confl->addWidget(height,i,1);
    QObject::connect(height,&QSpinBox::valueChanged,testWidget,&FrameWithModalPopup::setMaxHeightPercent);
    i++;

    auto width=new QSpinBox();
    width->setRange(0,100);
    width->setValue(FrameWithModalPopup::DefaultMaxWidthPercent);
    confl->addWidget(new QLabel("Width percent"),i,0);
    confl->addWidget(width,i,1);
    QObject::connect(width,&QSpinBox::valueChanged,testWidget,&FrameWithModalPopup::setMaxWidthPercent);
    i++;

    auto bottomFrame=new QFrame();
    l->addWidget(bottomFrame);
    auto bl=Layout::horizontal(bottomFrame,false);

    auto start=new QPushButton("Show busy waiting");
    bl->addWidget(start);
    QObject::connect(start,&QPushButton::clicked,testWidget,
    [testWidget](){
            testWidget->popupBusyWaiting();
        }
    );

    auto hide=new QPushButton("Finish");
    bl->addWidget(hide);
    QObject::connect(hide,&QPushButton::clicked,testWidget,&FrameWithModalStatus::finish);

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

    QString darkTheme=""
                        "*{font-size: 20px;} \n QTextEdit,QLineEdit {background-color: black;color:white;} \n uise--FrameWithModalPopup {background-color: black;}"
                        "uise--FrameWithModalStatus #buttonsFrame uise--PushButton QPushButton{color: #CCCCCC;border: 1px solid #999999; border-radius: 4px; margin:4px; padding: 4px 16px;}\n"
                        "uise--FrameWithModalStatus #buttonsFrame uise--PushButton QPushButton:hover{color: #EEEEEE;border: 1px solid #EEEEEE;}\n"
                    "";
    QString lightTheme=""
                         "*{font-size: 20px;} \n QTextEdit,QLineEdit {background-color: white;color:black;} \n uise--FrameWithModalPopup {background-color: lightgrey;}"
                         "uise--FrameWithModalStatus #buttonsFrame uise--PushButton QPushButton{color: #555555;border: 1px solid #999999; border-radius: 4px; margin:4px; padding: 4px 16px;}\n"
                         "uise--FrameWithModalStatus #buttonsFrame uise--PushButton QPushButton:hover{color: #444444;border: 1px solid #444444;}\n"
                         "";
    auto styleButton=new QPushButton();
    styleButton->setText("Dark theme");
    styleButton->setCheckable(true);
    QObject::connect(styleButton,&QPushButton::toggled,testFrame,[&darkTheme,&lightTheme,testFrame,styleButton](bool enable){
        if (enable)
        {
            testFrame->setStyleSheet(darkTheme);
            styleButton->setText("Light theme");
        }
        else
        {
            testFrame->setStyleSheet(lightTheme);
            styleButton->setText("Dark theme");
        }
        testFrame->style()->unpolish(testFrame);
        testFrame->style()->polish(testFrame);
    });
    bl->addWidget(styleButton);

    testFrame->setStyleSheet(lightTheme);
    testWidget->setPopupAlpha(alpha->value());
    testWidget->setMaxWidthPercent(width->value());
    testWidget->setMaxHeightPercent(height->value());

    w.setCentralWidget(mainFrame);
    w.resize(1200,800);
    w.setWindowTitle("Modal popup Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
