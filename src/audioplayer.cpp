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

/** @file uise/desktop/src/audioplayer.cpp
*
*  Defines AudioPlayer and AudioPlayerWidget.
*
*/

/****************************************************************************/

#include <algorithm>
#include <vector>

#include <QCoreApplication>
#include <QCursor>
#include <QEnterEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QHideEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <uise/desktop/audioplayer.hpp>
#include <uise/desktop/dropdownframe.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/elidedlabel.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/ripple.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/svgiconlocator.hpp>
#include <uise/desktop/utils/audiotime.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/layout.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

namespace {

//! The pointer has to rest on the volume button this long before the slider opens.
constexpr int VolumeHoverOpenDelayMs=120;

//! How often the open slider checks whether the pointer is still over the button or the popup, and
//! for how many checks in a row it may be away before the popup closes (about 400 ms).
constexpr int VolumeHoverPollMs=100;
constexpr int VolumeHoverCloseTicks=4;

constexpr int VolumeSliderMax=100;

//! Exclusive group of the speed menu's checkable items.
constexpr int SpeedMenuGroup=0;

/**
 * The ripple of a button that shows text is a circle, the side of which is the height of the button, on the
 * middle of the text. The overlay covers the whole button and its insets are what cuts a square out of that,
 * so they are set from where the text really is, and not from what the style sheet believes about the box.
 */
void centerRippleOnText(IconTextButton* button)
{
    auto* ripple=button->rippleOverlay();
    auto* text=button->findChild<QLabel*>(QStringLiteral("text"));
    if (ripple==nullptr || text==nullptr || !text->isVisible() || button->width()<=0)
    {
        return;
    }

    const auto side=button->height();
    const auto center=text->geometry().center().x();
    auto left=std::max(0,center-side/2);
    const auto right=std::max(0,button->width()-(left+side));
    // a text so far to the right that the square would leave the button is moved in, not cut
    left=std::max(0,std::min(left,button->width()-side));

    ripple->setRippleInsetLeft(left);
    ripple->setRippleInsetRight(right);
}

//! Keep a button lit for as long as the dropdown that it opens is on screen.
void keepLitWhileOpen(IconTextButton* button, DropdownFrame* frame)
{
    // While the button is told that its parent is hovered it takes no Enter and no Leave for the hover, so the
    // pointer can go into the dropdown without the button going dark.
    QObject::connect(frame,&DropdownFrame::aboutToShow,button,
        [button]()
        {
            button->setParentHovered(true);
        }
    );

    // aboutToHide() comes from the hide event, so it is there whichever way the dropdown went, and after the
    // fade, not before it.
    QObject::connect(frame,&DropdownFrame::aboutToHide,button,
        [button]()
        {
            button->setParentHovered(false);

            // A pointer that is still on the button has had its Enter long ago, and the button has just gone
            // dark: light it again by an Enter of its own.
            const auto global=QCursor::pos();
            const auto local=button->mapFromGlobal(global);
            if (button->isVisible() && button->rect().contains(local))
            {
                QEnterEvent enter(local,local,global);
                QCoreApplication::sendEvent(button,&enter);
            }
        }
    );
}

std::shared_ptr<SvgIcon> playerIcon(const QString& alias, QWidget* context)
{
    return Style::instance().svgIconLocator().icon(QString("AudioPlayer::%1").arg(alias),context);
}

//! The glyph of a button that is drawn in a colour of its own, so it needs an icon context of its
//! own: a colour map is per context, never per alias (see svgiconlocator.cpp).
std::shared_ptr<SvgIcon> playerDangerIcon(const QString& alias, QWidget* context)
{
    return Style::instance().svgIconLocator().icon(QString("AudioPlayerDanger::%1").arg(alias),context);
}

/**
 * The clock string with every digit replaced by the widest digit of the font: "1:11" is narrower
 * than "0:00" in a proportional font, so the width that has to fit is the one of the widest
 * digits, not the one of the text that happens to be shown.
 */
QString widestDigitsOf(const QString& text, const QFontMetrics& metrics)
{
    auto widest=QLatin1Char('0');
    int widestAdvance=-1;
    for (char digit='0';digit<='9';digit++)
    {
        const auto advance=metrics.horizontalAdvance(QLatin1Char(digit));
        if (advance>widestAdvance)
        {
            widestAdvance=advance;
            widest=QLatin1Char(digit);
        }
    }

    auto mask=text;
    for (auto& ch: mask)
    {
        if (ch.isDigit())
        {
            ch=widest;
        }
    }
    return mask;
}

}

