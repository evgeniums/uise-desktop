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

#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

PushButton::PushButton(std::shared_ptr<SvgIcon> icon, QWidget* parent, bool toolButton)
    : QFrame(parent),
    m_icon(std::move(icon)),
    m_pushButton(nullptr),
    m_toolButton(nullptr),
    m_parentHovered(false)
{
    auto l=Layout::vertical(this);

    if (toolButton)
    {
        m_toolButton=new QToolButton(this);
        m_button=m_toolButton;
    }
    else
    {
        m_pushButton=new QPushButton(this);
        m_button=m_pushButton;
    }

    l->addWidget(m_button,0,Qt::AlignCenter);

    if (m_icon)
    {
        setIcon(m_icon->icon());
    }

    connect(m_button,&QPushButton::clicked,this,&PushButton::clicked);
    connect(m_button,&QPushButton::toggled,this,&PushButton::toggled);
}

//--------------------------------------------------------------------------

void PushButton::enterEvent(QEnterEvent* event)
{
    if (!m_parentHovered)
    {
        setProperty("hovered",true);
        if (m_icon)
        {
            m_button->setIcon(m_icon->hoverIcon());
        }
    }
    QFrame::enterEvent(event);
    Style::updateWidgetStyle(m_button);
    emit hovered(true);
}

//--------------------------------------------------------------------------

void PushButton::leaveEvent(QEvent* event)
{
    if (!m_parentHovered)
    {
        setProperty("hovered",false);
        if (m_icon)
        {
            m_button->setIcon(m_icon->icon());
        }
    }

    QFrame::leaveEvent(event);
    Style::updateWidgetStyle(m_button);
    emit hovered(false);
}

//--------------------------------------------------------------------------

void PushButton::setParentHovered(bool enable)
{
    m_parentHovered=enable;
    setProperty("hovered",enable);
    if (m_icon)
    {        
        if (enable)
        {            
            m_button->setIcon(m_icon->hoverIcon());
        }
        else
        {
            m_button->setIcon(m_icon->icon());
        }
    }

    Style::updateWidgetStyle(m_button);
}

//--------------------------------------------------------------------------

void PushButton::setChecked(bool enable)
{
    m_button->setChecked(enable);
}

//--------------------------------------------------------------------------

void PushButton::resetHover()
{
    setProperty("hovered",false);
    if (m_icon)
    {
        m_button->setIcon(m_icon->icon());
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
