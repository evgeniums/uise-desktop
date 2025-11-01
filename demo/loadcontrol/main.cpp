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

/** @file demo/loadcontrol/main.cpp
*
*  Demo application of LoadControl.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QLineEdit>
#include <QLabel>
#include <QScrollArea>
#include <QSlider>
#include <QComboBox>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/toast.hpp>
#include <uise/desktop/loadcontrol.hpp>

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

    auto toast=new Toast(mainFrame);

    auto loadControl=new LoadControl();
    l->addWidget(loadControl,0,Qt::AlignCenter);
    QObject::connect(
        loadControl,
        &LoadControl::clicked,
        toast,
        [toast]()
        {
            toast->show("Clicked");
        }
    );

    l->addStretch(1);

    auto controlsFrame=new QFrame();
    l->addWidget(controlsFrame,0,Qt::AlignCenter);

    auto cl=Layout::horizontal(controlsFrame);

    auto state=new QComboBox();
    state->addItems({"None","Can download","Can upload","Paused","Waiting","Running"});
    state->setCurrentIndex(1);
    cl->addWidget(state);
    QObject::connect(
        state,
        &QComboBox::currentIndexChanged,
        loadControl,
        [loadControl](int index)
        {
            auto state=static_cast<LoadControl::State>(index);
            loadControl->setState(state);
        }
    );

    auto progress=new QSlider();
    progress->setOrientation(Qt::Horizontal);
    progress->setMinimum(0);
    progress->setMaximum(100);
    cl->addWidget(progress);
    QObject::connect(
        progress,
        &QSlider::valueChanged,
        loadControl,
        [loadControl](int value)
        {
            loadControl->setProgress(value);
        }
    );

    cl->addStretch(1);

    w.setCentralWidget(mainFrame);
    w.resize(300,400);
    w.setWindowTitle("Avatar Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
