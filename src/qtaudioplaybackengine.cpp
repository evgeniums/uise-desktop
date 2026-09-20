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

/** @file uise/desktop/src/qtaudioplaybackengine.cpp
*
*  Defines QtAudioPlaybackEngine.
*
*/

/****************************************************************************/

#include <QAudioOutput>
#include <QMediaPlayer>

#include <uise/desktop/qtaudioplaybackengine.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

AudioPlaybackEngine::State toEngineState(QMediaPlayer::PlaybackState state)
{
    switch (state)
    {
        case (QMediaPlayer::PlayingState):
            return AudioPlaybackEngine::State::Playing;

        case (QMediaPlayer::PausedState):
            return AudioPlaybackEngine::State::Paused;

        case (QMediaPlayer::StoppedState):
            break;
    }
    return AudioPlaybackEngine::State::Stopped;
}

}

//--------------------------------------------------------------------------

class QtAudioPlaybackEngine_p
{
    public:

        QMediaPlayer* player=nullptr;
        QAudioOutput* output=nullptr;
};

//--------------------------------------------------------------------------

QtAudioPlaybackEngine::QtAudioPlaybackEngine(QObject* parent)
    : AudioPlaybackEngine(parent),
      pimpl(std::make_unique<QtAudioPlaybackEngine_p>())
{
    pimpl->player=new QMediaPlayer(this);
    pimpl->output=new QAudioOutput(this);
    pimpl->player->setAudioOutput(pimpl->output);

    connect(pimpl->player,&QMediaPlayer::positionChanged,this,&AudioPlaybackEngine::positionChanged);
    connect(pimpl->player,&QMediaPlayer::durationChanged,this,&AudioPlaybackEngine::durationChanged);
    connect(pimpl->player,&QMediaPlayer::playbackStateChanged,this,
        [this](QMediaPlayer::PlaybackState state)
        {
            emit stateChanged(toEngineState(state));
        }
    );
    connect(pimpl->player,&QMediaPlayer::errorOccurred,this,
        [this](QMediaPlayer::Error error, const QString& message)
        {
            if (error!=QMediaPlayer::NoError)
            {
                emit errorOccurred(message);
            }
        }
    );
}

//--------------------------------------------------------------------------

QtAudioPlaybackEngine::~QtAudioPlaybackEngine()
{
    // stop before the members go, so no callback runs into a half-destroyed engine
    pimpl->player->stop();
}

//--------------------------------------------------------------------------

void QtAudioPlaybackEngine::open(const QString& source)
{
    QUrl url(source);
    if (url.scheme().size()<=1)
    {
        // a plain path: no scheme at all, or a Windows drive letter that parsed as a one-letter one
        url=QUrl::fromLocalFile(source);
    }
    pimpl->player->setSource(url);
}

//--------------------------------------------------------------------------

void QtAudioPlaybackEngine::openDevice(QIODevice* device, const QUrl& hint)
{
    pimpl->player->setSourceDevice(device,hint);
}

//--------------------------------------------------------------------------

void QtAudioPlaybackEngine::play()
{
    pimpl->player->play();
}

//--------------------------------------------------------------------------

void QtAudioPlaybackEngine::pause()
{
    pimpl->player->pause();
}

//--------------------------------------------------------------------------

void QtAudioPlaybackEngine::stop()
{
    pimpl->player->stop();
}

//--------------------------------------------------------------------------

void QtAudioPlaybackEngine::seekMs(qint64 ms)
{
    pimpl->player->setPosition(ms);
}

//--------------------------------------------------------------------------

void QtAudioPlaybackEngine::setVolume(qreal volume)
{
    pimpl->output->setVolume(static_cast<float>(volume));
}

//--------------------------------------------------------------------------

void QtAudioPlaybackEngine::setMuted(bool muted)
{
    pimpl->output->setMuted(muted);
}

//--------------------------------------------------------------------------

void QtAudioPlaybackEngine::setPlaybackRate(qreal rate)
{
    pimpl->player->setPlaybackRate(rate);
}

//--------------------------------------------------------------------------

AudioPlaybackEngine::State QtAudioPlaybackEngine::state() const
{
    return toEngineState(pimpl->player->playbackState());
}

//--------------------------------------------------------------------------

qint64 QtAudioPlaybackEngine::positionMs() const
{
    return pimpl->player->position();
}

//--------------------------------------------------------------------------

qint64 QtAudioPlaybackEngine::durationMs() const
{
    return pimpl->player->duration();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
