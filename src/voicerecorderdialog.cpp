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
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPointer>
#include <QShortcut>
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
    // Cancel is destructive and drawn in the red palette, Send is the action and drawn in the accent of the
    // composer's active Send button; each is a colour context of its own: the colours of an icon belong to
    // its context, not to the alias.
    const char* contextName="VoiceRecorder";
    if (alias==QLatin1String("cancel"))
    {
        contextName="VoiceRecorderDanger";
    }
    else if (alias==QLatin1String("send"))
    {
        contextName="VoiceRecorderAccent";
    }
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

        //! The ticking value: elapsed/kept time outside Listening, playback position while
        //! Listening (task-voice-messages-plan.md, S4c). Split out of durationLabel below so the
        //! label that changes on every tick is a separate, individually width-reserved widget --
        //! see updateDuration() and updateTimeLabelWidth()'s own doc comment.
        QLabel* positionLabel=nullptr;
        QString positionMask;

        //! The kept/total duration -- shown alone outside Listening, or as "/ M:SS.T" beside
        //! positionLabel while Listening. Ticks itself only outside Listening (there is nothing
        //! else on screen then), hence its own mask tracked separately from positionLabel's.
        QLabel* durationLabel=nullptr;
        QString durationMask;

        //! Host-supplied, see setContextWidget(). Not owned: taken out, never destroyed, by
        //! setContextWidget(nullptr).
        QPointer<QWidget> context;
        bool contextVisible=false;

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

        //! Return/Enter sends while Pinned (recording, hands-free, buttons visible but nothing
        //! focusable to type Return into) -- sendButton itself is Qt::NoFocus like its siblings, so
        //! this is a shortcut rather than actually focusing it. Enabled only while Pinned (see
        //! applyState()): Held has no buttons row to send from yet, and Paused/Listening already
        //! send on Enter through commentEdit's own key handling (eventFilter()).
        QShortcut* sendShortcut=nullptr;

        QTimer* blinkTimer=nullptr;
        bool dim=false;

        //! The widest THIS dialog's own width has ever been reserved to -- see
        //! reserveContentWidth()'s own doc comment on why it only ever grows outside a reset.
        int reservedContentWidth=0;
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

    // Hidden outside Listening (see updateDuration()); its own reserved width, when shown, is
    // absorbed by the stretch below rather than shifting durationLabel/context, which sit after it.
    pimpl->positionLabel=new QLabel(pimpl->header);
    pimpl->positionLabel->setObjectName("positionLabel");
    pimpl->positionLabel->setVisible(false);
    headerLayout->addWidget(pimpl->positionLabel);

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
    // Enter sends, Ctrl/Cmd/Shift+Enter inserts a newline -- see eventFilter().
    pimpl->commentEdit->installEventFilter(this);
    layout->addWidget(pimpl->commentEdit);

    // Escape cancels the recording while typing a comment: handled entirely by
    // FloatingDialogFrame's own Qt::WindowShortcut Escape (floatingdialog.cpp), which discards the
    // recording from anywhere in this popup -- no shortcut of this dialog's own needed. See that
    // shortcut's own comment for why it connects activatedAmbiguously() as well as activated(): a
    // second enabled Escape shortcut in the same window (this dialog briefly had one here, on
    // commentEdit) makes BOTH ambiguous, firing neither's activated() at all.

    // Absorbs slack, so nothing ABOVE it ever does. Without this, collapsing (Paused/Listening's
    // playerRow+commentEdit hiding, applyState()) has a visible in-between frame: the FRAME has
    // not shrunk yet (adjustSize() is deferred one event-loop turn, applyState()'s own comment
    // explains why), and with no stretch to claim the now-empty space, QVBoxLayout hands it to
    // whichever visible item's own QSizePolicy::Preferred allows it to grow (every widget here,
    // by default) -- reading as the just-revealed #buttonsRow stretching down to fill the old,
    // still-tall frame before it catches up and shrinks. A trailing stretch is exactly what it
    // sounds like it should be: BELOW everything, so every row above stays pinned to its own
    // natural size at the TOP, and the empty gap moves to the bottom, where it is invisible
    // either way -- gone once the deferred adjustSize() actually runs, and unnoticeable before it.
    layout->addStretch(1);

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
    auto sendFromDialog=[this](){emit sendRequested(comment(),cropStart(),cropEnd());};
    connect(pimpl->sendButton,&IconTextButton::clicked,this,sendFromDialog);

    // Two alternative key sequences via setKeys(), NOT QKeySequence(Qt::Key_Return,Qt::Key_Enter):
    // that constructor builds a two-stroke CHORD (Return, then Enter), so a lone Return is only a
    // partial match -- nothing fires, and Qt's shortcut map then swallows the next key too (e.g. an
    // Escape) while resetting the half-matched chord. activatedAmbiguously() is paired with
    // activated() because another enabled shortcut on the same key in this window would otherwise
    // make Qt hand the press to one candidate at a time, cycling across presses.
    pimpl->sendShortcut=new QShortcut(this);
    pimpl->sendShortcut->setKeys({QKeySequence(Qt::Key_Return),QKeySequence(Qt::Key_Enter)});
    pimpl->sendShortcut->setContext(Qt::WindowShortcut);
    pimpl->sendShortcut->setEnabled(false);
    connect(pimpl->sendShortcut,&QShortcut::activated,this,sendFromDialog);
    connect(pimpl->sendShortcut,&QShortcut::activatedAmbiguously,this,sendFromDialog);
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
    connect(pimpl->bar,&WaveformBar::cropChanged,this,
        [this](qreal start, qreal end)
        {
            updateDuration();
            emit cropChanged(start,end);
        }
    );

    setWidget(content);

    // No bottom button row: Dialog<>'s constructor installs a Close button by default, and the
    // buttons that matter are in the content.
    setButtons({});

    retranslate();
    applyState();
    // After applyState(): reserveContentWidth() measures durationLabel/positionLabel's OWN
    // reserved widths (set by updateDuration(), called from applyState()), not their pre-tick
    // defaults -- see its own doc comment.
    reserveContentWidth();
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
    // Every other control in this dialog is Qt::NoFocus (pauseButton/cancelButton/sendButton/
    // listenButton), so commentEdit is the only widget here that can ever hold keyboard focus --
    // without grabbing it here, Enter-to-send (eventFilter()) needs an extra click into the field
    // first every time recording is paused, since becoming visible does not by itself move focus.
    // Guarded to fire once, on the Held/Pinned->Paused transition, not on every Paused<->Listening
    // toggle (which would otherwise yank focus back whenever Listen is clicked).
    const auto enteringReview=reviewing && pimpl->commentEdit->isHidden();
    pimpl->sendShortcut->setEnabled(state==State::Pinned);

    pimpl->targets->setVisible(held);
    pimpl->buttons->setVisible(!held);
    pimpl->playerRow->setVisible(reviewing);
    pimpl->commentEdit->setVisible(reviewing);
    if (enteringReview)
    {
        pimpl->commentEdit->setFocus(Qt::OtherFocusReason);
    }

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
    // a disabled button is not to look clickable: IconTextButton keeps the hand of its constructor otherwise
    pimpl->pauseButton->setCursor(state!=State::Listening ? Qt::PointingHandCursor : Qt::ArrowCursor);

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

    // Closable in every state, including Held/Pinned (user's decision, 2026-09-22): the title
    // bar's X button unconditionally discards on click, exactly like it already did while Paused/
    // Listening (onVoiceRecorderClosed() treats any close that is not mid-send as a cancel) --
    // hiding it only while recording, so it could not be clicked THERE, was the inconsistency:
    // if closing is accepted as a way to discard once Paused, there is no principled reason it
    // should be blocked while still recording. Left at AbstractDialog's own default (true) by
    // simply never calling setClosable(false) here at all.

    // Defensive: nothing above is supposed to touch the host's context widget, but a state
    // change (task-voice-messages-plan.md, S4c: Pause/Resume specifically) was reported to lose
    // it -- reasserted here rather than left dependent on nothing else ever clearing it, since
    // reading through every call this function makes did not turn up what does.
    if (!pimpl->context.isNull())
    {
        pimpl->context->setVisible(pimpl->contextVisible);
    }

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
                // adjustSize() alone leaves the newly exposed/shrunk area to Qt's ordinary
                // (queued) repaint, which on a WA_TranslucentBackground top-level can show one
                // frame of the raw, unpainted backing store -- black, since nothing has drawn
                // the QSS background there yet -- before the queued paint event catches up.
                // repaint() forces that paint synchronously, in the same tick as the resize.
                top->repaint();
            }
        }
    );
}

