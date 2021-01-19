/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** \file uise/desktop/utils/singleshottimer.hpp
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

UISE_DESKTOP_NAMESPACE_EMD
