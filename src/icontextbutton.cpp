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
#include <uise/desktop/utils/destroywidget.hpp>
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

    // Default state before any setText() call is icon-only -- m_text is already empty and
    // hidden above. Must be set BEFORE RippleOverlay::install() below: install() polishes the
    // overlay immediately (see its own doc comment), resolving every qproperty-* the ripple QSS
    // keys on -- including the [iconOnly="..."] rules in ripple.qss -- against whatever dynamic
    // properties `this` carries at that exact moment. Setting it after install() would leave the
    // overlay permanently polished against the property's absence, since Qt only polishes a
    // widget once and nothing here ever repolishes the overlay child on its own.
    setProperty("iconOnly",true);

    // Covers the whole button, padding included -- not just the icon -- so it reads as a halo
    // around the icon (icon-only buttons) or a horizontal spread across the whole button (once
    // text is visible), see ripple.qss and the iconOnly property in setText() below. Installed
    // last so it ends up on top of the icon/text children above -- see RippleOverlay::install().
    // Auto-trigger stays on: the whole button is clickable, unlike CalendarDay which must gate
    // the ripple on isSelectable().
    m_ripple=RippleOverlay::install(this);
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

    // The ripple stays on regardless of checkable: activation happens on mouseReleaseEvent (see
    // below), so the ripple is the only press-down feedback the button gives -- the checked
    // state is a separate, persistent signal that only appears once the click completes.
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
    // A dynamic property set on `this` never cascades a repolish to the overlay CHILD on its
    // own (see the constructor's comment on install() ordering) -- without this, a button that
    // starts with text and later has it cleared (or vice versa) would keep whichever ripple
    // shape matched its FIRST iconOnly value forever.
    if (m_ripple)
    {
        Style::updateWidgetStyle(m_ripple);
    }
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

void IconTextButton::setLeadingWidget(QWidget* widget)
{
    if (m_leadingWidget==widget)
    {
        // Idempotent on purpose: HTreeTab_p::reconstructLastNode() re-pushes the node's stored
        // widget into the surviving item on every reconstruct, and the destroy branch below
        // would otherwise delete a widget the caller is still handing us.
        return;
    }
    if (!m_leadingWidget.isNull())
    {
        destroyWidget(m_leadingWidget);
    }
    m_leadingWidget=widget;
    if (widget)
    {
        // QWidget::setParent() hides the widget as a side effect (QWidgetPrivate::setParent_sys()),
        // and re-adding it to the layout below can then queue an unwanted show -- capture the
        // caller's intended visibility now and re-assert it last, after both of those have run,
        // so a widget that is legitimately hidden at attach time (e.g. a control with nothing to
        // show yet) does not flash into view, and a visible one does not silently vanish.
        const bool wasHidden=widget->isHidden();
        widget->setParent(this);
        rebuildLayout();
        widget->setVisible(!wasHidden);
        return;
    }
    rebuildLayout();
}

//--------------------------------------------------------------------------

void IconTextButton::setTrailingWidget(QWidget* widget)
{
    if (m_trailingWidget==widget)
    {
        return;
    }
    if (!m_trailingWidget.isNull())
    {
        destroyWidget(m_trailingWidget);
    }
    m_trailingWidget=widget;
    if (widget)
    {
        const bool wasHidden=widget->isHidden();
        widget->setParent(this);
        rebuildLayout();
        widget->setVisible(!wasHidden);
        return;
    }
    rebuildLayout();
}

//--------------------------------------------------------------------------

void IconTextButton::setIconPosition(IconPosition iconPosition)
{
    m_iconPosition=iconPosition;
    rebuildLayout();
}

//--------------------------------------------------------------------------

