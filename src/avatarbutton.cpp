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
#include <uise/desktop/ripple.hpp>
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

    // Default state before any setAvatarOnly() call is text-visible -- m_avatar is already
    // hidden above and m_text is unhidden. Must be set BEFORE RippleOverlay::install() below --
    // same ordering requirement as IconTextButton's iconOnly (see its constructor's comment):
    // install() polishes the overlay immediately, resolving ripple.qss's [avatarOnly="..."]
    // rules against whatever this property is at that instant, and nothing later repolishes the
    // overlay child on its own.
    setProperty("avatarOnly",false);

    // Covers the whole button, padding included -- same reasoning as IconTextButton's ripple
    // (see icontextbutton.cpp): a halo around the avatar in avatar-only mode, a horizontal
    // spread across the whole button once text is visible, see ripple.qss and the avatarOnly
    // property in setAvatarOnly() below. Installed last so it ends up on top of the avatar/
    // text/tailIcon children above.
    m_ripple=RippleOverlay::install(this);
}

//--------------------------------------------------------------------------

AvatarButton::AvatarButton(std::shared_ptr<SvgIcon> icon, QWidget* parent)
    : AvatarButton(parent)
{
    setAvatarOnly(static_cast<bool>(icon));
    m_avatar->setSvgIcon(std::move(icon));
}

//--------------------------------------------------------------------------

void AvatarButton::setAvatarOnly(bool enable)
{
    m_avatarOnly=enable;
    m_avatar->setVisible(enable);
    m_text->setVisible(!enable);

    // Drives ripple.qss's choice between a centred halo (avatar-only) and a horizontal spread
    // (text visible) -- see uise--AvatarButton[avatarOnly="..."] uise--RippleOverlay there.
    setProperty("avatarOnly",enable);
    Style::updateWidgetStyle(this);
    // A dynamic property set on `this` never cascades a repolish to the overlay CHILD on its
    // own (see the constructor's comment on install() ordering) -- without this, the two-arg
    // AvatarButton(icon, parent) constructor's setAvatarOnly() call (which runs AFTER install())
    // would never actually switch the ripple to the avatar-only capsule.
    if (m_ripple)
    {
        Style::updateWidgetStyle(m_ripple);
    }
}

//--------------------------------------------------------------------------

void AvatarButton::setHovered(bool enable)
{
    setProperty("hovered",enable);
    m_text->setProperty("hovered",enable);
    // Without repolishing `this` too, a QSS rule keyed on this widget's own [hovered="true"]
    // (e.g. a background-color on the button itself, as opposed to on #text) never takes
    // effect: a dynamic property change alone does not invalidate Qt's cached style
    // evaluation for the widget it was set on.
    Style::updateWidgetStyle(this);
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

void AvatarButton::setCheckable(bool enable) noexcept
{
    m_checkable=enable;

    // The ripple stays on regardless of checkable -- same reasoning as
    // IconTextButton::setCheckable(): activation happens on mouseReleaseEvent, so the ripple is
    // the only press-down feedback the button gives; the checked state is a separate, persistent
    // signal that only appears once the click completes.
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
    // Matches QAbstractButton/CalendarDay: a press only marks the button down, it does not
    // fire the click by itself -- that lets a press dragged out of the button before release
    // cancel it, same as every other button in this library (PushButton wraps a real
    // QAbstractButton, which already has this behaviour natively; CalendarDay has its own hand
    // -rolled version, see calendar.cpp).
    if (event->button()==Qt::LeftButton)
    {
        m_pressed=true;
    }
    QFrame::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void AvatarButton::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button()==Qt::LeftButton && m_pressed)
    {
        m_pressed=false;
        if (rect().contains(event->pos()))
        {
            click();
        }
    }
    QFrame::mouseReleaseEvent(event);
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
