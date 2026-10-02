/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/utils/frameticker.hpp
*
*  Contains implementation of FrameTicker.
*
*/

/****************************************************************************/

#include <uise/desktop/utils/frameticker.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------
FrameTicker::FrameTicker(
        QObject* parent
    ) : QAbstractAnimation(parent)
{
}

//--------------------------------------------------------------------------
void FrameTicker::setHandler(HandlerT handler)
{
    m_handler=std::move(handler);
}

//--------------------------------------------------------------------------
void FrameTicker::startTicking()
{
    if (isTicking())
    {
        return;
    }

    // QAbstractAnimation::start() synchronously calls updateCurrentTime(0); the -1 sentinel makes
    // that first call only record the start time instead of invoking the handler.
    m_lastTime=-1;
    start();
}

//--------------------------------------------------------------------------
void FrameTicker::stopTicking()
{
    if (!isTicking())
    {
        return;
    }
    stop();
    m_lastTime=-1;
}

//--------------------------------------------------------------------------
void FrameTicker::updateCurrentTime(int currentTime)
{
    if (m_lastTime<0)
    {
        m_lastTime=currentTime;
        return;
    }

    auto dt=currentTime-m_lastTime;
    m_lastTime=currentTime;
    if (dt<=0 || !m_handler)
    {
        return;
    }
    m_handler(dt);
}

UISE_DESKTOP_NAMESPACE_END