void IconTextButton::rebuildLayout()
{
    if (m_layout)
    {
        m_layout->removeWidget(m_icon->parentWidget());
        m_layout->removeWidget(m_text);
        m_layout->removeWidget(m_trailingIcon->parentWidget());
        if (!m_leadingWidget.isNull())
        {
            m_layout->removeWidget(m_leadingWidget);
        }
        if (!m_trailingWidget.isNull())
        {
            m_layout->removeWidget(m_trailingWidget);
        }
    }

    m_icon->setVisible(true);

    // Visibility of everything this rebuild re-adds is preserved across it. !isHidden(), NOT
    // isVisible(): isVisible() is also false whenever THIS button is itself hidden -- which
    // NavigationBar::updateSingleItemVisibleMode() makes the normal state for most navbar items --
    // and restoring that false would turn an implicitly hidden child into an EXPLICITLY hidden
    // one, which the parent's later show() then refuses to bring back on its own.
    bool trailingVisible=!m_trailingIcon->parentWidget()->isHidden();
    const bool leadingWidgetVisible=!m_leadingWidget.isNull() && !m_leadingWidget->isHidden();
    const bool trailingWidgetVisible=!m_trailingWidget.isNull() && !m_trailingWidget->isHidden();

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

    // Leading/trailing widgets bracket the icon+text group in every IconPosition -- they are not
    // part of the icon-vs-text ordering the switch above decides, just first/last in whichever
    // direction m_layout now runs. Alignment mirrors each branch's own convention: no explicit
    // alignment for the two horizontal text-visible cases above (BeforeText/AfterText), AlignCenter
    // for the three that already center their children (AboveText/BelowText/Invisible).
    const bool centered=m_iconPosition==IconPosition::AboveText
                         || m_iconPosition==IconPosition::BelowText
                         || m_iconPosition==IconPosition::Invisible;
    if (!m_leadingWidget.isNull())
    {
        if (centered) m_layout->insertWidget(0,m_leadingWidget,0,Qt::AlignCenter);
        else          m_layout->insertWidget(0,m_leadingWidget);
    }
    if (!m_trailingWidget.isNull())
    {
        if (centered) m_layout->addWidget(m_trailingWidget,0,Qt::AlignCenter);
        else          m_layout->addWidget(m_trailingWidget);
    }

    m_trailingIcon->parentWidget()->setVisible(trailingVisible);
    if (!m_leadingWidget.isNull())
    {
        m_leadingWidget->setVisible(leadingWidgetVisible);
    }
    if (!m_trailingWidget.isNull())
    {
        m_trailingWidget->setVisible(trailingWidgetVisible);
    }

    // Drives ripple.qss's choice between the wide/flat ellipse tuned for horizontal rows
    // (BeforeText/AfterText) and the fuller fill needed for the much-closer-to-square
    // AboveText/BelowText buttons -- see uise--IconTextButton[verticalLayout="..."]
    // uise--RippleOverlay there.
    setProperty("verticalLayout",m_iconPosition==IconPosition::AboveText || m_iconPosition==IconPosition::BelowText);
    Style::updateWidgetStyle(this);
    // m_ripple is still null on the constructor's own call to setIconPosition() (it runs before
    // RippleOverlay::install() -- see the constructor) -- nothing to repolish yet there, the
    // overlay picks this property up when install() polishes it for the first time. Only a
    // later, runtime call to setIconPosition() needs this -- see setText()'s identical comment.
    if (m_ripple)
    {
        Style::updateWidgetStyle(m_ripple);
    }
}

//--------------------------------------------------------------------------

void IconTextButton::mousePressEvent(QMouseEvent* event)
{
    // Matches QAbstractButton/CalendarDay: a press only marks the button down, it does not
    // fire the click by itself -- that lets a press dragged out of the button before release
    // cancel it, same as every other button in this library (PushButton wraps a real
    // QAbstractButton, which already has this behaviour natively; CalendarDay has its own hand
    // -rolled version, see calendar.cpp).
    if (event->button()==Qt::LeftButton)
    {
        m_pressed=true;
        // Claim the press -- otherwise an unaccepted QMouseEvent auto-propagates to the
        // parent widget chain (Qt::WA_NoMousePropagation is off by default; DropdownFrame
        // relies on the same mechanism, see dropdownframe.cpp's own setAttribute() call).
        // Without this, a button embedded in a click-sensitive parent (e.g.
        // ChatMessageFileItem's own menuButton) leaks its press up to the parent's
        // mousePressEvent(), which then treats it as a click on the PARENT too.
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void IconTextButton::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button()==Qt::LeftButton && m_pressed)
    {
        m_pressed=false;
        if (rect().contains(event->pos()))
        {
            click();
        }
        // See mousePressEvent()'s identical comment -- the release must be claimed here too,
        // or it leaks to the parent right after this button already acted on it (opening its
        // own dropdown), double-firing whatever the parent's own release handler does.
        event->accept();
        return;
    }
    QFrame::mouseReleaseEvent(event);
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
