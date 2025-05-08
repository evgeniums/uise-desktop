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

/** @file uise/desktop/utils/singleshottimer.hpp
*
*  Contains implementation of SingleShorTimer.
*
*/

/****************************************************************************/

#include <uise/desktop/utils/singleshottimer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------
SingleShotTimer::SingleShotTimer(
        QObject* parent
    ) : QObject(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer,SIGNAL(timeout()),this,SLOT(onTimeout()));
}

//--------------------------------------------------------------------------
void SingleShotTimer::onTimeout()
{
    if (m_handler)
    {
        m_handler();
    }
}

//--------------------------------------------------------------------------
void SingleShotTimer::shot(size_t milliseconds, HandlerT handler)
{
    m_handler=std::move(handler);

    // in case of successive calls activate timer as fast as possible
    if (!m_timer.isActive() || m_timer.interval()>milliseconds)
    {
        m_timer.setInterval(milliseconds);
        m_timer.start();
    }
}

//--------------------------------------------------------------------------
void SingleShotTimer::clear()
{
    m_timer.stop();
    m_handler=HandlerT();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
