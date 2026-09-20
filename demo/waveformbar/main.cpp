/**
@copyright Evgeny Sidorov 2022-2025

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file demo/waveformbar/main.cpp
*
*  Demo application of WaveformBar.
*
*/

/****************************************************************************/

#include <memory>

#include <QApplication>
#include <QCheckBox>
#include <QDebug>
#include <QFrame>
#include <QLabel>
#include <QList>
#include <QMainWindow>
#include <QTimer>
#include <QtMath>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/waveformbar.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

namespace {

//! 100 bytes shaped a little like speech: phrases with pauses between them.
QByteArray makeWaveform(int seed)
{
    QByteArray waveform(100,0);
    for (int i=0;i<waveform.size();i++)
    {
        const auto phrase=0.5+0.5*qSin(i*0.21+seed);
        const auto detail=0.5+0.5*qSin(i*1.7+seed*3.0);
        const auto value=(0.75*phrase+0.25*detail)*255.0;
        waveform[i]=static_cast<char>(static_cast<quint8>(qBound(0.0,value,255.0)));
    }
    return waveform;
}

}

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    Style::instance().applyStyleSheet();

    QMainWindow w;
    auto mainFrame=new QFrame();
    auto l=Layout::vertical(mainFrame);
    l->setContentsMargins(16,16,16,16);
    l->setSpacing(10);

    // ---- a voice message: bars ---------------------------------------------------------------

    l->addWidget(new QLabel("Style::Bars -- a stored waveform, as in a voice message bubble",mainFrame));
    auto* bars=new WaveformBar(mainFrame);
    bars->setWaveform(makeWaveform(2));
    bars->setFixedWidth(320);
    l->addWidget(bars);

    // ---- an audio file: line -----------------------------------------------------------------

    l->addWidget(new QLabel("Style::Line -- no waveform, an ordinary audio file",mainFrame));
    auto* line=new WaveformBar(mainFrame);
    line->setStyle(WaveformBar::Style::Line);
    line->setFixedWidth(320);
    l->addWidget(line);

    // ---- the same bars, narrow: neighbouring values are merged by their maximum -------------------

    l->addWidget(new QLabel("The same 100 values in 120 px: merged by maximum, the peaks survive",mainFrame));
    auto* narrow=new WaveformBar(mainFrame);
    narrow->setWaveform(makeWaveform(2));
    narrow->setFixedWidth(120);
    l->addWidget(narrow);

    // ---- no waveform yet: placeholder bars ---------------------------------------------------

    l->addWidget(new QLabel("No waveform yet -- flat placeholder bars, so a bubble has its shape before the data arrives",mainFrame));
    auto* empty=new WaveformBar(mainFrame);
    empty->setFixedWidth(320);
    l->addWidget(empty);

    // ---- controls -------------------------------------------------------------------------------

    auto* seekable=new QCheckBox("Seekable",mainFrame);
    seekable->setChecked(true);
    l->addWidget(seekable);
    auto* croppable=new QCheckBox("Croppable (two handles, as in the recorder's paused state)",mainFrame);
    l->addWidget(croppable);
    auto* playing=new QCheckBox("Playing (a ticker moves the progress, as a player would)",mainFrame);
    l->addWidget(playing);

    auto* log=new QLabel("(interact with a bar)",mainFrame);
    l->addWidget(log);
    l->addStretch(1);

    const QList<WaveformBar*> all{bars,line,narrow,empty};

    QObject::connect(seekable,&QCheckBox::toggled,&w,[all](bool on){for (auto* bar : all){bar->setSeekable(on);}});
    QObject::connect(croppable,&QCheckBox::toggled,&w,
        [all](bool on)
        {
            for (auto* bar : all)
            {
                bar->setCroppable(on);
                if (on)
                {
                    bar->setCropRange(0.2,0.8);
                }
            }
        }
    );

    for (auto* bar : all)
    {
        QObject::connect(bar,&WaveformBar::seekRequested,&w,[log](qreal f){log->setText(QString("seekRequested %1").arg(f,0,'f',3));});
        QObject::connect(bar,&WaveformBar::seekFinished,&w,[log](qreal f){log->setText(QString("seekFinished %1").arg(f,0,'f',3));});
        QObject::connect(bar,&WaveformBar::cropChanged,&w,
            [log](qreal s, qreal e)
            {
                log->setText(QString("cropChanged %1 .. %2").arg(s,0,'f',3).arg(e,0,'f',3));
            }
        );
    }

    // The progress is pushed in, like a player's position. While a bar is being dragged it
    // ignores the ticks, which is what lets the finger win.
    auto* ticker=new QTimer(&w);
    ticker->setInterval(50);
    auto position=std::make_shared<qreal>(0.0);
    QObject::connect(ticker,&QTimer::timeout,&w,
        [all,position]()
        {
            *position+=0.004;
            if (*position>1.0)
            {
                *position=0.0;
            }
            for (auto* bar : all)
            {
                bar->setProgress(*position);
            }
        }
    );
    QObject::connect(playing,&QCheckBox::toggled,&w,
        [ticker](bool on)
        {
            if (on)
            {
                ticker->start();
            }
            else
            {
                ticker->stop();
            }
        }
    );

    w.setCentralWidget(mainFrame);
    w.resize(560,640);
    w.setWindowTitle("Waveform Bar Demo");
    w.show();
    return app.exec();
}
