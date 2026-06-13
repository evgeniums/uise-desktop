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

/** @file demo/audiodevicesetting/main.cpp
*
*  Demo application of AudioDeviceSetting.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QFrame>
#include <QLabel>
#include <QTimer>
#include <QDebug>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/audiodevicesetting.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    QMainWindow w;
    auto mainFrame=new QFrame();
    auto l=Layout::vertical(mainFrame);

    // capture device
    l->addWidget(new QLabel(QObject::tr("Microphone:"),mainFrame));
    auto capture=new AudioDeviceSetting(AudioDeviceType::Capture,mainFrame);
    capture->setDevices({{"mic1","USB Microphone"},{"mic2","Built-in Microphone"}});
    capture->setDefaultDeviceName("System default (Built-in Microphone)");
    capture->setCurrentDevice("mic2");
    l->addWidget(capture);

    // playback device
    l->addWidget(new QLabel(QObject::tr("Speaker:"),mainFrame));
    auto playback=new AudioDeviceSetting(AudioDeviceType::Playback,mainFrame);
    playback->setDevices({{"spk1","Headphones"},{"spk2","Built-in Speakers"}});
    playback->setDefaultDeviceName("System default (Built-in Speakers)");
    l->addWidget(playback);

    auto logDevice=[](const char* tag)
    {
        return [tag](const AudioDevice& device)
        {
            qDebug() << tag << "deviceChanged id=" << device.id << "name=" << device.name;
        };
    };
    QObject::connect(capture,&AudioDeviceSetting::deviceChanged,&w,logDevice("[capture]"));
    QObject::connect(playback,&AudioDeviceSetting::deviceChanged,&w,logDevice("[playback]"));

    QObject::connect(capture,&AudioDeviceSetting::testToggled,&w,[](bool testing){
        qDebug() << "[capture] testToggled" << testing;
    });
    QObject::connect(playback,&AudioDeviceSetting::testToggled,&w,[](bool testing){
        qDebug() << "[playback] testToggled" << testing;
    });

    // animate volume meters while testing
    auto timer=new QTimer(&w);
    int phase=0;
    QObject::connect(timer,&QTimer::timeout,&w,[capture,playback,&phase](){
        phase=(phase+7)%100;
        if (capture->isTesting())
        {
            capture->setVolume(phase);
        }
        if (playback->isTesting())
        {
            playback->setVolume((phase+50)%100);
        }
    });
    timer->start(120);

    w.setCentralWidget(mainFrame);
    w.resize(480,260);
    w.setWindowTitle("AudioDeviceSetting Demo");
    w.show();
    return app.exec();
}

//--------------------------------------------------------------------------
