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

/** @file uise/desktop/src/chataudiofileitems.cpp
*
*  Defines ChatVoiceFileItem and ChatAudioFileItem.
*
*/

/****************************************************************************/

#include <algorithm>

#include <QBoxLayout>
#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QMouseEvent>
#include <QShowEvent>

#include <uise/desktop/chataudiofileitems.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/svgiconlocator.hpp>
#include <uise/desktop/utils/audiotime.hpp>
#include <uise/desktop/utils/filesizeformat.hpp>
#include <uise/desktop/utils/layout.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

namespace {

//! Between the waveform and the info line under it.
constexpr int WaveformSpacing=2;

std::shared_ptr<SvgIcon> fileIcon(const QString& alias, QWidget* context)
{
    return Style::instance().svgIconLocator().icon(QString("ChatMessageFiles::%1").arg(alias),context);
}

//! task-voice-messages-plan.md S4f item 6: the voice row's play/pause glyph gets its own icon
//! context, ChatVoiceFileItem, coloured to match the waveform's played colour -- a colour map is
//! per CONTEXT, never per alias, so it cannot share ChatMessageFiles (grey, every other file-row
//! glyph) without recolouring those too.
std::shared_ptr<SvgIcon> voiceIcon(const QString& alias, QWidget* context)
{
    return Style::instance().svgIconLocator().icon(QString("ChatVoiceFileItem::%1").arg(alias),context);
}

//! The states in which the file is there to play: the same two in which the icon slot shows the
//! file icon rather than the load control.
bool isPlayable(const ChatFileItem& item)
{
    return item.state()==ChatFileTransferState::Ready || item.state()==ChatFileTransferState::Unresolved;
}

}

//==========================================================================
// ChatVoiceFileItem
//==========================================================================

class ChatVoiceFileItem_p
{
    public:

        //! The tile over the icon slot; the button in it only draws the glyph, the tile takes the click.
        QFrame* overlay=nullptr;
        IconTextButton* playButton=nullptr;
        WaveformBar* bar=nullptr;
        QFrame* infoRow=nullptr;
        QLabel* dot=nullptr;

        //! task-voice-messages-plan.md (S4f item 3). The playing clock -- shown INSTEAD OF
        //! infoLabel() while showingPosition is true, never together. TWO SEPARATE labels, not one
        //! "position / duration" string: the position digits change width every tick in a
        //! proportional font, and if the separator and duration text shared that same string, they
        //! would shift sideways with it. Splitting them so only positionLabel (below, floored) ever
        //! changes text is what stops that -- the same fix already used in this codebase's other
        //! two playback clocks (AudioPlayerWidget's positionLabel/durationLabel pair either side of
        //! its progress bar, and the recorder popup's own clock), applied here without a bar to
        //! lean on for the visual split, hence the still-needed explicit "/" in the second label.
        QFrame* clockFrame=nullptr;
        QLabel* positionLabel=nullptr;
        //! "/ <duration>" -- static for the run of one session (the duration does not tick), so it
        //! needs no floor of its own; positionLabel's floor alone keeps it from ever shifting.
        QLabel* durationSuffixLabel=nullptr;

        //! While true the info line is the clock; the host turns it off with a negative position
        //! when the session ends, which 0 cannot say.
        bool showingPosition=false;
        qint64 positionMs=0;
        qint64 positionDurationMs=0;

        //! The widest-digit mask positionLabel's minimum width is currently reserved for -- see
        //! reserveInfoWidth(). Empty means no floor is reserved.
        QString timeMask;
};

//--------------------------------------------------------------------------