//==========================================================================
// AudioPlayerWidget
//==========================================================================

class AudioPlayerWidget_p
{
    public:

        bool panelMode=true;
        bool playing=false;
        bool titleClickable=true;

        //! The pointer is on the title AND the title is clickable -- the state the "hovered"
        //! property of #titleLabel carries.
        bool titleHovered=false;
        bool muted=false;
        qreal volume=1.0;
        qreal speed=1.0;
        qint64 duration=0;
        qint64 position=0;

        //! Set while a setter moves the slider or the menu, so that their own change signals are
        //! not taken for the user.
        bool applyingVolume=false;
        bool applyingSpeed=false;

        //! A centering of the speed button's ripple is already queued.
        bool speedRipplePending=false;

        QFrame* topRow=nullptr;
        QFrame* bottomRow=nullptr;
        ElidedLabel* titleLabel=nullptr;
        IconTextButton* volumeButton=nullptr;
        IconTextButton* speedButton=nullptr;
        IconTextButton* stopCloseButton=nullptr;
        IconTextButton* stopButton=nullptr;
        IconTextButton* playButton=nullptr;
        QLabel* positionLabel=nullptr;
        WaveformBar* bar=nullptr;
        QLabel* durationLabel=nullptr;

        //! The mask the position label was last measured with, so the measuring is not redone for
        //! every tick of the clock. Empty forces the next updateTimeLabelWidth() to measure again.
        QString timeMask;

        //! A re-measuring of the position label is already queued.
        bool timeWidthPending=false;

        //! Both are parentless top-level frames, destroyed by the widget's destructor.
        QPointer<DropdownFrame> volumeFrame;
        QPointer<QSlider> volumeSlider;
        QPointer<DropdownMenu> speedMenu;

        QTimer* hoverOpenTimer=nullptr;
        QTimer* hoverPollTimer=nullptr;
        int awayTicks=0;
};

//--------------------------------------------------------------------------

