/**
@copyright Evgeny Sidorov 2026

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/src/calendardialog.cpp
*
*  Defines CalendarDialog.
*
*/

/****************************************************************************/

#include <QFrame>
#include <QPointer>
#include <QBoxLayout>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QMetaObject>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/calendardialog.hpp>

// CalendarDialog is the only translation unit that instantiates Dialog<AbstractCalendarDialog>,
// so the template definition must be visible here -- see the identical comment in
// src/statusdialog.cpp for why.
#include <uise/desktop/ipp/dialog.ipp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

//--------------------------------------------------------------------------

class CalendarDialog_p
{
    public:

        Calendar* calendar=nullptr;
        QBoxLayout* contentLayout=nullptr;
        QPointer<QWidget> headerWidget;
        QFrame* buttonsFrame=nullptr;
        PushButton* applyButton=nullptr;
        SingleShotTimer* autoCloseTimer=nullptr;
        int autoCloseDelayMs=AbstractCalendarDialog::DefaultAutoCloseDelayMs;
        bool autoCloseEnabled=true;
        CalendarDialogCloseMode closeMode=CalendarDialogCloseMode::ExplicitButton;
};

//--------------------------------------------------------------------------

CalendarDialog::CalendarDialog(QWidget* parent)
    : CalendarDialog(CalendarMode::Activation,parent)
{}

//--------------------------------------------------------------------------

CalendarDialog::CalendarDialog(CalendarMode mode, QWidget* parent)
    : Base(parent),
      pimpl(std::make_unique<CalendarDialog_p>())
{
    constructCalendar(mode);
}

//--------------------------------------------------------------------------

CalendarDialog::~CalendarDialog()
{}

//--------------------------------------------------------------------------

void CalendarDialog::constructCalendar(CalendarMode mode)
{
    // Content mirrors CalendarDropdown::construct() (see calendarinput.cpp): the calendar with
    // a buttons row directly below it, holding a single icon-only Apply-styled button built the
    // exact same way (same PushButton(icon,parent) ctor, the same checkmark as "Calendar::apply" but in
    // its own icon context, "CalendarDialogApply", so its colours can differ from the calendar's other icons)
    // -- rather than routing through AbstractDialog's own generic button-row/ButtonConfig
    // system, which is never used here (setButtons({}) below, permanently). An X here would
    // duplicate the dialog's own title-bar close button; a checkmark reads as "done" instead,
    // and CalendarDialog has no snapshot/revert semantics to make a Cancel meaningful anyway --
    // this button, like every other way of closing, keeps whatever selection stands at the time.
    auto* content=new QFrame(this);
    content->setObjectName(QStringLiteral("calendarDialogContent"));
    auto* l=Layout::vertical(content);
    pimpl->contentLayout=l;

    pimpl->calendar=new Calendar(mode,content);
    l->addWidget(pimpl->calendar);

    pimpl->buttonsFrame=new QFrame(content);
    pimpl->buttonsFrame->setObjectName(QStringLiteral("buttonsFrame"));
    auto* bl=Layout::horizontal(pimpl->buttonsFrame);
    bl->addStretch(1);

    pimpl->applyButton=new PushButton(Style::instance().svgIconLocator().icon(QStringLiteral("CalendarDialogApply::apply"),this),pimpl->buttonsFrame);
    pimpl->applyButton->setObjectName(QStringLiteral("applyButton"));
    // A bare checkmark, never a platform push button: flat keeps the native style from drawing a
    // bezel (a bordered grey box) in any state, whatever the style sheet does or does not match. No
    // focus ring either: it is a click target, the dialog's own keys (Escape) do not go through it.
    pimpl->applyButton->qPushButton()->setFlat(true);
    pimpl->applyButton->qPushButton()->setFocusPolicy(Qt::NoFocus);
    bl->addWidget(pimpl->applyButton);
    connect(pimpl->applyButton,&PushButton::clicked,this,[this]()
    {
        // Before closeDialog(): the only close path that means "the selection is confirmed"
        emit applied();
        closeDialog();
    });

    l->addWidget(pimpl->buttonsFrame);

    setWidget(content);
    setTitle(tr("Select date"));
    setButtons({});
    applyCloseMode();

    pimpl->autoCloseTimer=new SingleShotTimer(this);

    connect(pimpl->calendar,&Calendar::activity,this,&CalendarDialog::notifyActivity);

    // AutoCloseOnSelection wiring -- connected unconditionally and gated on pimpl->closeMode at
    // the moment each signal fires, rather than (dis)connected from setCloseMode(), so a mode
    // switch takes effect on the very next selection with no connection bookkeeping. Mirrors
    // CalendarDropdown's own auto-close logic (see calendarinput.cpp) for the same three
    // "a selection just settled" signals; MultipleSelection has no such moment and is
    // deliberately left to the button row / title bar / Escape in both modes.
    connect(pimpl->calendar,&Calendar::dateActivated,this,[this](const QDate&)
    {
        if (pimpl->closeMode==CalendarDialogCloseMode::AutoCloseOnSelection)
        {
            closeDialog();
        }
    });
    connect(pimpl->calendar,&Calendar::selectionChanged,this,[this]()
    {
        if (pimpl->closeMode==CalendarDialogCloseMode::AutoCloseOnSelection
            && pimpl->calendar->effectiveMode()==CalendarMode::SingleSelection
            && pimpl->calendar->hasSelection())
        {
            closeDialog();
        }
    });
    connect(pimpl->calendar,&Calendar::rangeSelected,this,[this](const QDate&, const QDate&)
    {
        if (pimpl->closeMode==CalendarDialogCloseMode::AutoCloseOnSelection)
        {
            closeDialog();
        }
    });
}

