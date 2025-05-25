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

/** @file uise/desktop/pushm_button.cpp
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
    if (!m_parentHovered && !m_button->isChecked())
    {
        setProperty("hovered",true);
        if (m_icon)
        {
            m_button->setIcon(m_icon->hoverIcon());
        }
    }
    QFrame::enterEvent(event);

    style()->unpolish(this);
    style()->polish(this);

    style()->unpolish(m_button);
    style()->polish(m_button);
}

//--------------------------------------------------------------------------

void PushButton::leaveEvent(QEvent* event)
{
    if (m_parentHovered)
    {
        return;
    }

    if (!m_parentHovered && !m_button->isChecked())
    {
        setProperty("hovered",false);

        if (m_icon)
        {
            m_button->setIcon(m_icon->icon());
        }
    }
    QFrame::leaveEvent(event);

    style()->unpolish(this);
    style()->polish(this);

    style()->unpolish(m_button);
    style()->polish(m_button);
}

//--------------------------------------------------------------------------

void PushButton::setParentHovered(bool enable)
{
    m_parentHovered=enable;
    if (!m_button->isChecked() && m_icon)
    {
        setProperty("hovered",enable);
        if (enable)
        {            
            m_button->setIcon(m_icon->hoverIcon());
        }
        else
        {
            m_button->setIcon(m_icon->icon());
        }
    }

    style()->unpolish(this);
    style()->polish(this);

    style()->unpolish(m_button);
    style()->polish(m_button);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