AudioPlayerWidget::AudioPlayerWidget(QWidget* parent)
    : WidgetQFrame(parent),
      pimpl(std::make_unique<AudioPlayerWidget_p>())
{
    auto layout=Layout::vertical(this);

    // ---- row 1: title, volume, speed ----------------------------------------------------

    pimpl->topRow=new QFrame(this);
    pimpl->topRow->setObjectName("topRow");
    auto topLayout=Layout::horizontal(pimpl->topRow);
    layout->addWidget(pimpl->topRow);

    pimpl->titleLabel=new ElidedLabel(pimpl->topRow);
    pimpl->titleLabel->setObjectName("titleLabel");
    pimpl->titleLabel->setElideMode(Qt::ElideRight);
    // a long title must be elided, not widen the player
    pimpl->titleLabel->setIgnoreSizeHint(true);
    pimpl->titleLabel->setCursor(Qt::PointingHandCursor);
    pimpl->titleLabel->setProperty("clickable",true);
    pimpl->titleLabel->installEventFilter(this);
    topLayout->addWidget(pimpl->titleLabel,1);

    pimpl->volumeButton=new IconTextButton(playerIcon("volumeHigh",this),pimpl->topRow);
    pimpl->volumeButton->setObjectName("volumeButton");
    pimpl->volumeButton->setText(QString());
    pimpl->volumeButton->setCursor(Qt::PointingHandCursor);
    pimpl->volumeButton->setFocusPolicy(Qt::NoFocus);
    pimpl->volumeButton->installEventFilter(this);
    topLayout->addWidget(pimpl->volumeButton);

    pimpl->speedButton=new IconTextButton(QString(),pimpl->topRow,IconTextButton::IconPosition::Invisible);
    pimpl->speedButton->setObjectName("speedButton");
    pimpl->speedButton->setCursor(Qt::PointingHandCursor);
    pimpl->speedButton->setFocusPolicy(Qt::NoFocus);
    pimpl->speedButton->installEventFilter(this);
    topLayout->addWidget(pimpl->speedButton);

    // ---- row 2: stop and close, stop, play/pause, position, progress, duration --------------

    pimpl->bottomRow=new QFrame(this);
    pimpl->bottomRow->setObjectName("bottomRow");
    auto bottomLayout=Layout::horizontal(pimpl->bottomRow);
    layout->addWidget(pimpl->bottomRow);

    // Dialog mode only, where closing the player is what stops it: a red button that does both at
    // once, so the user is not left hunting for the title bar's close button to stop a track. The
    // solid square of the filled icon set, and its own icon context for the red.
    pimpl->stopCloseButton=new IconTextButton(playerDangerIcon("stop",this),pimpl->bottomRow);
    pimpl->stopCloseButton->setObjectName("stopCloseButton");
    pimpl->stopCloseButton->setText(QString());
    pimpl->stopCloseButton->setCursor(Qt::PointingHandCursor);
    pimpl->stopCloseButton->setFocusPolicy(Qt::NoFocus);
    bottomLayout->addWidget(pimpl->stopCloseButton);

    pimpl->stopButton=new IconTextButton(playerIcon("stop",this),pimpl->bottomRow);
    pimpl->stopButton->setObjectName("stopButton");
    pimpl->stopButton->setText(QString());
    pimpl->stopButton->setCursor(Qt::PointingHandCursor);
    pimpl->stopButton->setFocusPolicy(Qt::NoFocus);
    bottomLayout->addWidget(pimpl->stopButton);

    pimpl->playButton=new IconTextButton(playerIcon("play",this),pimpl->bottomRow);
    pimpl->playButton->setObjectName("playButton");
    pimpl->playButton->setText(QString());
    pimpl->playButton->setCursor(Qt::PointingHandCursor);
    pimpl->playButton->setFocusPolicy(Qt::NoFocus);
    bottomLayout->addWidget(pimpl->playButton);

    pimpl->positionLabel=new QLabel(formatAudioTime(0),pimpl->bottomRow);
    pimpl->positionLabel->setObjectName("positionLabel");
    bottomLayout->addWidget(pimpl->positionLabel);

    pimpl->bar=new WaveformBar(pimpl->bottomRow);
    pimpl->bar->setObjectName("progressBar");
    pimpl->bar->setStyle(WaveformBar::Style::Line);
    bottomLayout->addWidget(pimpl->bar,1);

    pimpl->durationLabel=new QLabel(formatAudioTime(0),pimpl->bottomRow);
    pimpl->durationLabel->setObjectName("durationLabel");
    bottomLayout->addWidget(pimpl->durationLabel);

    // ---- volume popup: a vertical slider in a top-level dropdown, opened by hovering ----------

    auto* popup=new QFrame();
    popup->setObjectName("volumePopup");
    auto popupLayout=Layout::vertical(popup);

    pimpl->volumeSlider=new QSlider(Qt::Vertical,popup);
    pimpl->volumeSlider->setObjectName("volumeSlider");
    pimpl->volumeSlider->setRange(0,VolumeSliderMax);
    pimpl->volumeSlider->setValue(VolumeSliderMax);
    pimpl->volumeSlider->setFocusPolicy(Qt::NoFocus);
    popupLayout->addWidget(pimpl->volumeSlider);

    pimpl->volumeFrame=new DropdownFrame();
    // a top-level frame has no ancestor to hang a selector on, so it is styled by its own name
    pimpl->volumeFrame->setObjectName("volumeDropdown");
    pimpl->volumeFrame->setContent(popup);
    // Closed by the hover poll below. Self-dismissal would also arm an Escape shortcut for a
    // popup that only ever lives while the pointer is over it, which is one more Escape consumer
    // for the window to be ambiguous about.
    pimpl->volumeFrame->setSelfDismissEnabled(false);
    keepLitWhileOpen(pimpl->volumeButton,pimpl->volumeFrame.data());

    pimpl->hoverOpenTimer=new QTimer(this);
    pimpl->hoverOpenTimer->setSingleShot(true);
    pimpl->hoverOpenTimer->setInterval(VolumeHoverOpenDelayMs);
    connect(pimpl->hoverOpenTimer,&QTimer::timeout,this,
        [this]()
        {
            // the pointer may have left again, or a click may have muted meanwhile
            const auto pos=QCursor::pos();
            if (pimpl->volumeButton->isVisible()
                && pimpl->volumeButton->rect().contains(pimpl->volumeButton->mapFromGlobal(pos)))
            {
                openVolumePopup();
            }
        }
    );

    pimpl->hoverPollTimer=new QTimer(this);
    pimpl->hoverPollTimer->setInterval(VolumeHoverPollMs);
    connect(pimpl->hoverPollTimer,&QTimer::timeout,this,&AudioPlayerWidget::onVolumeHoverPoll);

    connect(pimpl->volumeSlider,&QSlider::valueChanged,this,
        [this](int value)
        {
            if (pimpl->applyingVolume)
            {
                return;
            }
            pimpl->volume=static_cast<qreal>(value)/VolumeSliderMax;
            const auto wasMuted=pimpl->muted;
            if (wasMuted && value>0)
            {
                // dragging the slider up is asking to hear it
                pimpl->muted=false;
            }
            updateVolumeButton();
            emit volumeChanged(pimpl->volume);
            if (wasMuted && !pimpl->muted)
            {
                emit mutedChanged(false);
            }
        }
    );

    connect(pimpl->volumeButton,&IconTextButton::clicked,this,
        [this]()
        {
            pimpl->muted=!pimpl->muted;
            updateVolumeButton();
            emit mutedChanged(pimpl->muted);
        }
    );

    // ---- speed menu ------------------------------------------------------------------------

    // Built once with all its items: attachTo() below wires the click that opens it, and the
    // items have to exist by then. Only their checkmarks and texts change afterwards.
    pimpl->speedMenu=new DropdownMenu();
    std::vector<MenuItem> items;
    const auto ratios=speeds();
    for (int i=0;i<ratios.size();i++)
    {
        auto item=MenuItem::checkable(i,QString(),qFuzzyCompare(ratios[i],1.0));
        item.group=SpeedMenuGroup;
        items.push_back(std::move(item));
    }
    pimpl->speedMenu->setItems(std::move(items));
    pimpl->speedMenu->setCloseOnCheckableActivation(true);
    pimpl->speedMenu->attachTo(pimpl->speedButton);
    keepLitWhileOpen(pimpl->speedButton,pimpl->speedMenu.data());
    connect(pimpl->speedMenu,&DropdownMenu::itemToggled,this,
        [this](int id, bool checked)
        {
            const auto ratios=speeds();
            // the unchecking of the previous item in the group is not a choice
            if (pimpl->applyingSpeed || !checked || id<0 || id>=ratios.size())
            {
                return;
            }
            pimpl->speed=ratios[id];
            updateSpeedButton();
            emit speedChanged(pimpl->speed);
        }
    );

    // ---- transport and seeking -----------------------------------------------------------

    connect(pimpl->stopCloseButton,&IconTextButton::clicked,this,[this](){emit stopAndCloseRequested();});
    connect(pimpl->stopButton,&IconTextButton::clicked,this,[this](){emit stopRequested();});
    connect(pimpl->playButton,&IconTextButton::clicked,this,
        [this]()
        {
            if (pimpl->playing)
            {
                emit pauseRequested();
            }
            else
            {
                emit playRequested();
            }
        }
    );

    // while the bar is being dragged the labels follow the finger; the host is told once, on release
    connect(pimpl->bar,&WaveformBar::seekRequested,this,
        [this](qreal fraction)
        {
            pimpl->positionLabel->setText(formatAudioTime(static_cast<qint64>(fraction*static_cast<qreal>(pimpl->duration))));
        }
    );
    connect(pimpl->bar,&WaveformBar::seekFinished,this,
        [this](qreal fraction)
        {
            const auto target=static_cast<qint64>(fraction*static_cast<qreal>(pimpl->duration));
            pimpl->position=target;
            updateTimeLabels();
            emit seekRequested(target);
        }
    );

    retranslate();
    // the two stop buttons exclude each other, and nothing has told us the mode yet
    setPanelMode(pimpl->panelMode);
    updateVolumeButton();
    updatePlayButton();
    updateTimeLabels();
}

