/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/pushbutton.cpp
*
*  Defines PushButton.
*
*/

/****************************************************************************/

#include <QPaintEvent>
#include <QStylePainter>
#include <QStyleOptionButton>

#include <uise/desktop/pushbutton.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

void PushButton::enterEvent(QEnterEvent* event)
{
    if (m_icon)
    {
        setIcon(m_icon->hoverIcon());
    }
    QPushButton::enterEvent(event);
}

//--------------------------------------------------------------------------

void PushButton::leaveEvent(QEvent* event)
{
    if (m_icon)
    {
        setIcon(m_icon->icon());
    }
    QPushButton::leaveEvent(event);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
