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

#include <QCursor>
#include <QEvent>
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

std::shared_ptr<SvgIcon> playerIcon(const QString& alias, QWidget* context)
{
    return Style::instance().svgIconLocator().icon(QString("AudioPlayer::%1").arg(alias),context);
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
        bool muted=false;
        qreal volume=1.0;
        qreal speed=1.0;
        qint64 duration=0;
        qint64 position=0;

        //! Set while a setter moves the slider or the menu, so that their own change signals are
        //! not taken for the user.
        bool applyingVolume=false;
        bool applyingSpeed=false;

        QFrame* topRow=nullptr;
        QFrame* bottomRow=nullptr;
        ElidedLabel* titleLabel=nullptr;
        IconTextButton* volumeButton=nullptr;
        IconTextButton* speedButton=nullptr;
        IconTextButton* stopButton=nullptr;
        IconTextButton* playButton=nullptr;
        QLabel* positionLabel=nullptr;
        WaveformBar* bar=nullptr;
        QLabel* durationLabel=nullptr;

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

    pimpl->volumeButton=new IconTextButton(playerIcon("volume2",this),pimpl->topRow);
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
    topLayout->addWidget(pimpl->speedButton);

    // ---- row 2: stop, play/pause, position, progress, duration -----------------------------

    pimpl->bottomRow=new QFrame(this);
    pimpl->bottomRow->setObjectName("bottomRow");
    auto bottomLayout=Layout::horizontal(pimpl->bottomRow);
    layout->addWidget(pimpl->bottomRow);

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
    Style::updateWidgetStyle(pimpl->titleLabel);
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

    QString alias("volume2");
    if (silent)
    {
        alias="volumeOff";
    }
    else if (pimpl->volume<0.5)
    {
        alias="volume";
    }
    pimpl->volumeButton->setSvgIcon(playerIcon(alias,this));
    pimpl->volumeButton->setToolTip(pimpl->muted?tr("Unmute"):tr("Mute"));
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::updateSpeedButton()
{
    pimpl->speedButton->setText(tr("%1×").arg(QString::number(pimpl->speed)));
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
    pimpl->positionLabel->setText(formatAudioTime(pimpl->position));
    pimpl->durationLabel->setText(formatAudioTime(pimpl->duration));
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

            default:
                break;
        }
    }
    else if (watched==pimpl->titleLabel)
    {
        if (event->type()==QEvent::MouseButtonRelease && pimpl->titleClickable)
        {
            auto* mouseEvent=static_cast<QMouseEvent*>(event);
            if (mouseEvent->button()==Qt::LeftButton
                && pimpl->titleLabel->rect().contains(mouseEvent->position().toPoint()))
            {
                emit titleClicked();
            }
        }
    }

    // never consumed, this filter only observes
    return WidgetQFrame::eventFilter(watched,event);
}

//--------------------------------------------------------------------------

void AudioPlayerWidget::changeEvent(QEvent* event)
{
    WidgetQFrame::changeEvent(event);
    if (event->type()==QEvent::LanguageChange)
    {
        retranslate();
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