ChatVoiceFileItem::ChatVoiceFileItem(QWidget* parent)
    : ChatMessageFileItem(parent),
      pimpl(std::make_unique<ChatVoiceFileItem_p>())
{
    // ---- the play button, over the icon slot -------------------------------------------------

    // A tile the size of the slot, because IconTextButton does not centre a lone icon in a frame
    // wider than itself: the tile centres the button, and the tile is what takes the click.
    pimpl->overlay=new QFrame(iconSlot());
    pimpl->overlay->setObjectName("playOverlay");
    // the slot's other children are placed by geometry too, all at the slot's full size
    pimpl->overlay->setGeometry(iconSlot()->rect());
    pimpl->overlay->setCursor(Qt::PointingHandCursor);
    pimpl->overlay->setVisible(false);
    pimpl->overlay->installEventFilter(this);

    auto* overlayLayout=Layout::horizontal(pimpl->overlay);
    pimpl->playButton=new IconTextButton(voiceIcon("play",this),pimpl->overlay);
    pimpl->playButton->setObjectName("playButton");
    pimpl->playButton->setText(QString());
    pimpl->playButton->setFocusPolicy(Qt::NoFocus);
    pimpl->playButton->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    overlayLayout->addWidget(pimpl->playButton,1,Qt::AlignCenter);

    // ---- waveform and info line in the text column ------------------------------------------

    pimpl->bar=new WaveformBar(this);
    pimpl->bar->setObjectName("voiceWaveform");
    pimpl->bar->setStyle(WaveformBar::Style::Bars);
    connect(pimpl->bar,&WaveformBar::seekFinished,this,[this](qreal fraction){emit seekRequested(fraction);});

    // The info line is moved into a row of its own, to make room for the "listened" dot on its
    // left. Its widget stays the base row's own infoLabel(), so the base keeps rewriting it.
    pimpl->infoRow=new QFrame(this);
    pimpl->infoRow->setObjectName("infoRow");
    auto* rowLayout=Layout::horizontal(pimpl->infoRow);

    pimpl->dot=new QLabel(pimpl->infoRow);
    pimpl->dot->setObjectName("listenedDot");
    pimpl->dot->setVisible(false);
    rowLayout->addWidget(pimpl->dot,0,Qt::AlignVCenter);

    auto* column=textColumnLayout();
    const auto infoIndex=column->indexOf(infoLabel());
    column->removeWidget(infoLabel());
    rowLayout->addWidget(infoLabel(),1);

    // The playing clock: built here, alongside infoLabel(), and kept hidden until a session is
    // current -- see the pimpl fields' own doc comment for why it is two labels, not one string.
    pimpl->clockFrame=new QFrame(pimpl->infoRow);
    pimpl->clockFrame->setObjectName("clockFrame");
    pimpl->clockFrame->setVisible(false);
    auto* clockLayout=Layout::horizontal(pimpl->clockFrame);
    pimpl->positionLabel=new QLabel(pimpl->clockFrame);
    pimpl->positionLabel->setObjectName("positionLabel");
    clockLayout->addWidget(pimpl->positionLabel);
    pimpl->durationSuffixLabel=new QLabel(pimpl->clockFrame);
    pimpl->durationSuffixLabel->setObjectName("durationSuffixLabel");
    clockLayout->addWidget(pimpl->durationSuffixLabel);
    // clockFrame itself is stretched to fill the row (below, stretch 1, matching infoLabel()'s own
    // stretch); without this, THAT leftover width has nowhere to go but into positionLabel and
    // durationSuffixLabel themselves (neither has a maximum width), stretching a visible gap into
    // the middle of the clock. A trailing stretch item absorbs it instead, at the row's own end.
    clockLayout->addStretch(1);
    rowLayout->addWidget(pimpl->clockFrame,1);

    // name, spacer, waveform, spacer, info row -- the info line's place is now the waveform's
    column->insertWidget(infoIndex,pimpl->bar);
    column->insertSpacing(infoIndex+1,WaveformSpacing);
    column->insertWidget(infoIndex+2,pimpl->infoRow);

    retranslate();
}

//--------------------------------------------------------------------------

ChatVoiceFileItem::~ChatVoiceFileItem()
{
}

//--------------------------------------------------------------------------

void ChatVoiceFileItem::refresh()
{
    ChatMessageFileItem::refresh();

    const auto& voice=item();
    const auto playable=isPlayable(voice);

    setNameText(tr("Voice message"));

    pimpl->bar->setWaveform(voice.voiceWaveform());
    // the waveform comes with the message, so it is drawn at once; there is nothing to seek in
    // until the audio is here
    pimpl->bar->setSeekable(playable);

    pimpl->dot->setVisible(voice.isListenedByPeer());

    if (playable)
    {
        // While it transfers the base row's own progress text stays, it says more than this would.
        // updateInfoText() picks "position / duration" or "duration · size" on its own, from
        // pimpl->showingPosition -- a row rebuilt (flyweight recycle) while its item plays must
        // still show the clock, not stale duration text.
        updateInfoText();
    }

    pimpl->overlay->setVisible(playable);
    updatePlayButton();
}