//--------------------------------------------------------------------------

AudioPlayerWidget::~AudioPlayerWidget()
{
    if (!pimpl->volumeFrame.isNull())
    {
        destroyWidget(pimpl->volumeFrame);
    }
    if (!pimpl->speedMenu.isNull())
    {
        destroyWidget(pimpl->speedMenu);
    }
}

//--------------------------------------------------------------------------

QList<qreal> AudioPlayerWidget::speeds()
{
    return QList<qreal>{0.5,0.75,1.0,1.25,1.5,2.0};
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::setPanelMode(bool panel)
{
    pimpl->panelMode=panel;
    pimpl->stopButton->setVisible(panel);
    // never both: a panel is stopped without being closed, a dialog is closed by being stopped
    pimpl->stopCloseButton->setVisible(!panel);
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::setTitle(const QString& title)
{
    pimpl->titleLabel->setText(title);
    pimpl->titleLabel->setToolTip(title);
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::setTitleClickable(bool enable)
{
    pimpl->titleClickable=enable;
    pimpl->titleLabel->setCursor(enable?Qt::PointingHandCursor:Qt::ArrowCursor);
    pimpl->titleLabel->setProperty("clickable",enable);
    if (!enable)
    {
        // a title that is no longer a link is not lit, wherever the pointer happens to be
        pimpl->titleHovered=false;
    }
    pimpl->titleLabel->setProperty("hovered",pimpl->titleHovered);

    // Recursive, not updateWidgetStyle(): both properties live on the ElidedLabel frame while the
    // colour they pick is on the QLabel INSIDE it, and a descendant selector is only re-resolved
    // when that child is repolished too.
    Style::repolishRecursive(pimpl->titleLabel);
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::updateTitleHover(bool hovered)
{
    // a title with nowhere to go does not light up, and its Enter is not worth a repolish
    const auto lit=hovered && pimpl->titleClickable;
    if (lit==pimpl->titleHovered)
    {
        return;
    }
    pimpl->titleHovered=lit;
    pimpl->titleLabel->setProperty("hovered",lit);
    Style::repolishRecursive(pimpl->titleLabel);
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::setDuration(qint64 ms)
{
    pimpl->duration=std::max<qint64>(0,ms);
    updateTimeLabels();
    updateProgress();
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::setPosition(qint64 ms)
{
    pimpl->position=std::max<qint64>(0,ms);
    if (!pimpl->bar->isSeeking())
    {
        updateTimeLabels();
        updateProgress();
    }
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::setPlaying(bool playing)
{
    pimpl->playing=playing;
    updatePlayButton();
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::setVolume(qreal volume)
{
    pimpl->volume=std::min<qreal>(1.0,std::max<qreal>(0.0,volume));
    if (!pimpl->volumeSlider.isNull())
    {
        // the slider's own signal must not come back as if the user had moved it
        pimpl->applyingVolume=true;
        pimpl->volumeSlider->setValue(qRound(pimpl->volume*VolumeSliderMax));
        pimpl->applyingVolume=false;
    }
    updateVolumeButton();
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::setMuted(bool muted)
{
    pimpl->muted=muted;
    updateVolumeButton();
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::setSpeed(qreal speed)
{
    pimpl->speed=speed;

    // check the menu item that matches, or none when the host chose a ratio the menu lacks
    const auto ratios=speeds();
    pimpl->applyingSpeed=true;
    for (int i=0;i<ratios.size();i++)
    {
        pimpl->speedMenu->setItemChecked(i,qFuzzyCompare(ratios[i],speed));
    }
    pimpl->applyingSpeed=false;

    updateSpeedButton();
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::setWaveform(const QByteArray& waveform)
{
    pimpl->bar->setWaveform(waveform);
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::setProgressStyle(WaveformBar::Style style)
{
    pimpl->bar->setStyle(style);
}

//--------------------------------------------------------------------------

WaveformBar* AudioPlayerWidget::waveformBar() const noexcept
{
    return pimpl->bar;
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::retranslate()
{
    pimpl->stopCloseButton->setToolTip(tr("Stop and close"));
    pimpl->stopButton->setToolTip(tr("Stop"));
    pimpl->speedButton->setToolTip(tr("Playback speed"));

    const auto ratios=speeds();
    for (int i=0;i<ratios.size();i++)
    {
        pimpl->speedMenu->setItemText(i,tr("%1×").arg(QString::number(ratios[i])));
    }

    updateSpeedButton();
    updateVolumeButton();
    updatePlayButton();
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::updateVolumeButton()
{
    const auto silent=pimpl->muted || pimpl->volume<=0.0;

    // Two arcs, one arc, crossed out: the icon loses a step as the level goes down. The names are of what is
    // drawn, not of the tabler files: tabler's "volume" has two arcs and its "volume-2" has one.
    QString alias("volumeHigh");
    if (silent)
    {
        alias="volumeOff";
    }
    else if (pimpl->volume<0.5)
    {
        alias="volumeLow";
    }
    pimpl->volumeButton->setSvgIcon(playerIcon(alias,this));
    pimpl->volumeButton->setToolTip(pimpl->muted?tr("Unmute"):tr("Mute"));
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::updateSpeedButton()
{
    pimpl->speedButton->setText(tr("%1×").arg(QString::number(pimpl->speed)));
    scheduleSpeedRippleCentering();
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::scheduleSpeedRippleCentering()
{
    if (pimpl->speedRipplePending)
    {
        return;
    }
    pimpl->speedRipplePending=true;
    QTimer::singleShot(0,this,
        [this]()
        {
            pimpl->speedRipplePending=false;
            centerRippleOnText(pimpl->speedButton);
        }
    );
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::updatePlayButton()
{
    pimpl->playButton->setSvgIcon(playerIcon(pimpl->playing?"pause":"play",this));
    pimpl->playButton->setToolTip(pimpl->playing?tr("Pause"):tr("Play"));
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::updateTimeLabels()
{
    // before the texts, so the label is already wide enough for the one it is about to show
    updateTimeLabelWidth();
    pimpl->positionLabel->setText(formatAudioTime(pimpl->position));
    pimpl->durationLabel->setText(formatAudioTime(pimpl->duration));
}

//--------------------------------------------------------------------------

/**
 * The clock is drawn in a proportional font, where "0:01" and "0:02" are not the same number of
 * pixels: the position label's size hint changed with nearly every tick, and with it the row, the
 * player and -- the player being a dialog of its natural size -- the dialog's own window, which
 * kept shifting while a track played. The label is given a floor instead, the width of the widest
 * string the clock can reach here: the position does not pass the duration, so the duration's own
 * formatting is what has to fit, with every digit taken at its widest (see widestDigitsOf()). A
 * duration that is not known yet (a stream) is no bound at all, hence the position as well -- the
 * step at "9:59" to "10:00" is then the one width change left, once per track.
 *
 * A floor and not a fixed width: no string of this track is wider than the mask, so the floor IS
 * the width the label settles at, and there is no cap to clip a string that turns out longer.
 *
 * The width is the label's OWN size hint for the mask rather than a font-metrics sum, so whatever
 * the style sheet puts around the text (see the #positionLabel rule in audioplayer.qss) counts the
 * same way it does for any other text the label shows.
 */
void AudioPlayerWidget::updateTimeLabelWidth()
{
    const auto longest=std::max(pimpl->duration,pimpl->position);
    const auto mask=widestDigitsOf(formatAudioTime(longest),pimpl->positionLabel->fontMetrics());
    if (mask==pimpl->timeMask)
    {
        return;
    }
    pimpl->timeMask=mask;

    // measured with no floor of its own: the one left by an earlier, longer mask would otherwise
    // be the answer, and a shorter track would keep the wider label
    const auto text=pimpl->positionLabel->text();
    pimpl->positionLabel->setMinimumWidth(0);
    pimpl->positionLabel->setText(mask);
    const auto width=pimpl->positionLabel->sizeHint().width();
    pimpl->positionLabel->setText(text);
    pimpl->positionLabel->setMinimumWidth(width);
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::updateProgress()
{
    qreal fraction=0.0;
    if (pimpl->duration>0)
    {
        fraction=static_cast<qreal>(pimpl->position)/static_cast<qreal>(pimpl->duration);
    }
    pimpl->bar->setProgress(fraction);
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::openVolumePopup()
{
    auto* frame=pimpl->volumeFrame.data();
    if (frame==nullptr || frame->isOpen())
    {
        return;
    }
    frame->popupAbove(pimpl->volumeButton);
    pimpl->awayTicks=0;
    pimpl->hoverPollTimer->start();
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::raiseVolumePopup()
{
    if (pimpl->volumeFrame.isNull() || !pimpl->volumeFrame->isOpen())
    {
        return;
    }

    QPointer<DropdownFrame> frame(pimpl->volumeFrame);
    QTimer::singleShot(0,this,
        [frame]()
        {
            // the popup may have closed in the meantime -- raise() on a hidden frame would
            // order its window back on screen
            if (!frame.isNull() && frame->isOpen())
            {
                frame->raise();
            }
        }
    );
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::closeVolumePopup()
{
    pimpl->hoverOpenTimer->stop();
    pimpl->hoverPollTimer->stop();
    pimpl->awayTicks=0;
    if (!pimpl->volumeFrame.isNull() && pimpl->volumeFrame->isOpen())
    {
        pimpl->volumeFrame->closeDropdown();
    }
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::onVolumeHoverPoll()
{
    auto* frame=pimpl->volumeFrame.data();
    if (frame==nullptr || !frame->isOpen())
    {
        pimpl->hoverPollTimer->stop();
        return;
    }

    const auto pos=QCursor::pos();
    const auto overButton=pimpl->volumeButton->rect().contains(pimpl->volumeButton->mapFromGlobal(pos));
    const auto overPopup=frame->frameGeometry().contains(pos);

    // a drag that strayed off the popup must not close it under the finger
    if (overButton || overPopup || pimpl->volumeSlider->isSliderDown())
    {
        pimpl->awayTicks=0;
        return;
    }

    if (++pimpl->awayTicks>=VolumeHoverCloseTicks)
    {
        closeVolumePopup();
    }
}

//--------------------------------------------------------------------------

bool AudioPlayerWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched==pimpl->volumeButton)
    {
        switch (event->type())
        {
            case (QEvent::Enter):
            {
                if (!pimpl->volumeFrame.isNull() && !pimpl->volumeFrame->isOpen())
                {
                    pimpl->hoverOpenTimer->start();
                }
                break;
            }

            case (QEvent::Leave):
            {
                // passing through is not asking for the slider
                pimpl->hoverOpenTimer->stop();
                break;
            }

            case (QEvent::MouseButtonPress):
            {
                /*
                 * The slider's dropdown and a player hosted in a FloatingAudioPlayerDialog are
                 * both Qt::Tool top-levels, so they share one window level (NSFloatingWindowLevel
                 * on macOS) and the platform orders them by use: the press that toggles mute
                 * brings the player's own window to the front of that level and leaves the
                 * still-open slider -- the pointer never left the button, so nothing closed it --
                 * stacked behind the player. Nothing in Qt reports a restack, so put the frame
                 * back on top by hand, the same way FloatingDialogFrame re-asserts itself above
                 * its host. Queued so it runs after the platform's own ordering for this press,
                 * which on other platforms may only happen once the press has been dispatched.
                 */
                raiseVolumePopup();
                break;
            }

            default:
                break;
        }
    }
    else if (watched==pimpl->speedButton)
    {
        switch (event->type())
        {
            // the text moves with a change of the layout, a new text or a new style, and the layout is not done yet when this comes
            case (QEvent::Resize):
            case (QEvent::Show):
            case (QEvent::LayoutRequest):
            case (QEvent::StyleChange):
            {
                scheduleSpeedRippleCentering();
                break;
            }

            default:
                break;
        }
    }
    else if (watched==pimpl->titleLabel)
    {
        switch (event->type())
        {
            /*
             * The pointer is really over the QLabel inside the ElidedLabel, but Qt sends Enter and
             * Leave to every widget between the old and the new position's common ancestor
             * (QApplicationPrivate::dispatchEnterLeave()), so the frame hears about its own child
             * -- and hears nothing when the pointer only moves between the child and the frame's
             * own padding, which is what "the pointer is on the title" should mean.
             */
            case (QEvent::Enter):
            {
                updateTitleHover(true);
                break;
            }

            case (QEvent::Leave):
            {
                updateTitleHover(false);
                break;
            }

            // The inner QLabel takes no text interaction, so it ignores the button events and
            // QApplication::notify() walks them up to this frame, remapped to its coordinates.
            case (QEvent::MouseButtonRelease):
            {
                if (pimpl->titleClickable)
                {
                    auto* mouseEvent=static_cast<QMouseEvent*>(event);
                    if (mouseEvent->button()==Qt::LeftButton
                        && pimpl->titleLabel->rect().contains(mouseEvent->position().toPoint()))
                    {
                        emit titleClicked();
                    }
                }
                break;
            }

            default:
                break;
        }
    }

    // never consumed, this filter only observes
    return WidgetQFrame::eventFilter(watched,event);
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::changeEvent(QEvent* event)
{
    WidgetQFrame::changeEvent(event);
    switch (event->type())
    {
        case (QEvent::LanguageChange):
        {
            retranslate();
            break;
        }

        /*
         * The width of the mask is a width in THIS font under THIS style sheet, and the first
         * measuring happens in the constructor, before either has been applied.
         *
         * Queued rather than done here: a style sheet's own "min-width" reaches the label as a
         * setMinimumWidth() of its own (QStyleSheetStyle::setGeometry()), which is the very floor
         * this method writes, and the polish that does it is not ordered against this event. By
         * the next turn of the event loop it has happened, whichever way round the two came.
         */
        case (QEvent::FontChange):
        case (QEvent::StyleChange):
        {
            // one of these may reach a widget whose children are not all built yet
            if (pimpl->positionLabel!=nullptr && !pimpl->timeWidthPending)
            {
                pimpl->timeWidthPending=true;
                QTimer::singleShot(0,this,
                    [this]()
                    {
                        pimpl->timeWidthPending=false;
                        pimpl->timeMask.clear();
                        updateTimeLabelWidth();
                    }
                );
            }
            break;
        }

        default:
            break;
    }
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::hideEvent(QHideEvent* event)
{
    // a popup of a hidden player has nothing left to point at
    closeVolumePopup();
    WidgetQFrame::hideEvent(event);
}

//==========================================================================
// AudioPlayer
//==========================================================================

//--------------------------------------------------------------------------

Widget* AudioPlayer::doCreateActualWidget(QWidget* parent)
{
    auto* widget=new AudioPlayerWidget(parent);
    m_widget=widget;

    // user input up to the host
    connect(widget,&AudioPlayerWidget::playRequested,this,&AbstractAudioPlayer::playRequested);
    connect(widget,&AudioPlayerWidget::pauseRequested,this,&AbstractAudioPlayer::pauseRequested);
    connect(widget,&AudioPlayerWidget::stopRequested,this,&AbstractAudioPlayer::stopRequested);
    connect(widget,&AudioPlayerWidget::stopAndCloseRequested,this,&AbstractAudioPlayer::stopAndCloseRequested);
    connect(widget,&AudioPlayerWidget::seekRequested,this,&AbstractAudioPlayer::seekRequested);
    connect(widget,&AudioPlayerWidget::titleClicked,this,&AbstractAudioPlayer::titleClicked);
    // these three also have to be remembered, so the getters and a later setEngine() see them
    connect(widget,&AudioPlayerWidget::volumeChanged,this,&AudioPlayer::userChangedVolume);
    connect(widget,&AudioPlayerWidget::mutedChanged,this,&AudioPlayer::userChangedMuted);
    connect(widget,&AudioPlayerWidget::speedChanged,this,&AudioPlayer::userChangedSpeed);

    // whatever the host set before the widget existed
    widget->setPanelMode(mode()==Mode::Panel);
    widget->setTitle(title());
    widget->setTitleClickable(isTitleClickable());
    widget->setDuration(duration());
    widget->setPosition(position());
    widget->setPlaying(isPlaying());
    widget->setVolume(volume());
    widget->setMuted(isMuted());
    widget->setSpeed(speed());
    widget->setWaveform(waveform());
    widget->setProgressStyle(progressStyle());

    return widget;
}

//--------------------------------------------------------------------------

void AudioPlayer::updateMode()
{
    if (!m_widget.isNull())
    {
        m_widget->setPanelMode(mode()==Mode::Panel);
    }
}

//--------------------------------------------------------------------------

void AudioPlayer::updateTitle()
{
    if (!m_widget.isNull())
    {
        m_widget->setTitle(title());
    }
}

//--------------------------------------------------------------------------

void AudioPlayer::updateTitleClickable()
{
    if (!m_widget.isNull())
    {
        m_widget->setTitleClickable(isTitleClickable());
    }
}

//--------------------------------------------------------------------------

void AudioPlayer::updateDuration()
{
    if (!m_widget.isNull())
    {
        m_widget->setDuration(duration());
    }
}

//--------------------------------------------------------------------------

void AudioPlayer::updatePosition()
{
    if (!m_widget.isNull())
    {
        m_widget->setPosition(position());
    }
}

//--------------------------------------------------------------------------

void AudioPlayer::updatePlaying()
{
    if (!m_widget.isNull())
    {
        m_widget->setPlaying(isPlaying());
    }
}

//--------------------------------------------------------------------------

void AudioPlayer::updateVolume()
{
    if (!m_widget.isNull())
    {
        m_widget->setVolume(volume());
    }
}

//--------------------------------------------------------------------------

void AudioPlayer::updateMuted()
{
    if (!m_widget.isNull())
    {
        m_widget->setMuted(isMuted());
    }
}

//--------------------------------------------------------------------------

void AudioPlayer::updateSpeed()
{
    if (!m_widget.isNull())
    {
        m_widget->setSpeed(speed());
    }
}

//--------------------------------------------------------------------------

void AudioPlayer::updateWaveform()
{
    if (!m_widget.isNull())
    {
        m_widget->setWaveform(waveform());
    }
}

//--------------------------------------------------------------------------

void AudioPlayer::updateProgressStyle()
{
    if (!m_widget.isNull())
    {
        m_widget->setProgressStyle(progressStyle());
    }
}

//--------------------------------------------------------------------------

}
