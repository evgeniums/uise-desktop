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

/** @file uise/desktop/lineedit.cpp
*
*  Defines LineEdit.
*
*/

/****************************************************************************/

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/lineedit.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

void LineEdit::enterEvent(QEnterEvent* event)
{
    if (!m_parentHovered)
    {
        setProperty("hovered",true);
        updateIcons(true);
    }
    QLineEdit::enterEvent(event);
    Style::updateWidgetStyle(this);
    emit hovered(true);
}

//--------------------------------------------------------------------------

void LineEdit::leaveEvent(QEvent* event)
{
    if (!m_parentHovered)
    {
        setProperty("hovered",false);
        updateIcons(false);
    }

    QLineEdit::leaveEvent(event);
    Style::updateWidgetStyle(this);
    emit hovered(false);
}

//--------------------------------------------------------------------------

void LineEdit::setParentHovered(bool enable)
{
    m_parentHovered=enable;
    setProperty("hovered",enable);
    updateIcons(m_parentHovered);
    Style::updateWidgetStyle(this);
}

//--------------------------------------------------------------------------

ActionWithSvgIcon LineEdit::addActionWithSvgIcon(std::shared_ptr<SvgIcon> icon, QLineEdit::ActionPosition position)
{
    ActionWithSvgIcon action{std::move(icon)};
    action.action=addAction(action.icon->icon(),position);
    m_actions.push_back(action);
    return action;
}

//--------------------------------------------------------------------------

void LineEdit::updateIcons(bool hovered)
{
    for (auto& action : m_actions)
    {
        if (hovered)
        {
            action.action->setIcon(action.icon->hoverIcon());
        }
        else
        {
            action.action->setIcon(action.icon->icon());
        }
    }
}

//--------------------------------------------------------------------------

PushButton* LineEdit::addPushButton(std::shared_ptr<SvgIcon> icon, QLineEdit::ActionPosition position)
{
    auto button=new PushButton(std::move(icon),this);
    addPushButton(button,position);
    return button;
}

//--------------------------------------------------------------------------

void LineEdit::addPushButton(PushButton* button, QLineEdit::ActionPosition position)
{
    Q_ASSERT(button->qPushButton());

    if (position==QLineEdit::LeadingPosition)
    {
        m_leadingButtons.push_back(button);
    }
    else
    {
        m_trailingButtons.push_back(button);
    }

    if (m_updatePositionsTimer==nullptr)
    {
        m_updatePositionsTimer=new SingleShotTimer(this);
    }
    m_updatePositionsTimer->shot(
        1,
        [this]()
        {
            updateButtonPositions();
        }
    );
    button->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

void LineEdit::resizeEvent(QResizeEvent* event)
{
    QLineEdit::resizeEvent(event);
    updateButtonPositions();
}

//--------------------------------------------------------------------------

void LineEdit::updateButtonPositions()
{
    auto r=contentsRect();
    auto m=contentsMargins();
    auto h=r.height();
    QSize buttonSize(h,h);

    auto tm=textMargins();

    int lastX=0;
    int leftX=0;
    for (auto& button : m_leadingButtons)
    {
        if (button->isHidden())
        {
            continue;
        }
        if (leftX==0)
        {
            leftX=button->width()/4;
        }
        button->move(leftX,r.center().y()-button->height()/2);
        leftX+=button->width();
        lastX=leftX-button->width()/4;
    }

    auto rightX=r.right();
    auto rightW=m.right();
    for (auto& button : m_trailingButtons)
    {
        if (button->isHidden())
        {
            continue;
        }
        if (rightX==r.right())
        {
            rightX-=button->width()/4;
        }
        rightX-=button->width();
        rightW+=button->width();
        button->move(rightX,r.center().y()-button->height()/2);
    }

    setTextMargins(lastX,tm.top(),rightW,tm.bottom());
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