//--------------------------------------------------------------------------

/**
 * Reserves \p label's minimum width for the widest string its OWN font can render of the same
 * digit pattern as \p valueMs's formatted text -- see widestDigitsOf()'s own doc comment
 * (uise/desktop/utils/audiotime.hpp) for why a proportional-font clock needs this at all:
 * without it, the label (and the recorder popup that sizes itself exactly to its content) shifts
 * a pixel or two on every tick, which reads as a flicker.
 *
 * \p mask is the caller's own per-label cache of the last-applied mask, so a tick that keeps the
 * same digit PATTERN (the overwhelmingly common case -- the pattern only changes when the digit
 * COUNT does, e.g. crossing 9:59.9 to 10:00.0) recomputes nothing at all.
 */
void VoiceRecorderDialog::updateTimeLabelWidth(QLabel* label, QString& mask, const QString& sample)
{
    const auto newMask=widestDigitsOf(sample,label->fontMetrics());
    if (newMask==mask)
    {
        return;
    }
    mask=newMask;

    // FIXED, not a floor: AudioPlayerWidget's own version of this (which this otherwise mirrors)
    // reserves only a minimum width, on the reasoning that Qt's layout gives an item exactly its
    // sizeHint() -- itself already clamped up to minimumWidth() -- whenever there is slack space
    // to spare (an Expanding stretch item right next to it, here as there). That reasoning left
    // this label's LEFT NEIGHBOUR (durationLabel, immediately adjacent, not spanned by a stretch
    // the way AudioPlayerWidget's positionLabel/durationLabel are) visibly shifting anyway, so a
    // floor is not enough here -- fixing minimum AND maximum to the mask's own size hint removes
    // any dependency on exactly how the layout treats slack space: this label's occupied width is
    // then a hard constant, never merely a lower bound.
    const auto text=label->text();
    label->setMinimumWidth(0);
    label->setMaximumWidth(QWIDGETSIZE_MAX);
    label->setText(mask);
    const auto width=label->sizeHint().width();
    label->setText(text);
    label->setFixedWidth(width);
}

