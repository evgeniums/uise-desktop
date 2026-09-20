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

/** @file uise/desktop/src/voicerecorderdialog.cpp
*
*  Defines VoiceRecorderDialog and FloatingVoiceRecorderDialog.
*
*/

/****************************************************************************/

#include <algorithm>

#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPointer>
#include <QTimer>

#include <uise/desktop/voicerecorderdialog.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/svgiconlocator.hpp>
#include <uise/desktop/waveformbar.hpp>
#include <uise/desktop/utils/audiotime.hpp>
#include <uise/desktop/utils/layout.hpp>

#include <uise/desktop/ipp/dialog.ipp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

namespace {

//! The record dot is on for this long, then off for as long: a blink.
constexpr int BlinkHalfPeriodMs=500;

std::shared_ptr<SvgIcon> recorderIcon(const QString& alias, QWidget* context)
{
    // Cancel is destructive and drawn in the red palette, which is a colour context of its own:
    // the colours of an icon belong to its context, not to the alias.
    const auto* contextName=(alias==QLatin1String("cancel"))?"VoiceRecorderDanger":"VoiceRecorder";
    return Style::instance().svgIconLocator().icon(QString("%1::%2").arg(QLatin1String(contextName),alias),context);
}

//! Set a dynamic property and make the stylesheet see it.
void setStyleProperty(QWidget* widget, const char* name, bool value)
{
    widget->setProperty(name,value);
    Style::updateWidgetStyle(widget);
}

}

//--------------------------------------------------------------------------

class VoiceRecorderDialog_p
{
    public:

        using State=AbstractVoiceRecorderDialog::State;
        using Target=AbstractVoiceRecorderDialog::Target;

        State state=State::Held;
        Target hot=Target::None;
        qint64 elapsedMs=0;
        qint64 playbackMs=0;

        QFrame* header=nullptr;
        QLabel* dot=nullptr;
        QLabel* durationLabel=nullptr;

        QFrame* targets=nullptr;
        QLabel* continueArea=nullptr;
        QLabel* cancelArea=nullptr;

        QFrame* buttons=nullptr;
        IconTextButton* pauseButton=nullptr;
        IconTextButton* cancelButton=nullptr;
        IconTextButton* sendButton=nullptr;

        QFrame* playerRow=nullptr;
        IconTextButton* listenButton=nullptr;
        WaveformBar* bar=nullptr;

        QPlainTextEdit* commentEdit=nullptr;

        QTimer* blinkTimer=nullptr;
        bool dim=false;
};

//--------------------------------------------------------------------------

VoiceRecorderDialog::VoiceRecorderDialog(QWidget* parent)
    : Base(parent),
      pimpl(std::make_unique<VoiceRecorderDialog_p>())
{
}

//--------------------------------------------------------------------------

VoiceRecorderDialog::~VoiceRecorderDialog()
{
}

//--------------------------------------------------------------------------

