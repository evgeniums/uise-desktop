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

/** @file demo/audioplayer/main.cpp
*
*  Demo application of AudioPlayer, its floating dialog and the voice/audio chat rows.
*
*/

/****************************************************************************/

#include <memory>

#include <QApplication>
#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QLabel>
#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QtMath>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/audioplayer.hpp>
#include <uise/desktop/audioplayerdialog.hpp>
#include <uise/desktop/qtaudioplaybackengine.hpp>
#include <uise/desktop/chataudiofileitems.hpp>

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

//! What a host does for a voice message: it owns the position and the playing flag, and a timer
//! stands in for the audio engine. Nothing here plays a sound.
struct FakeVoicePlayback
{
    qint64 durationMs=14000;
    qint64 positionMs=0;
    qreal speed=1.0;
    QTimer* timer=nullptr;
};

constexpr int TickMs=50;

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

    // ---- 1. an audio file, played by Qt Multimedia ---------------------------------------------

    l->addWidget(new QLabel("Audio file -- QtAudioPlaybackEngine, progress as a line",mainFrame));

    auto* engine=new QtAudioPlaybackEngine(&w);

    auto* filePlayer=new AudioPlayer(mainFrame);
    filePlayer->initWidget(mainFrame);
    filePlayer->setTitle("No file chosen");
    filePlayer->setTitleClickable(false);
    filePlayer->setProgressStyle(WaveformBar::Style::Line);
    // the engine drives the display and receives every request
    filePlayer->setEngine(engine);
    l->addWidget(filePlayer->qWidget());

    auto* openButton=new QPushButton("Open audio file...",mainFrame);
    l->addWidget(openButton);
    QObject::connect(openButton,&QPushButton::clicked,&w,
        [&w,engine,filePlayer]()
        {
            const auto path=QFileDialog::getOpenFileName(
                &w,"Open audio file",QString(),
                "Audio (*.mp3 *.m4a *.wav *.flac *.ogg *.opus *.aac);;All files (*)"
            );
            if (path.isEmpty())
            {
                return;
            }
            engine->stop();
            engine->open(path);
            filePlayer->setTitle(QFileInfo(path).fileName());
        }
    );
    QObject::connect(filePlayer,&AbstractAudioPlayer::errorOccurred,&w,[](const QString& message){qDebug()<<"[file player] error"<<message;});

    // ---- 2. a voice message, no engine -------------------------------------------------------

    l->addWidget(new QLabel("Voice message -- no engine, the host pushes the position; progress as a waveform",mainFrame));

    auto* voicePlayer=new AudioPlayer(mainFrame);
    voicePlayer->initWidget(mainFrame);
    voicePlayer->setTitle("Alice, today 10:32");
    voicePlayer->setProgressStyle(WaveformBar::Style::Bars);
    voicePlayer->setWaveform(makeWaveform(3));
    l->addWidget(voicePlayer->qWidget());

    auto fake=std::make_shared<FakeVoicePlayback>();
    fake->timer=new QTimer(&w);
    fake->timer->setInterval(TickMs);
    voicePlayer->setDuration(fake->durationMs);

    QObject::connect(fake->timer,&QTimer::timeout,&w,
        [voicePlayer,fake]()
        {
            fake->positionMs+=static_cast<qint64>(TickMs*fake->speed);
            if (fake->positionMs>=fake->durationMs)
            {
                fake->positionMs=0;
                fake->timer->stop();
                voicePlayer->setPlaying(false);
            }
            voicePlayer->setPosition(fake->positionMs);
        }
    );
    QObject::connect(voicePlayer,&AbstractAudioPlayer::playRequested,&w,[voicePlayer,fake](){voicePlayer->setPlaying(true);fake->timer->start();});
    QObject::connect(voicePlayer,&AbstractAudioPlayer::pauseRequested,&w,[voicePlayer,fake](){voicePlayer->setPlaying(false);fake->timer->stop();});
    QObject::connect(voicePlayer,&AbstractAudioPlayer::stopRequested,&w,
        [voicePlayer,fake]()
        {
            voicePlayer->setPlaying(false);
            fake->timer->stop();
            fake->positionMs=0;
            voicePlayer->setPosition(0);
        }
    );
    QObject::connect(voicePlayer,&AbstractAudioPlayer::seekRequested,&w,
        [voicePlayer,fake](qint64 ms)
        {
            fake->positionMs=ms;
            voicePlayer->setPosition(ms);
        }
    );
    QObject::connect(voicePlayer,&AbstractAudioPlayer::speedChanged,&w,[fake](qreal speed){fake->speed=speed;});
    QObject::connect(voicePlayer,&AbstractAudioPlayer::titleClicked,&w,[](){qDebug()<<"[voice player] titleClicked: jump to the message";});

    // ---- 3. the same player in a floating dialog ------------------------------------------------

    auto* dialogButton=new QPushButton("Open floating player (shares the Qt engine above)",mainFrame);
    l->addWidget(dialogButton);
    auto floating=QPointer<FloatingAudioPlayerDialog>();
    QObject::connect(dialogButton,&QPushButton::clicked,&w,
        [&w,engine,&floating,filePlayer]()
        {
            if (floating.isNull())
            {
                floating=new FloatingAudioPlayerDialog(&w);
                floating->openDialog(false,false);
                if (!floating->dialog().isNull() && floating->dialog()->player()!=nullptr)
                {
                    auto* player=floating->dialog()->player();
                    player->setProgressStyle(WaveformBar::Style::Line);
                    player->setEngine(engine);
                }
            }
            if (!floating.isNull() && !floating->dialog().isNull() && floating->dialog()->player()!=nullptr)
            {
                floating->dialog()->player()->setTitle(filePlayer->title());
                floating->popup();
            }
        }
    );

    // ---- 4. the rows a chat message shows -----------------------------------------------------------

    l->addWidget(new QLabel("Chat rows -- a voice message and an audio file (the row's own menu has Play/Stop)",mainFrame));

    ChatFileItem voiceItem;
    voiceItem.setFileName("voice_20260920_1032.ogg");
    voiceItem.setVoice(true);
    voiceItem.setVoiceWaveform(makeWaveform(7));
    voiceItem.setVoiceDurationMs(9000);
    voiceItem.setSize(31*1024);
    voiceItem.setState(ChatFileTransferState::Ready);
    voiceItem.setListenedByPeer(true);

    ChatFileItem audioItem;
    audioItem.setFileName("some song.mp3");
    audioItem.setSize(3*1024*1024);
    audioItem.setState(ChatFileTransferState::Ready);

    auto* voiceRow=makeChatAudioFileItem(voiceItem,mainFrame);
    voiceRow->setItem(voiceItem,false);
    voiceRow->limitWidth(380);
    l->addWidget(voiceRow);

    auto* audioRow=makeChatAudioFileItem(audioItem,mainFrame);
    audioRow->setItem(audioItem,true);
    audioRow->limitWidth(380);
    l->addWidget(audioRow);

    // Playing the voice row: the item carries whether it plays, the position goes straight to the row.
    auto voicePosition=std::make_shared<qreal>(0.0);
    auto* voiceTimer=new QTimer(&w);
    voiceTimer->setInterval(TickMs);
    QObject::connect(voiceTimer,&QTimer::timeout,&w,
        [voiceRow,voicePosition,voiceTimer,voiceItem]()
        {
            *voicePosition+=static_cast<qreal>(TickMs)/static_cast<qreal>(voiceItem.voiceDurationMs());
            if (*voicePosition>=1.0)
            {
                *voicePosition=0.0;
                voiceTimer->stop();
                auto item=voiceRow->item();
                item.setPlaying(false);
                voiceRow->setItem(item,voiceRow->isIncoming());
            }
            voiceRow->setPlaybackProgress(*voicePosition);
        }
    );
    QObject::connect(voiceRow,&ChatMessageFileItem::playRequested,&w,
        [voiceRow,voiceTimer]()
        {
            auto item=voiceRow->item();
            item.setPlaying(true);
            voiceRow->setItem(item,voiceRow->isIncoming());
            voiceTimer->start();
        }
    );
    QObject::connect(voiceRow,&ChatMessageFileItem::stopRequested,&w,
        [voiceRow,voiceTimer]()
        {
            // for a voice message "stop" is a pause: the position stays where it is
            auto item=voiceRow->item();
            item.setPlaying(false);
            voiceRow->setItem(item,voiceRow->isIncoming());
            voiceTimer->stop();
        }
    );
    QObject::connect(voiceRow,&ChatMessageFileItem::seekRequested,&w,
        [voiceRow,voicePosition](qreal fraction)
        {
            *voicePosition=fraction;
            voiceRow->setPlaybackProgress(fraction);
        }
    );

    for (auto* row : {voiceRow,audioRow})
    {
        QObject::connect(row,&ChatMessageFileItem::clicked,&w,[](){qDebug()<<"[row] clicked (a host maps this to play for audio)";});
        QObject::connect(row,&ChatMessageFileItem::menuTriggered,&w,[](int action){qDebug()<<"[row] menu action"<<action;});
    }

    l->addStretch(1);

    w.setCentralWidget(mainFrame);
    w.resize(560,700);
    w.setWindowTitle("Audio Player Demo");
    w.show();
    return app.exec();
}