//--------------------------------------------------------------------------

void ChatVoiceFileItem::setPlaybackProgress(qreal fraction)
{
    // WaveformBar drops it while the user is dragging, so a position tick never fights the finger
    pimpl->bar->setProgress(fraction);
}

//--------------------------------------------------------------------------

void ChatVoiceFileItem::setPlaybackPosition(qint64 positionMs, qint64 durationMs)
{
    if (positionMs<0)
    {
        // the session ended: back to "duration · size", and the clock's width floor goes with it
        if (!pimpl->showingPosition)
        {
            return;
        }
        pimpl->showingPosition=false;
        pimpl->timeMask.clear();
        pimpl->positionLabel->setMinimumWidth(0);
        updateInfoText();
        return;
    }

    pimpl->showingPosition=true;
    pimpl->positionMs=positionMs;
    pimpl->positionDurationMs=durationMs>0 ? durationMs : static_cast<qint64>(item().voiceDurationMs());
    updateInfoText();
}

//--------------------------------------------------------------------------

WaveformBar* ChatVoiceFileItem::waveformBar() const noexcept
{
    return pimpl->bar;
}

//--------------------------------------------------------------------------

QRect ChatVoiceFileItem::lastLineRect() const
{
    // Hint-derived, never geometry() -- see ChatMessageFileItem::lastLineRect()'s own doc comment.
    //
    // The column is now `stretch(1), nameLabel, spacing, bar, spacing, infoRow, stretch(1)` (see
    // the ctor) -- both stretches still equal, so its leftover height (this row's own height being
    // driven by whichever is taller, the 36px icon slot or this now-waveform-carrying column)
    // splits evenly above and below the pair, exactly like the base row's name/size block, unless
    // setTextVerticalAlignment(Qt::AlignTop) pinned it to the top instead.
    auto columnHeight=textColumnLayout()->sizeHint().height();
    auto inner=sizeHint().height()-contentsMargins().top()-contentsMargins().bottom();
    auto leftover=std::max(0,inner-columnHeight);
    auto topShare=(textVerticalAlignment()==Qt::AlignTop) ? 0 : leftover/2;
    auto bottom=contentsMargins().top()+topShare+columnHeight;

    auto infoHeight=pimpl->infoRow->sizeHint().height();
    // x/width are never read: ChatMessageFiles::allowsInlineBottom() refuses the inline path for
    // a caption-less voice row's line (only its bottom edge, used to measure the row's own bottom
    // padding as dead space, is read) -- kept plausible rather than exact.
    return QRect(0,bottom-infoHeight,pimpl->infoRow->sizeHint().width(),infoHeight);
}

//--------------------------------------------------------------------------

void ChatVoiceFileItem::updatePlayButton()
{
    const auto playing=item().isPlaying();
    pimpl->playButton->setSvgIcon(voiceIcon(playing?"pause":"play",this));
    pimpl->overlay->setToolTip(playing?tr("Pause"):tr("Play"));

    // above the file icon that the base row shows in the same place
    pimpl->overlay->raise();
}

//--------------------------------------------------------------------------

void ChatVoiceFileItem::updateInfoText()
{
    infoLabel()->setVisible(!pimpl->showingPosition);
    pimpl->clockFrame->setVisible(pimpl->showingPosition);

    if (pimpl->showingPosition)
    {
        // Digits and a colon need no translation (see formatAudioTime()'s own doc comment), and
        // neither does a bare "/" -- same reasoning, one more universal character.
        const auto durationText=formatAudioTime(pimpl->positionDurationMs);

        // The floor is measured against the DURATION's own text: the position can read anything
        // from "0:00" up to the duration, never past it, so the duration is the widest thing
        // positionLabel will ever have to show.
        reserveInfoWidth(durationText);

        pimpl->positionLabel->setText(formatAudioTime(pimpl->positionMs));
        // Static for the run of this session -- see its own field doc comment -- so it is simply
        // set, no floor of its own needed.
        // Leading space: clockLayout is zero-spacing (Layout::horizontal()'s own default), so the
        // gap on both sides of the slash has to come from the text itself to read as "0:05 / 0:12"
        // rather than "0:05/ 0:12".
        pimpl->durationSuffixLabel->setText(QStringLiteral(" / %1").arg(durationText));
        return;
    }

    setInfoText(tr("%1 · %2").arg(formatAudioTime(static_cast<qint64>(item().voiceDurationMs())),
                                  formatFileSize(item().size())));
}