void VoiceRecorderDialog::construct()
{
    setObjectName("voiceRecorderDialog");

    auto* content=new QFrame(this);
    content->setObjectName("recorderContent");
    auto layout=Layout::vertical(content);

    // ---- header: the record dot and the length -----------------------------------------------

    pimpl->header=new QFrame(content);
    pimpl->header->setObjectName("recorderHeader");
    auto headerLayout=Layout::horizontal(pimpl->header);

    pimpl->dot=new QLabel(pimpl->header);
    pimpl->dot->setObjectName("recordDot");
    headerLayout->addWidget(pimpl->dot,0,Qt::AlignVCenter);

    pimpl->durationLabel=new QLabel(pimpl->header);
    pimpl->durationLabel->setObjectName("durationLabel");
    headerLayout->addWidget(pimpl->durationLabel);
    headerLayout->addStretch(1);

    layout->addWidget(pimpl->header);

    // ---- Held: the two areas to drag onto ----------------------------------------------------

    pimpl->targets=new QFrame(content);
    pimpl->targets->setObjectName("targetsRow");
    auto targetsLayout=Layout::horizontal(pimpl->targets);

    pimpl->continueArea=new QLabel(pimpl->targets);
    pimpl->continueArea->setObjectName("continueArea");
    pimpl->continueArea->setAlignment(Qt::AlignCenter);
    pimpl->continueArea->setWordWrap(true);
    targetsLayout->addWidget(pimpl->continueArea,1);

    pimpl->cancelArea=new QLabel(pimpl->targets);
    pimpl->cancelArea->setObjectName("cancelArea");
    pimpl->cancelArea->setAlignment(Qt::AlignCenter);
    pimpl->cancelArea->setWordWrap(true);
    targetsLayout->addWidget(pimpl->cancelArea,1);

    layout->addWidget(pimpl->targets);

    // ---- Pinned and after: Pause, Cancel and Send -------------------------------------------

    pimpl->buttons=new QFrame(content);
    pimpl->buttons->setObjectName("buttonsRow");
    auto buttonsLayout=Layout::horizontal(pimpl->buttons);

    auto makeButton=[this](const QString& name, const QString& alias, IconTextButton*& button, QLayout* into)
    {
        button=new IconTextButton(QString(),recorderIcon(alias,this),pimpl->buttons,IconTextButton::IconPosition::AboveText);
        button->setObjectName(name);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        into->addWidget(button);
    };
    makeButton("pauseButton","pause",pimpl->pauseButton,buttonsLayout);
    makeButton("cancelButton","cancel",pimpl->cancelButton,buttonsLayout);
    makeButton("sendButton","send",pimpl->sendButton,buttonsLayout);

    layout->addWidget(pimpl->buttons);

    // ---- Paused and Listening: the player row and the comment ----------------------------------

    pimpl->playerRow=new QFrame(content);
    pimpl->playerRow->setObjectName("playerRow");
    auto playerLayout=Layout::horizontal(pimpl->playerRow);

    pimpl->listenButton=new IconTextButton(QString(),recorderIcon("listen",this),pimpl->playerRow,IconTextButton::IconPosition::BeforeText);
    pimpl->listenButton->setObjectName("listenButton");
    pimpl->listenButton->setCursor(Qt::PointingHandCursor);
    pimpl->listenButton->setFocusPolicy(Qt::NoFocus);
    playerLayout->addWidget(pimpl->listenButton);

    pimpl->bar=new WaveformBar(pimpl->playerRow);
    pimpl->bar->setObjectName("progressBar");
    pimpl->bar->setStyle(WaveformBar::Style::Bars);
    pimpl->bar->setSeekable(true);
    pimpl->bar->setCroppable(true);
    playerLayout->addWidget(pimpl->bar,1);

    layout->addWidget(pimpl->playerRow);

    pimpl->commentEdit=new QPlainTextEdit(content);
    pimpl->commentEdit->setObjectName("commentEdit");
    pimpl->commentEdit->setTabChangesFocus(true);
    layout->addWidget(pimpl->commentEdit);

    // ---- the blinking dot ------------------------------------------------------------------

    pimpl->blinkTimer=new QTimer(this);
    pimpl->blinkTimer->setInterval(BlinkHalfPeriodMs);
    connect(pimpl->blinkTimer,&QTimer::timeout,this,
        [this]()
        {
            pimpl->dim=!pimpl->dim;
            setStyleProperty(pimpl->dot,"dim",pimpl->dim);
        }
    );

    // ---- what the user can do --------------------------------------------------------------

    connect(pimpl->pauseButton,&IconTextButton::clicked,this,
        [this]()
        {
            if (pimpl->state==State::Pinned)
            {
                setState(State::Paused);
                emit pauseRequested();
            }
            else if (pimpl->state==State::Paused)
            {
                setState(State::Pinned);
                emit resumeRequested();
            }
            // while Listening the button is disabled
        }
    );
    connect(pimpl->cancelButton,&IconTextButton::clicked,this,[this](){emit cancelRequested();});
    connect(pimpl->sendButton,&IconTextButton::clicked,this,
        [this]()
        {
            emit sendRequested(comment(),cropStart(),cropEnd());
        }
    );
    connect(pimpl->listenButton,&IconTextButton::clicked,this,
        [this]()
        {
            if (pimpl->state==State::Paused)
            {
                setState(State::Listening);
                emit listenRequested();
            }
            else if (pimpl->state==State::Listening)
            {
                setState(State::Paused);
                emit listenPauseRequested();
            }
        }
    );
    connect(pimpl->bar,&WaveformBar::seekFinished,this,[this](qreal fraction){emit seekRequested(fraction);});
    connect(pimpl->bar,&WaveformBar::cropChanged,this,[this](qreal start, qreal end){emit cropChanged(start,end);});

    setWidget(content);

    // No bottom button row: Dialog<>'s constructor installs a Close button by default, and the
    // buttons that matter are in the content.
    setButtons({});

    retranslate();
    applyState();
}