//--------------------------------------------------------------------------

void CalendarDialog::applyCloseMode()
{
    pimpl->buttonsFrame->setVisible(pimpl->closeMode==CalendarDialogCloseMode::ExplicitButton);
}

//--------------------------------------------------------------------------

Calendar* CalendarDialog::calendar() const
{
    return pimpl->calendar;
}

//--------------------------------------------------------------------------

void CalendarDialog::setHeaderWidget(QWidget* widget)
{
    if (pimpl->headerWidget==widget)
    {
        return;
    }
    if (!pimpl->headerWidget.isNull())
    {
        pimpl->contentLayout->removeWidget(pimpl->headerWidget);
        delete pimpl->headerWidget.data();
    }
    pimpl->headerWidget=widget;
    if (widget!=nullptr)
    {
        widget->setParent(pimpl->contentLayout->parentWidget());
        pimpl->contentLayout->insertWidget(0,widget);
    }
}

//--------------------------------------------------------------------------

QWidget* CalendarDialog::headerWidget() const noexcept
{
    return pimpl->headerWidget.data();
}

//--------------------------------------------------------------------------

void CalendarDialog::setApplyRowRetainsSpace(bool enable)
{
    auto policy=pimpl->buttonsFrame->sizePolicy();
    policy.setRetainSizeWhenHidden(enable);
    pimpl->buttonsFrame->setSizePolicy(policy);
}

//--------------------------------------------------------------------------

void CalendarDialog::setApplyEnabled(bool enable)
{
    pimpl->applyButton->setEnabled(enable);
}

//--------------------------------------------------------------------------

bool CalendarDialog::isApplyEnabled() const noexcept
{
    return pimpl->applyButton->isEnabled();
}

//--------------------------------------------------------------------------

void CalendarDialog::setAutoCloseDelayMs(int ms)
{
    pimpl->autoCloseDelayMs=ms;
    armAutoClose();
}

//--------------------------------------------------------------------------

int CalendarDialog::autoCloseDelayMs() const noexcept
{
    return pimpl->autoCloseDelayMs;
}

