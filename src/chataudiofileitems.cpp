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
        setInfoText(tr("%1 · %2").arg(formatAudioTime(static_cast<qint64>(voice.voiceDurationMs())),
                                         formatFileSize(voice.size())));
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

WaveformBar* ChatVoiceFileItem::waveformBar() const noexcept
{
    return pimpl->bar;
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