//--------------------------------------------------------------------------

void VoiceRecorderDialog::setState(State state)
{
    if (pimpl->state==state)
    {
        return;
    }
    pimpl->state=state;
    applyState();
}

//--------------------------------------------------------------------------

VoiceRecorderDialog::State VoiceRecorderDialog::state() const
{
    return pimpl->state;
}

//--------------------------------------------------------------------------

void VoiceRecorderDialog::applyState()
{
    const auto state=pimpl->state;
    const auto held=(state==State::Held);
    const auto recording=(state==State::Held || state==State::Pinned);
    const auto reviewing=(state==State::Paused || state==State::Listening);

    pimpl->targets->setVisible(held);
    pimpl->buttons->setVisible(!held);
    pimpl->playerRow->setVisible(reviewing);
    pimpl->commentEdit->setVisible(reviewing);

    // Pause and Resume are one button; while the message plays it may not be resumed
    if (reviewing)
    {
        pimpl->pauseButton->setText(tr("Resume"));
        pimpl->pauseButton->setSvgIcon(recorderIcon("resume",this));
    }
    else
    {
        pimpl->pauseButton->setText(tr("Pause"));
        pimpl->pauseButton->setSvgIcon(recorderIcon("pause",this));
    }
    pimpl->pauseButton->setEnabled(state!=State::Listening);

    // Listen and Pause of the pre-listen, likewise
    if (state==State::Listening)
    {
        pimpl->listenButton->setText(tr("Pause"));
        pimpl->listenButton->setSvgIcon(recorderIcon("pause",this));
    }
    else
    {
        pimpl->listenButton->setText(tr("Listen"));
        pimpl->listenButton->setSvgIcon(recorderIcon("listen",this));
    }

    // the dot blinks for as long as audio is being taken in
    setStyleProperty(pimpl->dot,"recording",recording);
    if (recording)
    {
        if (!pimpl->blinkTimer->isActive())
        {
            pimpl->dim=false;
            setStyleProperty(pimpl->dot,"dim",false);
            pimpl->blinkTimer->start();
        }
    }
    else
    {
        pimpl->blinkTimer->stop();
        pimpl->dim=false;
        setStyleProperty(pimpl->dot,"dim",false);
    }

    // Escape and a click outside must not throw a live recording away; Cancel is the way out
    setClosable(!recording);

    updateDuration();

    // The content grew or shrank. The frame has to follow; it was anchored by its bottom corner, so
    // it grows upward. Deferred: the visibility changes above are still to be laid out.
    Layout::activateUpward(this);
    QPointer<QWidget> top=window();
    QTimer::singleShot(0,this,
        [top]()
        {
            if (!top.isNull() && top->isVisible())
            {
                top->adjustSize();
            }
        }
    );
}

//--------------------------------------------------------------------------

void VoiceRecorderDialog::updateDuration()
{
    if (pimpl->state==State::Listening)
    {
        pimpl->durationLabel->setText(tr("%1 / %2").arg(formatAudioTime(pimpl->playbackMs),formatAudioTime(pimpl->elapsedMs)));
    }
    else
    {
        pimpl->durationLabel->setText(formatAudioTime(pimpl->elapsedMs));
    }
}

//--------------------------------------------------------------------------

void VoiceRecorderDialog::setElapsedMs(qint64 ms)
{
    pimpl->elapsedMs=std::max<qint64>(0,ms);
    updateDuration();
    setPlaybackMs(pimpl->playbackMs);
}

//--------------------------------------------------------------------------

void VoiceRecorderDialog::setPlaybackMs(qint64 ms)
{
    pimpl->playbackMs=std::max<qint64>(0,ms);
    if (pimpl->elapsedMs>0)
    {
        pimpl->bar->setProgress(static_cast<qreal>(pimpl->playbackMs)/static_cast<qreal>(pimpl->elapsedMs));
    }
    else
    {
        pimpl->bar->setProgress(0.0);
    }
    if (pimpl->state==State::Listening)
    {
        updateDuration();
    }
}