//--------------------------------------------------------------------------

void VoiceRecorderDialog::updateDuration()
{
    // What is recorded while recording. Paused and Listening show what is KEPT, the part between the crop
    // handles, since that is the length of the message that Send makes; and while listening the position
    // counts from the start of that part, as listening is limited to it.
    qint64 startMs=0;
    qint64 endMs=pimpl->elapsedMs;
    if (pimpl->state==State::Paused || pimpl->state==State::Listening)
    {
        startMs=qRound64(pimpl->bar->cropStart()*static_cast<qreal>(pimpl->elapsedMs));
        endMs=qRound64(pimpl->bar->cropEnd()*static_cast<qreal>(pimpl->elapsedMs));
    }
    const auto keptMs=std::max<qint64>(0,endMs-startMs);

    const auto listening=(pimpl->state==State::Listening);
    pimpl->positionLabel->setVisible(listening);

    if (listening)
    {
        // positionLabel is the one ticking here (once or twice per playback frame); durationLabel
        // shows the fixed "total" beside it, wrapped as " / M:SS.T" -- a DIFFERENT text pattern
        // than the plain "M:SS.T" the else branch below shows, and so a mask of its own: reusing
        // the plain one here previously left the wrapped text too wide for what had been reserved
        // for the plain one, clipping it on the right. A leading AND trailing space around the
        // slash, both the SAME glyph in the SAME label, keeps the gap on both sides identical --
        // relying on a QSS margin for one side and this text for the other could not guarantee that.
        const auto positionMs=std::clamp<qint64>(pimpl->playbackMs-startMs,0,keptMs);
        const auto durationText=tr(" / %1").arg(formatAudioTimeTenths(keptMs));
        updateTimeLabelWidth(pimpl->positionLabel,pimpl->positionMask,formatAudioTimeTenths(keptMs));
        updateTimeLabelWidth(pimpl->durationLabel,pimpl->durationMask,durationText);
        pimpl->positionLabel->setText(formatAudioTimeTenths(positionMs));
        pimpl->durationLabel->setText(durationText);
    }
    else
    {
        // Recording/Paused: durationLabel is the only number shown, and while actually recording
        // (Held/Pinned) it is the one ticking -- same mask treatment, applied to whichever label
        // is doing the ticking rather than to a fixed one.
        const auto durationText=formatAudioTimeTenths(keptMs);
        updateTimeLabelWidth(pimpl->durationLabel,pimpl->durationMask,durationText);
        pimpl->durationLabel->setText(durationText);
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
    updateDuration();
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

void VoiceRecorderDialog::setContextWidget(QWidget* widget)
{
    if (pimpl->context.data()==widget)
    {
        return;
    }

    if (!pimpl->context.isNull())
    {
        // Taken out, not destroyed: ownership of whatever the host built stays with the host.
        pimpl->header->layout()->removeWidget(pimpl->context);
        pimpl->context->setParent(nullptr);
    }

    pimpl->context=widget;

    if (widget!=nullptr)
    {
        widget->setParent(pimpl->header);
        static_cast<QBoxLayout*>(pimpl->header->layout())->addWidget(widget,0,Qt::AlignVCenter);
        widget->setVisible(pimpl->contextVisible);
    }

    // Grow-only (reset=false): a context widget attached while parked must not shrink this
    // dialog's own width back down the moment it is detached on the very next un-park, which
    // would itself be the same width flicker this exists to stop.
    reserveContentWidth();

    // The header may have widened or narrowed. Same deferred re-anchor -- and the same
    // repaint() to avoid one frame of unpainted black on a WA_TranslucentBackground top-level --
    // applyState() uses for its own visibility changes.
    Layout::activateUpward(this);
    QPointer<QWidget> top=window();
    QTimer::singleShot(0,this,
        [top]()
        {
            if (!top.isNull() && top->isVisible())
            {
                top->adjustSize();
                top->repaint();
            }
        }
    );
}

//--------------------------------------------------------------------------

QWidget* VoiceRecorderDialog::contextWidget() const
{
    return pimpl->context.data();
}

//--------------------------------------------------------------------------

void VoiceRecorderDialog::setContextVisible(bool enable)
{
    pimpl->contextVisible=enable;
    if (!pimpl->context.isNull())
    {
        pimpl->context->setVisible(enable);
    }
}

//--------------------------------------------------------------------------

bool VoiceRecorderDialog::isContextVisible() const
{
    return pimpl->contextVisible;
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

    // Translated words are a different length in a different language -- redone here so a
    // language change (changeEvent() below) keeps the reservation in step, not just the
    // construct()-time call this same method makes.
    reserveWidths();
}

//--------------------------------------------------------------------------

void VoiceRecorderDialog::reserveWidths()
{
    // The duration/position clock reserves its OWN width per tick instead, see
    // updateTimeLabelWidth() and updateDuration() -- a translation change does not need to redo
    // that here, since neither label's text is ever a translated word (digits and punctuation
    // only, see formatAudioTimeTenths()).

    // Pause<->Resume and Listen<->Pause (applyState()) are two different words, not two lengths
    // of the same one -- measured through the button's own sizeHint() (icon, padding and all)
    // rather than raw font metrics, so nothing here has to guess at IconTextButton's own layout.
    auto reserveButton=[](IconTextButton* button, const QString& textA, const QString& textB)
    {
        const auto original=button->text();
        button->setText(textA);
        const auto widthA=button->sizeHint().width();
        button->setText(textB);
        const auto widthB=button->sizeHint().width();
        button->setMinimumWidth(std::max(widthA,widthB));
        button->setText(original);
    };
    reserveButton(pimpl->pauseButton,tr("Pause"),tr("Resume"));
    reserveButton(pimpl->listenButton,tr("Listen"),tr("Pause"));
}

//--------------------------------------------------------------------------

/**
 * Held's #targetsRow, Pinned's #buttonsRow and Paused/Listening's #playerRow+#commentEdit each
 * have their own natural width, and so does the header row with or without a host-supplied
 * context widget (setContextWidget()) -- #recorderContent's own width (a QVBoxLayout's width is
 * the MAX of its visible children's) therefore changes with the state and with park status even
 * though every individual ticking label's OWN width is already stable (reserveWidths(),
 * updateTimeLabelWidth()). FloatingDialogFrame tracks this dialog's size hint exactly
 * (isResizable()==false), so that showed as the whole popup's WIDTH shifting a few pixels on
 * every Pause/Resume, on Held->Pinned, and on park/un-park.
 *
 * The fix: show every row and the context widget AT ONCE (bypassing whatever the current state
 * actually wants visible), measure this dialog's own natural width in that "everything visible"
 * configuration -- the widest it can ever be -- and pin THIS dialog's own width to exactly that,
 * fixed, so FloatingDialogFrame's tracked size hint never changes on the width axis again, only on
 * height (which still grows/shrinks with the state, as intended). Fixed on this dialog rather than
 * on #recorderContent itself: that QFrame has its own QSS min-width (voicerecorder.qss), and a
 * later re-polish (a theme switch) would silently overwrite a C++-set minimum width on the SAME
 * widget with that rule's 280px again.
 */
void VoiceRecorderDialog::reserveContentWidth(bool reset)
{
    // isHidden(), NOT isVisible(): isVisible() answers "is this actually on screen right now",
    // which is false for EVERY child whenever the top-level itself has never been shown yet --
    // exactly the case the very first call here runs under (construct() calls this right after
    // applyState(), before the popup is ever popped up). Read that way, every "was" below comes
    // back false regardless of what applyState() just set, and the restore at the end of this
    // function then HIDES whatever should have stayed visible (Held's own #targetsRow, the very
    // first time this runs) -- isHidden() reports this widget's own explicit flag instead,
    // independent of whether any ancestor is currently on screen.
    const auto wasTargets=!pimpl->targets->isHidden();
    const auto wasButtons=!pimpl->buttons->isHidden();
    const auto wasPlayer=!pimpl->playerRow->isHidden();
    const auto wasComment=!pimpl->commentEdit->isHidden();
    const auto wasPosition=!pimpl->positionLabel->isHidden();
    const auto wasContext=!pimpl->context.isNull() && !pimpl->context->isHidden();

    pimpl->targets->setVisible(true);
    pimpl->buttons->setVisible(true);
    pimpl->playerRow->setVisible(true);
    pimpl->commentEdit->setVisible(true);
    pimpl->positionLabel->setVisible(true);
    if (!pimpl->context.isNull())
    {
        pimpl->context->setVisible(true);
    }

    // sizeHint() for a widget with a layout is the layout's OWN size hint -- purely a function of
    // its children's current preferred sizes, unaffected by this widget's own prior
    // setFixedWidth() -- so no need to clear that first before re-measuring.
    if (layout()!=nullptr)
    {
        layout()->activate();
    }
    auto width=sizeHint().width();
    if (!reset)
    {
        width=std::max(width,pimpl->reservedContentWidth);
    }
    pimpl->reservedContentWidth=width;
    setFixedWidth(width);

    pimpl->targets->setVisible(wasTargets);
    pimpl->buttons->setVisible(wasButtons);
    pimpl->playerRow->setVisible(wasPlayer);
    pimpl->commentEdit->setVisible(wasComment);
    pimpl->positionLabel->setVisible(wasPosition);
    if (!pimpl->context.isNull())
    {
        pimpl->context->setVisible(wasContext);
    }
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
        // reset=true: the previous language's reservation may now be too wide (or too narrow),
        // and nothing about a NEW language should be held down to the OLD one's width.
        reserveContentWidth(true);
    }
}

//--------------------------------------------------------------------------

/**
 * #commentEdit is a bare QPlainTextEdit, which has no built-in Enter-to-send convention of its own
 * (unlike the main composer's EnhancedTextEdit) -- always inserting a newline on Return/Enter
 * regardless of modifiers. An event filter, not a QPlainTextEdit subclass, so this stays a small
 * addition to an otherwise stock widget rather than a new Q_OBJECT class in this .cpp.
 *
 * Escape is NOT handled here -- see FloatingDialogFrame's own Qt::WindowShortcut Escape
 * (floatingdialog.cpp), which owns it for this whole popup.
 */
bool VoiceRecorderDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched==pimpl->commentEdit && event->type()==QEvent::KeyPress)
    {
        auto* keyEvent=static_cast<QKeyEvent*>(event);
        if (keyEvent->key()==Qt::Key_Return || keyEvent->key()==Qt::Key_Enter)
        {
            // Same rule as the main composer's EnhancedTextEdit::isFinishKey(): plain Enter sends,
            // Ctrl/Cmd/Shift+Enter falls through below to insert a newline instead.
            const auto newLine=static_cast<bool>(keyEvent->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier));
            if (!newLine)
            {
                emit sendRequested(comment(),cropStart(),cropEnd());
                return true;
            }
            // Stripped, as EnhancedTextEdit does: QPlainTextEdit ignores Ctrl+Return (Cmd+Return on
            // macOS) outright, so without this only Shift+Enter would insert the newline.
            keyEvent->setModifiers(keyEvent->modifiers() & ~(Qt::ControlModifier | Qt::ShiftModifier));
        }
    }
    return Base::eventFilter(watched,event);
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
