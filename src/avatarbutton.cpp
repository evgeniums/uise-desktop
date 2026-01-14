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

/** @file uise/desktop/avatarbutton.cpp
*
*  Defines AvatarButton.
*
*/

/****************************************************************************/

#include <QLabel>
#include <QMouseEvent>

#include <uise/desktop/style.hpp>
#include <uise/desktop/avatar.hpp>
#include <uise/desktop/avatarbutton.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

AvatarButton::AvatarButton(QWidget* parent)
    : QFrame(parent),
      m_layout(nullptr),
      m_avatar(nullptr),
      m_tailIcon(nullptr),
      m_text(nullptr),
      m_parentHovered(false),
      m_checked(false),
      m_checkable(false)
{
    m_avatar=new AvatarWidget(this);
    m_avatar->setDisableHover(true);
    m_avatar->setVisible(false);
    m_avatar->setObjectName("avatar");

    m_text=new QLabel(this);
    m_text->setObjectName("text");

    m_tailIcon=new RoundedImage(this);
    m_tailIcon->setDisableHover(true);
    m_tailIcon->setVisible(false);
    m_tailIcon->setObjectName("tailIcon");

    m_layout=Layout::horizontal(this);
    m_layout->addWidget(m_avatar);
    m_layout->addWidget(m_text);
    m_layout->addWidget(m_tailIcon);

    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

void AvatarButton::setHovered(bool enable)
{
    setProperty("hovered",enable);    
    m_text->setProperty("hovered",enable);
    Style::updateWidgetStyle(m_text);
    m_avatar->setParentHovered(enable);
    m_tailIcon->setParentHovered(enable);
    m_text->repaint();
}

//--------------------------------------------------------------------------

void AvatarButton::enterEvent(QEnterEvent* event)
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

void AvatarButton::leaveEvent(QEvent* event)
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

void AvatarButton::setParentHovered(bool enable)
{
    m_parentHovered=enable;
    setHovered(enable);
}

//--------------------------------------------------------------------------

void AvatarButton::setChecked(bool enable)
{
    if (!m_checkable)
    {
        return;
    }

    auto prevChecked=m_checked;

    m_checked=enable;
    setProperty("checked",enable);
    m_avatar->setSelected(enable);
    m_tailIcon->setSelected(enable);
    m_text->setProperty("checked",enable);
    Style::updateWidgetStyle(this);
    Style::updateWidgetStyle(m_text);
    Style::updateWidgetStyle(m_avatar);
    Style::updateWidgetStyle(m_tailIcon);

    if (prevChecked!=m_checked)
    {
        emit toggled(m_checked);
    }
}

//--------------------------------------------------------------------------

void AvatarButton::toggle()
{
    setChecked(!m_checked);
}

//--------------------------------------------------------------------------

void AvatarButton::click()
{
    emit clicked();
    toggle();
}

//--------------------------------------------------------------------------

QString AvatarButton::text() const
{
    return m_text->text();
}

//--------------------------------------------------------------------------

void AvatarButton::setText(const QString& text)
{
    m_text->setText(text);
}

//--------------------------------------------------------------------------

void AvatarButton::mousePressEvent(QMouseEvent* event)
{
    if (event->button()==Qt::LeftButton)
    {
        click();
    }
    QFrame::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void AvatarButton::setTailSvgIcon(std::shared_ptr<SvgIcon> icon)
{
    m_tailIcon->setVisible(static_cast<bool>(icon));
    m_tailIcon->setSvgIcon(std::move(icon));
}

//--------------------------------------------------------------------------

std::shared_ptr<SvgIcon> AvatarButton::tailSvgIcon() const
{
    return m_tailIcon->svgIcon();
}

//--------------------------------------------------------------------------

void AvatarButton::setAvatarSource(std::shared_ptr<AvatarSource> avatarSource)
{
    m_avatar->setAvatarSource(std::move(avatarSource));
}

//--------------------------------------------------------------------------

std::shared_ptr<AvatarSource> AvatarButton::avatarSource() const
{
    return m_avatar->avatarSource();
}

//--------------------------------------------------------------------------

void AvatarButton::setAvatarPath(WithPath path)
{
    m_avatar->setVisible(!path.empty());
    m_avatar->setAvatarPath(std::move(path));
}

//--------------------------------------------------------------------------

const WithPath& AvatarButton::avatarPath() const noexcept
{
    return m_avatar->avatarPath();
}
//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
