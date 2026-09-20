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

/** @file demo/voicerecorder/main.cpp
*
*  Demo application of the composer's microphone button and the voice recorder popup.
*
*/

/****************************************************************************/

#include <memory>

#include <QApplication>
#include <QCheckBox>
#include <QDebug>
#include <QFrame>
#include <QLabel>
#include <QMainWindow>
#include <QPointer>
#include <QTimer>
#include <QtMath>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/messageeditor.hpp>
#include <uise/desktop/abstractvoicerecorderdialog.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

namespace {

constexpr int TickMs=100;

//! 100 bytes shaped a little like speech, for what has been "recorded".
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

//! What the host of the recorder popup would do with a real recorder and player. Here two timers
//! stand in for them: nothing is recorded and nothing is played, which is the point -- the popup
//! and the editor are backend-free, and everything below is what C4/S4 will replace with hatn media.
struct FakeRecording
{
    qint64 elapsedMs=0;
    qint64 playbackMs=0;
    QTimer* recordTimer=nullptr;
    QTimer* playTimer=nullptr;
};

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

    l->addWidget(new QLabel(
        "Hold the microphone button, right of the text, to record.\n"
        "  - Let go over the button: sent.\n"
        "  - Drag onto \"keep recording\" and let go: the popup pins itself.\n"
        "  - Drag onto \"cancel\" and let go: cancelled.\n"
        "Pinned: Pause shows the review area: Listen, a seekable waveform with two trim handles, a comment.\n"
        "Nothing is really recorded; a timer stands in for the recorder.",
        mainFrame
    ));
    l->addStretch(1);

    // ---- the composer ---------------------------------------------------------------------------------

    auto* editor=new MessageEditor(mainFrame);
    editor->setMicButtonVisible(true);
    editor->setEmojiButtonVisible(true);
    l->addWidget(editor);

    auto* enabled=new QCheckBox("Voice messages enabled (AbstractMessageEditor::setVoiceMessageEnabled())",mainFrame);
    enabled->setChecked(true);
    l->addWidget(enabled);
    QObject::connect(enabled,&QCheckBox::toggled,editor,&AbstractMessageEditor::setVoiceMessageEnabled);

    auto* log=new QLabel("(nothing yet)",mainFrame);
    log->setWordWrap(true);
    l->addWidget(log);

    // ---- the host -------------------------------------------------------------------------------------

    auto rec=std::make_shared<FakeRecording>();
    rec->recordTimer=new QTimer(&w);
    rec->recordTimer->setInterval(TickMs);
    rec->playTimer=new QTimer(&w);
    rec->playTimer->setInterval(TickMs);

    QPointer<AbstractVoiceRecorderDialog> current;

    QObject::connect(rec->recordTimer,&QTimer::timeout,&w,
        [rec,&current]()
        {
            rec->elapsedMs+=TickMs;
            if (!current.isNull())
            {
                current->setElapsedMs(rec->elapsedMs);
            }
        }
    );
    QObject::connect(rec->playTimer,&QTimer::timeout,&w,
        [rec,&current]()
        {
            rec->playbackMs+=TickMs;
            if (rec->playbackMs>=rec->elapsedMs)
            {
                // the end of the recording: stop listening
                rec->playbackMs=0;
                rec->playTimer->stop();
                if (!current.isNull())
                {
                    current->setState(AbstractVoiceRecorderDialog::State::Paused);
                }
            }
            if (!current.isNull())
            {
                current->setPlaybackMs(rec->playbackMs);
            }
        }
    );

    QObject::connect(editor,&AbstractMessageEditor::voiceRecorderOpened,&w,
        [rec,log,&current](AbstractVoiceRecorderDialog* dialog)
        {
            current=dialog;
            rec->elapsedMs=0;
            rec->playbackMs=0;
            rec->recordTimer->start();
            log->setText("recording started");

            // Connected here, after the editor's own connections: the editor closes the popup one
            // turn of the event loop after sendRequested() and cancelRequested(), so these run first.
            // Disconnected in voiceRecorderClosed() below: the popup outlives the recording.
            QObject::connect(dialog,&AbstractVoiceRecorderDialog::pinned,log,[log](){log->setText("pinned: recording goes on");});
            QObject::connect(dialog,&AbstractVoiceRecorderDialog::pauseRequested,log,
                [rec,log,dialog]()
                {
                    rec->recordTimer->stop();
                    // what a real recorder hands back when it is paused: the waveform so far
                    dialog->setWaveform(makeWaveform(5));
                    dialog->setCropRange(0.0,1.0);
                    log->setText("paused");
                }
            );
            QObject::connect(dialog,&AbstractVoiceRecorderDialog::resumeRequested,log,
                [rec,log]()
                {
                    rec->recordTimer->start();
                    log->setText("resumed");
                }
            );
            QObject::connect(dialog,&AbstractVoiceRecorderDialog::listenRequested,log,
                [rec,log]()
                {
                    rec->playTimer->start();
                    log->setText("listening");
                }
            );
            QObject::connect(dialog,&AbstractVoiceRecorderDialog::listenPauseRequested,log,
                [rec,log]()
                {
                    rec->playTimer->stop();
                    log->setText("listening paused");
                }
            );
            QObject::connect(dialog,&AbstractVoiceRecorderDialog::seekRequested,log,
                [rec,log](qreal fraction)
                {
                    rec->playbackMs=static_cast<qint64>(fraction*static_cast<qreal>(rec->elapsedMs));
                    log->setText(QString("seek to %1 ms").arg(rec->playbackMs));
                }
            );
            QObject::connect(dialog,&AbstractVoiceRecorderDialog::cropChanged,log,
                [log](qreal start, qreal end)
                {
                    log->setText(QString("crop %1 .. %2").arg(start,0,'f',3).arg(end,0,'f',3));
                }
            );
            QObject::connect(dialog,&AbstractVoiceRecorderDialog::sendRequested,log,
                [rec,log](const QString& comment, qreal start, qreal end)
                {
                    log->setText(QString("SEND %1 ms, crop %2 .. %3, comment: \"%4\"")
                                    .arg(rec->elapsedMs).arg(start,0,'f',3).arg(end,0,'f',3).arg(comment));
                }
            );
            QObject::connect(dialog,&AbstractVoiceRecorderDialog::cancelRequested,log,[log](){log->setText("CANCEL: the recording is discarded");});
        }
    );

    // always follows, whatever closed it
    QObject::connect(editor,&AbstractMessageEditor::voiceRecorderClosed,&w,
        [rec,log,&current]()
        {
            rec->recordTimer->stop();
            rec->playTimer->stop();
            // The popup is kept between recordings and comes back through voiceRecorderOpened()
            // every time, so what was connected to it has to go, or the next press connects it all
            // a second time. Everything above has `log` as its context object.
            if (!current.isNull())
            {
                QObject::disconnect(current.data(),nullptr,log,nullptr);
            }
            current=nullptr;
            qDebug()<<"[host] recorder closed";
        }
    );

    w.setCentralWidget(mainFrame);
    w.resize(620,420);
    w.setWindowTitle("Voice Recorder Demo");
    w.show();
    return app.exec();
}
