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
#include <uise/desktop/modalpopup.hpp>

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
    auto testWidget=new FrameWithModalPopup(testFrame);
    tfl->addWidget(testWidget);
    auto wl=Layout::vertical(testWidget,false);
    auto editor1=new QTextEdit();
    wl->addWidget(editor1);
    editor1->setText("hkjhas cljhglidsc lhgluisadb liuydfi liuylsiaudfyis fsadiuyaidfy;iu");

    auto configFrame=new QFrame();
    tl->addWidget(configFrame,0,Qt::AlignTop);
    auto confl=Layout::grid(configFrame,false);

    int i=0;

    auto alpha=new QSpinBox();
    alpha->setRange(0,255);
    alpha->setValue(150);
    confl->addWidget(new QLabel("Alpha"),i,0);
    confl->addWidget(alpha,i,1);
    QObject::connect(alpha,&QSpinBox::valueChanged,testWidget,&FrameWithModalPopup::setPopupAlpha);
    i++;

    auto height=new QSpinBox();
    height->setRange(0,100);
    height->setValue(50);
    confl->addWidget(new QLabel("Height percent"),i,0);
    confl->addWidget(height,i,1);
    QObject::connect(height,&QSpinBox::valueChanged,testWidget,&FrameWithModalPopup::setMaxHeightPercent);
    i++;

    auto width=new QSpinBox();
    width->setRange(0,100);
    width->setValue(50);
    confl->addWidget(new QLabel("Width percent"),i,0);
    confl->addWidget(width,i,1);
    QObject::connect(width,&QSpinBox::valueChanged,testWidget,&FrameWithModalPopup::setMaxWidthPercent);
    i++;

    auto bottomFrame=new QFrame();
    l->addWidget(bottomFrame);
    auto bl=Layout::horizontal(bottomFrame,false);

    auto start=new QPushButton("Show popup");
    bl->addWidget(start);
    QObject::connect(start,&QPushButton::clicked,testWidget,
    [testWidget](){
            auto widget=new QPushButton("Hello");
            widget->setMaximumHeight(100);
            widget->setMaximumWidth(200);
            testWidget->popup(widget);
        }
    );

    auto hide=new QPushButton("Hide popup");
    bl->addWidget(hide);
    QObject::connect(hide,&QPushButton::clicked,testWidget,&FrameWithModalPopup::closePopup);

    QString darkTheme="*{font-size: 20px;} \n *{color: white;background-color:black;} QPushButton{border: 1px solid white;}";
    QString lightTheme="*{font-size: 20px;} \n *{color: black;background-color:lightgray;} QPushButton{border: 1px solid black;}";
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
    w.resize(500,400);
    w.setWindowTitle("Modal popup Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