//--------------------------------------------------------------------------

void CalendarDialog::setAutoCloseEnabled(bool enable)
{
    pimpl->autoCloseEnabled=enable;
    armAutoClose();
}

//--------------------------------------------------------------------------

bool CalendarDialog::isAutoCloseEnabled() const noexcept
{
    return pimpl->autoCloseEnabled;
}

//--------------------------------------------------------------------------

void CalendarDialog::setCloseMode(CalendarDialogCloseMode mode)
{
    if (pimpl->closeMode==mode)
    {
        return;
    }
    pimpl->closeMode=mode;
    applyCloseMode();
}

//--------------------------------------------------------------------------

CalendarDialogCloseMode CalendarDialog::closeMode() const noexcept
{
    return pimpl->closeMode;
}

//--------------------------------------------------------------------------

void CalendarDialog::notifyActivity()
{
    armAutoClose();
}

//--------------------------------------------------------------------------

void CalendarDialog::setDialogFocus()
{
    if (pimpl->calendar!=nullptr)
    {
        pimpl->calendar->setFocus();
    }
}

//--------------------------------------------------------------------------

bool CalendarDialog::isResizable() const
{
    return false;
}

//--------------------------------------------------------------------------

void CalendarDialog::armAutoClose()
{
    if (!pimpl->autoCloseEnabled || pimpl->autoCloseDelayMs<=0 || !isVisible())
    {
        pimpl->autoCloseTimer->cancel();
        return;
    }

    pimpl->autoCloseTimer->shot(
        static_cast<size_t>(pimpl->autoCloseDelayMs),
        [this](){ onAutoCloseTimeout(); },
        true    // restart -- this is what makes activity() reset the countdown rather than
                // only starting it once
    );
}

//--------------------------------------------------------------------------

void CalendarDialog::onAutoCloseTimeout()
{
    // Never close under an open popup or while hidden: re-arm a full delay instead of
    // suspending, the same stateless idiom as ImageViewerWidget::fadeControlsOut(). Immune to
    // a missed hidden() signal, e.g. a dropdown destroyed while still open.
    if (!isVisible() || (pimpl->calendar!=nullptr && pimpl->calendar->isPopupOpen()))
    {
        // Queued, not a direct armAutoClose() call: SingleShotTimer::shot() reassigns
        // m_handler, which would destroy the very closure currently executing if called
        // straight from here.
        QMetaObject::invokeMethod(this,[this](){ armAutoClose(); },Qt::QueuedConnection);
        return;
    }

    emit AbstractCalendarDialog::autoClosed();
    closeDialog();      // may lead to this dialog being deleteLater()'d -- nothing may run
                         // after this line
}

//--------------------------------------------------------------------------

void CalendarDialog::mousePressEvent(QMouseEvent* event)
{
    notifyActivity();
    Base::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void CalendarDialog::keyPressEvent(QKeyEvent* event)
{
    notifyActivity();
    Base::keyPressEvent(event);
}

//--------------------------------------------------------------------------

void CalendarDialog::showEvent(QShowEvent* event)
{
    Base::showEvent(event);
    armAutoClose();
}

//--------------------------------------------------------------------------

void CalendarDialog::hideEvent(QHideEvent* event)
{
    pimpl->autoCloseTimer->cancel();
    Base::hideEvent(event);
}

//--------------------------------------------------------------------------

// Explicit, exported instantiation, like every other Dialog<AbstractXxx> in this library. Without it
// the members of Dialog<AbstractCalendarDialog> are instantiated implicitly in this translation unit
// only and are not exported from the shared library, so a class OUTSIDE uise that derives from
// CalendarDialog (whitemdesktop's ChatCalendarDialog) cannot link: its vtable references every virtual
// of the base (setButtons, setTitle, setWidget, titleBar, ...).
template class UISE_DESKTOP_EXPORT Dialog<AbstractCalendarDialog>;

//--------------------------------------------------------------------------

}
