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

/** @file demo/circlebusy/main.cpp
*
*  Demo application of CircleBusy.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QTextEdit>
#include <QScrollArea>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QComboBox>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/toast.hpp>
#include <uise/desktop/circlebusy.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    QMainWindow w;
    auto mainFrame=new QScrollArea();
    mainFrame->setWidgetResizable(true);
    auto contentFrame=new QFrame(mainFrame);
    auto l=Layout::vertical(contentFrame,false);
    mainFrame->setWidget(contentFrame);

    l->addStretch(1);

    auto textEdit=new QTextEdit(mainFrame);
    textEdit->setText("Hello world!");
    l->addWidget(textEdit);

    auto circleBusy=new CircleBusy(textEdit);

    l->addStretch(1);

    auto controlsFrame=new QFrame();
    l->addWidget(controlsFrame,0,Qt::AlignCenter);

    auto cl=new QHBoxLayout(controlsFrame);

    auto startStop=new QPushButton(mainFrame);
    startStop->setCheckable(true);
    startStop->setChecked(true);
    startStop->setText("Stop");
    cl->addWidget(startStop);
    QObject::connect(
        startStop,
        &QPushButton::toggled,
        mainFrame,
        [circleBusy,startStop](bool checked)
        {
            if (checked)
            {
                startStop->setText("Stop");
                circleBusy->start();
            }
            else
            {
                startStop->setText("Start");
                circleBusy->stop();
            }
        }
    );

    auto curve=new QComboBox();
    curve->addItems(
        QList<QString>{
        "Linear",
        "InQuad", "OutQuad", "InOutQuad", "OutInQuad",
        "InCubic", "OutCubic", "InOutCubic", "OutInCubic",
        "InQuart", "OutQuart", "InOutQuart", "OutInQuart",
        "InQuint", "OutQuint", "InOutQuint", "OutInQuint",
        "InSine", "OutSine", "InOutSine", "OutInSine",
        "InExpo", "OutExpo", "InOutExpo", "OutInExpo",
        "InCirc", "OutCirc", "InOutCirc", "OutInCirc",
        "InElastic", "OutElastic", "InOutElastic", "OutInElastic",
        "InBack", "OutBack", "InOutBack", "OutInBack",
        "InBounce", "OutBounce", "InOutBounce", "OutInBounce",
        "InCurve", "OutCurve", "SineCurve", "CosineCurve",
        "BezierSpline", "TCBSpline", "Custom", "NCurveTypes"
        }
    );
    curve->setCurrentIndex(static_cast<int>(CircleBusy::DefaultEasingCurve));
    cl->addWidget(curve);
    QObject::connect(
        curve,
        &QComboBox::currentIndexChanged,
        circleBusy,
        [circleBusy](int value)
        {
            circleBusy->setEasingCurve(static_cast<QEasingCurve::Type>(value));
        }
    );

    auto progress=new QSlider();
    progress->setOrientation(Qt::Horizontal);
    progress->setMinimum(0);
    progress->setMaximum(100);
    progress->setValue(CircleBusy::DefaultCirclePercent);
    cl->addWidget(progress);
    QObject::connect(
        progress,
        &QSlider::valueChanged,
        circleBusy,
        [circleBusy](int value)
        {
            circleBusy->setCirclePercent(value);
        }
    );

    cl->addStretch(1);

    w.setCentralWidget(mainFrame);
    w.resize(300,400);
    w.setWindowTitle("Circle Busy Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