//--------------------------------------------------------------------------

void VoiceRecorderDialog::setWaveform(const QByteArray& waveform)
{
    pimpl->bar->setWaveform(waveform);
}

//--------------------------------------------------------------------------

void VoiceRecorderDialog::setCropRange(qreal start, qreal end)
{
    pimpl->bar->setCropRange(start,end);
}

//--------------------------------------------------------------------------

qreal VoiceRecorderDialog::cropStart() const
{
    return pimpl->bar->cropStart();
}

//--------------------------------------------------------------------------

qreal VoiceRecorderDialog::cropEnd() const
{
    return pimpl->bar->cropEnd();
}

//--------------------------------------------------------------------------

void VoiceRecorderDialog::setComment(const QString& comment)
{
    pimpl->commentEdit->setPlainText(comment);
}

//--------------------------------------------------------------------------

QString VoiceRecorderDialog::comment() const
{
    return pimpl->commentEdit->toPlainText();
}

//--------------------------------------------------------------------------

void VoiceRecorderDialog::setHot(Target target)
{
    if (pimpl->hot==target)
    {
        return;
    }
    pimpl->hot=target;
    setStyleProperty(pimpl->continueArea,"hot",target==Target::Continue);
    setStyleProperty(pimpl->cancelArea,"hot",target==Target::Cancel);
}

//--------------------------------------------------------------------------

VoiceRecorderDialog::Target VoiceRecorderDialog::targetAt(const QPoint& globalPos) const
{
    auto over=[&globalPos](const QWidget* area)
    {
        return area->isVisible() && area->rect().contains(area->mapFromGlobal(globalPos));
    };

    if (over(pimpl->continueArea))
    {
        return Target::Continue;
    }
    if (over(pimpl->cancelArea))
    {
        return Target::Cancel;
    }
    return Target::None;
}

//--------------------------------------------------------------------------

void VoiceRecorderDialog::pointerMoved(const QPoint& globalPos)
{
    if (pimpl->state!=State::Held)
    {
        return;
    }
    setHot(targetAt(globalPos));
}

//--------------------------------------------------------------------------

void VoiceRecorderDialog::pointerReleased(const QPoint& globalPos)
{
    if (pimpl->state!=State::Held)
    {
        return;
    }

    const auto target=targetAt(globalPos);
    setHot(Target::None);

    switch (target)
    {
        case (Target::Continue):
        {
            setState(State::Pinned);
            emit pinned();
            break;
        }

        case (Target::Cancel):
        {
            emit cancelRequested();
            break;
        }

        case (Target::None):
        {
            // let go anywhere else -- on the microphone button itself, normally: that sends it
            emit sendRequested(comment(),cropStart(),cropEnd());
            break;
        }
    }
}

//--------------------------------------------------------------------------

void VoiceRecorderDialog::retranslate()
{
    setTitle(tr("Voice message"));
    pimpl->continueArea->setText(tr("Drag here to keep recording"));
    pimpl->cancelArea->setText(tr("Drag here to cancel"));
    pimpl->cancelButton->setText(tr("Cancel"));
    pimpl->sendButton->setText(tr("Send"));
    pimpl->commentEdit->setPlaceholderText(tr("Add a comment"));
}

//--------------------------------------------------------------------------

void VoiceRecorderDialog::changeEvent(QEvent* event)
{
    Base::changeEvent(event);
    if (event->type()==QEvent::LanguageChange && pimpl->commentEdit!=nullptr)
    {
        retranslate();
        // the texts that depend on the state
        applyState();
    }
}

//--------------------------------------------------------------------------

FloatingVoiceRecorderDialog::FloatingVoiceRecorderDialog(QWidget* parent)
    : FloatingVoiceRecorderDialogType(parent)
{
    // Re-typed from the base's Qt::Dialog, see the class doc comment. FramelessWindowHint stays:
    // the frame paints its own chrome and its content supplies the title bar it is dragged by.
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
}

//--------------------------------------------------------------------------

template class UISE_DESKTOP_EXPORT Dialog<AbstractVoiceRecorderDialog>;

//--------------------------------------------------------------------------

}