//--------------------------------------------------------------------------

void ChatVoiceFileItem::reserveInfoWidth(const QString& longest)
{
    // The clock is drawn in a proportional font, where "0:01" and "0:02" are not the same number
    // of pixels: ticking positionLabel's text directly would change ITS size hint on nearly every
    // tick, and with it everything laid out after it -- the separator and duration text, if they
    // shared its string (the bug this whole split avoids), or the row/bubble around it otherwise.
    // Same fix, same helper, as AudioPlayerWidget::updateTimeLabelWidth() -- a minimum width
    // measured from the widest string this label can reach, every digit taken at its widest.
    auto mask=widestDigitsOf(longest,pimpl->positionLabel->fontMetrics());
    if (mask==pimpl->timeMask)
    {
        return;
    }
    pimpl->timeMask=mask;

    // measured with no floor of its own: an earlier, longer mask's floor would otherwise be the
    // answer
    const auto text=pimpl->positionLabel->text();
    pimpl->positionLabel->setMinimumWidth(0);
    pimpl->positionLabel->setText(mask);
    const auto width=pimpl->positionLabel->sizeHint().width();
    pimpl->positionLabel->setText(text);
    pimpl->positionLabel->setMinimumWidth(width);
}

//--------------------------------------------------------------------------

void ChatVoiceFileItem::retranslate()
{
    setNameText(tr("Voice message"));
    pimpl->dot->setToolTip(tr("Listened"));
    updatePlayButton();
}

//--------------------------------------------------------------------------

bool ChatVoiceFileItem::eventFilter(QObject* watched, QEvent* event)
{
    if (watched==pimpl->overlay && event->type()==QEvent::MouseButtonRelease)
    {
        auto* mouseEvent=static_cast<QMouseEvent*>(event);
        if (mouseEvent->button()==Qt::LeftButton && pimpl->overlay->rect().contains(mouseEvent->position().toPoint()))
        {
            if (item().isPlaying())
            {
                emit stopRequested();
            }
            else
            {
                emit playRequested();
            }
        }
        // Not consumed: the press reached the row as well (the whole row is one drag handle), and
        // the release has to end that gesture the same way.
    }
    return ChatMessageFileItem::eventFilter(watched,event);
}

//--------------------------------------------------------------------------

void ChatVoiceFileItem::changeEvent(QEvent* event)
{
    ChatMessageFileItem::changeEvent(event);
    if (event->type()==QEvent::LanguageChange)
    {
        retranslate();
    }
}

//==========================================================================
// ChatAudioFileItem
//==========================================================================

//--------------------------------------------------------------------------

void ChatAudioFileItem::refresh()
{
    ChatMessageFileItem::refresh();
    applyIcon();
}

//--------------------------------------------------------------------------

void ChatAudioFileItem::applyIcon()
{
    setTypeIcon(fileIcon(item().isPlaying()?"pause":"audioFile",this));
}

//--------------------------------------------------------------------------

void ChatAudioFileItem::showEvent(QShowEvent* event)
{
    ChatMessageFileItem::showEvent(event);

    // refresh() of a row built before it is shown could not replace the icon yet: the children
    // are shown, and so visible, only by now
    applyIcon();
}

//==========================================================================

ChatMessageFileItem* makeChatAudioFileItem(const ChatFileItem& item, QWidget* parent)
{
    if (item.isVoice())
    {
        return new ChatVoiceFileItem(parent);
    }
    if (item.isAudio())
    {
        return new ChatAudioFileItem(parent);
    }
    return nullptr;
}

//--------------------------------------------------------------------------

}
