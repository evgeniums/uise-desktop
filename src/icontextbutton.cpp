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

/** @file uise/desktop/icontextbutton.cpp
*
*  Defines IconTextButton.
*
*/

/****************************************************************************/

#include <QLabel>
#include <QMouseEvent>

#include <uise/desktop/style.hpp>
#include <uise/desktop/roundedimage.hpp>
#include <uise/desktop/icontextbutton.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

IconTextButton::IconTextButton(std::shared_ptr<SvgIcon> icon, QWidget* parent, IconPosition iconPosition)
    : QFrame(parent),
      m_iconPosition(iconPosition),
      m_layout(nullptr),
      m_icon(nullptr),
      m_text(nullptr),
      m_parentHovered(false),
      m_checked(false),
      m_checkable(false)
{
    m_icon=new RoundedImage(this);
    m_icon->setDisableHover(true);
    m_text=new QLabel(this);
    m_text->setObjectName("text");
    setIconPosition(iconPosition);

    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

void IconTextButton::setHovered(bool enable)
{
    setProperty("hovered",enable);    
    m_text->setProperty("hovered",enable);
    Style::updateWidgetStyle(m_text);
    m_icon->setParentHovered(enable);
    m_text->repaint();
}

//--------------------------------------------------------------------------

void IconTextButton::enterEvent(QEnterEvent* event)
{
    if (!m_parentHovered)
    {
        event->accept();
        setHovered(true);
        emit hovered(true);
        return;
    }
    QFrame::enterEvent(event);
}

//--------------------------------------------------------------------------

void IconTextButton::leaveEvent(QEvent* event)
{    
    if (!m_parentHovered)
    {        
        setHovered(false);
        emit hovered(false);
        event->accept();
        return;
    }
    QFrame::leaveEvent(event);
}

//--------------------------------------------------------------------------

void IconTextButton::setParentHovered(bool enable)
{
    m_parentHovered=enable;
    setHovered(enable);
}

//--------------------------------------------------------------------------

void IconTextButton::setChecked(bool enable)
{
    if (!m_checkable)
    {
        return;
    }

    auto prevChecked=m_checked;

    m_checked=enable;
    setProperty("checked",enable);
    m_icon->setSelected(enable);
    m_text->setProperty("checked",enable);
    Style::updateWidgetStyle(this);
    Style::updateWidgetStyle(m_text);
    Style::updateWidgetStyle(m_icon);

    if (prevChecked!=m_checked)
    {
        emit toggled(m_checked);
    }
}

//--------------------------------------------------------------------------

void IconTextButton::toggle()
{
    setChecked(!m_checked);
}

//--------------------------------------------------------------------------

void IconTextButton::click()
{
    emit clicked();
    toggle();
}

//--------------------------------------------------------------------------

QString IconTextButton::text() const
{
    return m_text->text();
}

//--------------------------------------------------------------------------

void IconTextButton::setText(const QString& text)
{
    m_text->setText(text);
}

//--------------------------------------------------------------------------

void IconTextButton::setSvgIcon(std::shared_ptr<SvgIcon> icon)
{
    m_icon->setSvgIcon(std::move(icon));
}

//--------------------------------------------------------------------------

std::shared_ptr<SvgIcon> IconTextButton::svgIcon() const
{
    return m_icon->svgIcon();
}

//--------------------------------------------------------------------------

void IconTextButton::setIconPosition(IconPosition iconPosition)
{
    if (m_layout)
    {
        m_layout->removeWidget(m_icon);
        m_layout->removeWidget(m_text);
    }

    m_iconPosition=iconPosition;
    m_icon->setVisible(true);

    switch (m_iconPosition)
    {
        case (IconPosition::BeforeText):
        {
            m_layout=Layout::horizontal(this);
            m_layout->addWidget(m_icon);
            m_layout->addWidget(m_text);
            m_text->setProperty("position","after");
        }
        break;

        case IconPosition::AfterText:
        {
            m_layout=Layout::horizontal(this);
            m_layout->addWidget(m_text);
            m_layout->addWidget(m_icon);
            m_text->setProperty("position","before");
        }
        break;

        case IconPosition::AboveText:
        {
            m_layout=Layout::vertical(this);
            m_layout->addWidget(m_icon,0,Qt::AlignCenter);
            m_layout->addWidget(m_text,0,Qt::AlignCenter);
            m_text->setProperty("position","below");
        }
        break;

        case IconPosition::BelowText:
        {
            m_layout=Layout::vertical(this);
            m_layout->addWidget(m_text,0,Qt::AlignCenter);
            m_layout->addWidget(m_icon,0,Qt::AlignCenter);
            m_text->setProperty("position","above");
        }
        break;

        case IconPosition::Invisible:
        {
            m_layout=Layout::horizontal(this);
            m_layout->addWidget(m_text,0,Qt::AlignCenter);
            m_layout->addWidget(m_icon,0,Qt::AlignCenter);
            m_text->setProperty("position",QVariant{});
            m_icon->setVisible(false);
        }
        break;
    }
}

//--------------------------------------------------------------------------

void IconTextButton::mousePressEvent(QMouseEvent* event)
{
    if (event->button()==Qt::LeftButton)
    {
        click();
    }
    QFrame::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void IconTextButton::setTextInteractionFlags(Qt::TextInteractionFlags flags)
{
    m_text->setTextInteractionFlags(flags);
}

//--------------------------------------------------------------------------

Qt::TextInteractionFlags IconTextButton::textInteractionFlags() const
{
    return m_text->textInteractionFlags();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
