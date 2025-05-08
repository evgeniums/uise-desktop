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

/** @file demo/busywaiting/main.cpp
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
#include <uise/desktop/busywaiting.hpp>

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
    auto testWidget=new BusyWaiting(testFrame,true,true);

    auto configFrame=new QFrame();
    tl->addWidget(configFrame,0,Qt::AlignTop);
    auto confl=Layout::grid(configFrame,false);

    int i=0;

    auto innerRadius=new QSpinBox();
    innerRadius->setRange(0,100);
    innerRadius->setValue(30);
    confl->addWidget(new QLabel("Inner radius"),i,0);
    confl->addWidget(innerRadius,i,1);
    QObject::connect(innerRadius,&QSpinBox::valueChanged,testWidget,&BusyWaiting::setInnerRadius);
    i++;

    auto lines=new QSpinBox();
    lines->setRange(0,100);
    lines->setValue(20);
    confl->addWidget(new QLabel("Lines"),i,0);
    confl->addWidget(lines,i,1);
    QObject::connect(lines,&QSpinBox::valueChanged,testWidget,&BusyWaiting::setNumberOfLines);
    i++;

    auto lineL=new QSpinBox();
    lineL->setRange(1,50);
    lineL->setValue(30);
    confl->addWidget(new QLabel("Line length"),i,0);
    confl->addWidget(lineL,i,1);
    QObject::connect(lineL,&QSpinBox::valueChanged,testWidget,&BusyWaiting::setLineLength);
    i++;

    auto lineW=new QSpinBox();
    lineW->setRange(1,10);
    lineW->setValue(4);
    confl->addWidget(new QLabel("Line width"),i,0);
    confl->addWidget(lineW,i,1);
    QObject::connect(lineW,&QSpinBox::valueChanged,testWidget,&BusyWaiting::setLineWidth);
    i++;

    auto speed=new QDoubleSpinBox();
    speed->setRange(0,10);
    speed->setValue(M_PI/2);
    speed->setSingleStep(0.01);
    confl->addWidget(new QLabel("Speed"),i,0);
    confl->addWidget(speed,i,1);
    QObject::connect(speed,&QDoubleSpinBox::valueChanged,testWidget,&BusyWaiting::setRevolutionsPerSecond);
    i++;

    auto opacity=new QDoubleSpinBox();
    opacity->setRange(0,100);
    opacity->setValue(M_PI);
    confl->addWidget(new QLabel("Opacity"),i,0);
    confl->addWidget(opacity,i,1);
    QObject::connect(opacity,&QDoubleSpinBox::valueChanged,testWidget,&BusyWaiting::setMinimumTrailOpacity);
    i++;

    auto fade=new QDoubleSpinBox();
    fade->setRange(0,100);
    fade->setValue(80);
    fade->setSingleStep(1);
    confl->addWidget(new QLabel("Fade"),i,0);
    confl->addWidget(fade,i,1);
    QObject::connect(fade,&QDoubleSpinBox::valueChanged,testWidget,&BusyWaiting::setTrailFadePercentage);
    i++;

    auto roudness=new QDoubleSpinBox();
    roudness->setRange(0,100);
    roudness->setValue(100);
    confl->addWidget(new QLabel("Roudness"),i,0);
    confl->addWidget(roudness,i,1);
    QObject::connect(roudness,&QDoubleSpinBox::valueChanged,testWidget,&BusyWaiting::setRoundness);
    i++;

    auto bottomFrame=new QFrame();
    l->addWidget(bottomFrame);
    auto bl=Layout::horizontal(bottomFrame,false);

    auto start=new QPushButton("Start");
    bl->addWidget(start);
    QObject::connect(start,&QPushButton::clicked,testWidget,&BusyWaiting::start);

    auto stop=new QPushButton("Stop");
    bl->addWidget(stop);
    QObject::connect(stop,&QPushButton::clicked,testWidget,&BusyWaiting::stop);

    QString darkTheme="*{font-size: 20px;} \n *{color: white;background-color:black;}";
    QString lightTheme="*{font-size: 20px;} \n *{color: black;background-color:lightgray;}";
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
    testWidget->setInnerRadius(innerRadius->value());
    testWidget->setLineLength(lineL->value());
    testWidget->setLineWidth(lineW->value());
    testWidget->start();

    w.setCentralWidget(mainFrame);
    w.resize(500,400);
    w.setWindowTitle("BusyWaiting Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
