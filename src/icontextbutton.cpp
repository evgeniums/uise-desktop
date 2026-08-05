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
#include <uise/desktop/ripple.hpp>
#include <uise/desktop/icontextbutton.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

IconTextButton::IconTextButton(std::shared_ptr<SvgIcon> icon, QWidget* parent, IconPosition iconPosition)
    : QFrame(parent),
      m_iconPosition(iconPosition),
      m_layout(nullptr),
      m_icon(nullptr),
      m_trailingIcon(nullptr),
      m_text(nullptr),
      m_parentHovered(false),
      m_checked(false),
      m_checkable(false),
      m_ripple(nullptr)
{
    auto wrapper=new WithRoundedImage(this);
    wrapper->setObjectName("icon");

    m_icon=wrapper->image();
    m_icon->setDisableHover(true);

    auto trailingWrapper=new WithRoundedImage(this);
    trailingWrapper->setObjectName("trailingIcon");
    m_trailingIcon=trailingWrapper->image();
    m_trailingIcon->setDisableHover(true);
    trailingWrapper->setVisible(false);

    m_text=new QLabel(this);
    m_text->setObjectName("text");
    setIconPosition(iconPosition);
    setSvgIcon(std::move(icon));
    m_text->setVisible(false);

    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);

    // Covers the whole button, padding included -- not just the icon -- so it reads as a halo
    // around the icon (icon-only buttons) or a horizontal spread across the whole button (once
    // text is visible), see ripple.qss and the iconOnly property in setText() below. Installed
    // last so it ends up on top of the icon/text children above -- see RippleOverlay::install().
    // Auto-trigger stays on: the whole button is clickable, unlike CalendarDay which must gate
    // the ripple on isSelectable().
    m_ripple=RippleOverlay::install(this);

    // Default state before any setText() call is icon-only -- m_text is already empty and
    // hidden above.
    setProperty("iconOnly",true);
}

//--------------------------------------------------------------------------

void IconTextButton::setHovered(bool enable)
{
    setProperty("hovered",enable);
    m_text->setProperty("hovered",enable);
    // Without repolishing `this` too, a QSS rule keyed on this widget's own [hovered="true"]
    // (e.g. a background-color on the button itself, as opposed to on #text) never takes
    // effect: a dynamic property change alone does not invalidate Qt's cached style
    // evaluation for the widget it was set on -- setChecked() below already gets this right.
    Style::updateWidgetStyle(this);
    Style::updateWidgetStyle(m_text);
    m_icon->setParentHovered(enable);
    m_trailingIcon->setParentHovered(enable);
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

void IconTextButton::setCheckable(bool enable) noexcept
{
    m_checkable=enable;

    // A checkable button already gives persistent feedback via its own checked state (see
    // setChecked()'s background/icon/text styling) -- a transient ripple on top of that reads
    // as noisy rather than helpful, so it is switched off for as long as the button stays
    // checkable. setRippleEnabled(false) also cancels any ripple currently in flight.
    m_ripple->setRippleEnabled(!enable);
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
    m_trailingIcon->setSelected(enable);
    m_text->setProperty("checked",enable);
    Style::updateWidgetStyle(this);
    Style::updateWidgetStyle(m_text);
    Style::updateWidgetStyle(m_icon);
    Style::updateWidgetStyle(m_trailingIcon);

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
    m_text->setVisible(!text.isEmpty());

    // Drives ripple.qss's choice between a centred halo (icon-only) and a horizontal spread
    // (text visible) -- see uise--IconTextButton[iconOnly="..."] uise--RippleOverlay there.
    setProperty("iconOnly",text.isEmpty());
    Style::updateWidgetStyle(this);
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

void IconTextButton::setTrailingSvgIcon(std::shared_ptr<SvgIcon> icon)
{
    m_trailingIcon->setSvgIcon(icon);
    m_trailingIcon->parentWidget()->setVisible(icon != nullptr);
}

//--------------------------------------------------------------------------

std::shared_ptr<SvgIcon> IconTextButton::trailingSvgIcon() const
{
    return m_trailingIcon->svgIcon();
}

//--------------------------------------------------------------------------

void IconTextButton::setIconPosition(IconPosition iconPosition)
{
    if (m_layout)
    {
        m_layout->removeWidget(m_icon->parentWidget());
        m_layout->removeWidget(m_text);
        m_layout->removeWidget(m_trailingIcon->parentWidget());
    }

    m_iconPosition=iconPosition;
    m_icon->setVisible(true);

    // trailing icon visibility is preserved across layout rebuilds
    bool trailingVisible=m_trailingIcon->parentWidget()->isVisible();

    switch (m_iconPosition)
    {
        case (IconPosition::BeforeText):
        {
            m_layout=Layout::horizontal(this);
            m_layout->addWidget(m_icon->parentWidget());
            m_layout->addWidget(m_text);
            m_layout->addWidget(m_trailingIcon->parentWidget());
            m_text->setProperty("position","after");
        }
        break;

        case IconPosition::AfterText:
        {
            m_layout=Layout::horizontal(this);
            m_layout->addWidget(m_text);
            m_layout->addWidget(m_icon->parentWidget());
            m_layout->addWidget(m_trailingIcon->parentWidget());
            m_text->setProperty("position","before");
        }
        break;

        case IconPosition::AboveText:
        {
            m_layout=Layout::vertical(this);
            m_layout->addWidget(m_icon->parentWidget(),0,Qt::AlignCenter);
            m_layout->addWidget(m_text,0,Qt::AlignCenter);
            m_layout->addWidget(m_trailingIcon->parentWidget(),0,Qt::AlignCenter);
            m_text->setProperty("position","below");
        }
        break;

        case IconPosition::BelowText:
        {
            m_layout=Layout::vertical(this);
            m_layout->addWidget(m_text,0,Qt::AlignCenter);
            m_layout->addWidget(m_icon->parentWidget(),0,Qt::AlignCenter);
            m_layout->addWidget(m_trailingIcon->parentWidget(),0,Qt::AlignCenter);
            m_text->setProperty("position","above");
        }
        break;

        case IconPosition::Invisible:
        {
            m_layout=Layout::horizontal(this);
            m_layout->addWidget(m_text,0,Qt::AlignCenter);
            m_layout->addWidget(m_icon->parentWidget(),0,Qt::AlignCenter);
            m_layout->addWidget(m_trailingIcon->parentWidget(),0,Qt::AlignCenter);
            m_text->setProperty("position",QVariant{});
            m_icon->setVisible(false);
        }
        break;
    }

    m_trailingIcon->parentWidget()->setVisible(trailingVisible);
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
