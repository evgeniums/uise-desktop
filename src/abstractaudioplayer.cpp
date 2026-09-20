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

/** @file uise/desktop/src/abstractaudioplayer.cpp
*
*  Defines AbstractAudioPlayer.
*
*/

/****************************************************************************/

#include <uise/desktop/abstractaudioplayer.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

//--------------------------------------------------------------------------

AbstractAudioPlayer::~AbstractAudioPlayer()
{
    // Not the engine's to keep calling into a player that is gone
    if (!m_engine.isNull())
    {
        disconnect(m_engine.data(),nullptr,this,nullptr);
    }
}

//--------------------------------------------------------------------------

void AbstractAudioPlayer::userChangedVolume(qreal volume)
{
    m_volume=std::min<qreal>(1.0,std::max<qreal>(0.0,volume));
    emit volumeChanged(m_volume);
}

//--------------------------------------------------------------------------

void AbstractAudioPlayer::userChangedMuted(bool muted)
{
    m_muted=muted;
    emit mutedChanged(m_muted);
}

//--------------------------------------------------------------------------

void AbstractAudioPlayer::userChangedSpeed(qreal speed)
{
    m_speed=speed;
    emit speedChanged(m_speed);
}

//--------------------------------------------------------------------------

void AbstractAudioPlayer::setEngine(AudioPlaybackEngine* engine)
{
    if (m_engine.data()==engine)
    {
        return;
    }

    if (!m_engine.isNull())
    {
        disconnect(m_engine.data(),nullptr,this,nullptr);
        disconnect(this,nullptr,m_engine.data(),nullptr);
    }

    m_engine=engine;
    if (engine==nullptr)
    {
        return;
    }

    // engine -> display. setPosition() and friends are the host-side setters: they update the
    // widget and emit nothing, so nothing here loops back into the engine.
    connect(engine,&AudioPlaybackEngine::positionChanged,this,&AbstractAudioPlayer::setPosition);
    connect(engine,&AudioPlaybackEngine::durationChanged,this,&AbstractAudioPlayer::setDuration);
    connect(engine,&AudioPlaybackEngine::errorOccurred,this,&AbstractAudioPlayer::errorOccurred);
    connect(engine,&AudioPlaybackEngine::stateChanged,this,
        [this](AudioPlaybackEngine::State state)
        {
            setPlaying(state==AudioPlaybackEngine::State::Playing);
            if (state==AudioPlaybackEngine::State::Stopped)
            {
                // a finished or stopped clip is back at its start
                setPosition(0);
            }
        }
    );

    // user requests -> engine
    connect(this,&AbstractAudioPlayer::playRequested,engine,&AudioPlaybackEngine::play);
    connect(this,&AbstractAudioPlayer::pauseRequested,engine,&AudioPlaybackEngine::pause);
    connect(this,&AbstractAudioPlayer::stopRequested,engine,&AudioPlaybackEngine::stop);
    connect(this,&AbstractAudioPlayer::seekRequested,engine,&AudioPlaybackEngine::seekMs);
    connect(this,&AbstractAudioPlayer::volumeChanged,engine,&AudioPlaybackEngine::setVolume);
    connect(this,&AbstractAudioPlayer::mutedChanged,engine,&AudioPlaybackEngine::setMuted);
    connect(this,&AbstractAudioPlayer::speedChanged,engine,&AudioPlaybackEngine::setPlaybackRate);

    engine->setVolume(m_volume);
    engine->setMuted(m_muted);
    engine->setPlaybackRate(m_speed);
}

//--------------------------------------------------------------------------

}
