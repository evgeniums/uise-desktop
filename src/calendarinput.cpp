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

/** @file uise/desktop/src/calendarinput.cpp
*
*  Defines CalendarDropdown and CalendarInput.
*
*/

/****************************************************************************/

#include <QFrame>
#include <QKeyEvent>
#include <QMouseEvent>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>

#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>

#include <uise/desktop/calendarinput.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

CalendarDropdown::CalendarDropdown(QWidget* parent)
    : CalendarDropdown(CalendarMode::Activation,parent)
{}

//--------------------------------------------------------------------------

CalendarDropdown::CalendarDropdown(CalendarMode mode, QWidget* parent)
    : DropdownFrame(parent)
{
    construct(mode);
}

//--------------------------------------------------------------------------

void CalendarDropdown::construct(CalendarMode mode)
{
    auto content=new QFrame(this);
    content->setObjectName(QStringLiteral("calendarDropdownContent"));
    auto l=Layout::vertical(content);

    m_calendar=new Calendar(mode,content);
    l->addWidget(m_calendar);

    m_buttonsFrame=new QFrame(content);
    m_buttonsFrame->setObjectName(QStringLiteral("buttonsFrame"));
    auto bl=Layout::horizontal(m_buttonsFrame);
    bl->addStretch(1);

    m_cancelButton=new PushButton(Style::instance().svgIconLocator().icon(QStringLiteral("Calendar::cancel"),this),m_buttonsFrame);
    m_cancelButton->setObjectName(QStringLiteral("cancelButton"));
    bl->addWidget(m_cancelButton);
    connect(m_cancelButton,&PushButton::clicked,this,[this]()
    {
        restoreSnapshot();
        m_sessionCommitted=true;
        emit cancelled();
        closeDropdown();
    });

    m_applyButton=new PushButton(Style::instance().svgIconLocator().icon(QStringLiteral("Calendar::apply"),this),m_buttonsFrame);
    m_applyButton->setObjectName(QStringLiteral("applyButton"));
    bl->addWidget(m_applyButton);
    connect(m_applyButton,&PushButton::clicked,this,[this]()
    {
        m_sessionCommitted=true;
        emit applied();
        closeDropdown();
    });

    l->addWidget(m_buttonsFrame);

    setContent(content);

    connect(this,&DropdownFrame::aboutToShow,this,[this]()
    {
        takeSnapshot();
        m_sessionCommitted=false;
        updateButtonsPolicy();
    });

    // Explicit mode stages picks in the calendar until Apply commits them -- any OTHER way the
    // popup closes (Escape, an outside click, re-clicking the trigger, the host window changing,
    // or CalendarInput's own keyPressEvent() closing it directly) must discard those pending
    // picks exactly like Cancel does, or the next opening would silently adopt an abandoned edit
    // as its new baseline (see takeSnapshot() above, which just captures whatever the calendar
    // currently shows). aboutToHide() is the one hook that fires on every path to closed,
    // regardless of which of those triggered it or whether the owner called closeDropdown()
    // directly -- closeRequested() only covers the self-dismiss paths, missing direct
    // closeDropdown() callers such as CalendarInput's Escape handling. m_sessionCommitted, set by
    // the Apply/Cancel handlers above, distinguishes an already-resolved session (nothing to do)
    // from one abandoned some other way.
    connect(this,&DropdownFrame::aboutToHide,this,[this]()
    {
        if (m_buttonsFrame->isVisible() && !m_sessionCommitted)
        {
            restoreSnapshot();
            m_sessionCommitted=true;
            emit cancelled();
        }
    });

    // Activation/SingleSelection/RangeSelection are each terminal at some single, identifiable
    // click -- activation itself, the date picked, or the range's second endpoint -- so each
    // closes as soon as that click lands, but only while the buttons row is not showing
    // (Explicit-mode RangeSelection/Multiple/SingleSelection keep an explicit Apply/Cancel and
    // never auto-close on a mere click). MultipleSelection has no such terminal click -- every
    // click just toggles one more date in or out -- so it is deliberately not covered here; the
    // popup only closes on it via Apply or an explicit dismissal.
    //
    // The SingleSelection/RangeSelection checks below key on mode() (the pinned/configured mode),
    // not effectiveMode() -- ExtendedSelection's effectiveMode() also cycles through
    // SingleSelection and RangeSelection, but it is deliberately excluded here for the same
    // reason MultipleSelection is: the whole point of that mode is picking up more dates with
    // Ctrl/Shift right after a plain click or a completed range, so it must stay open exactly
    // like MultipleSelection does, regardless of which flavour effectiveMode() currently reports.
    connect(m_calendar,&Calendar::dateActivated,this,[this](const QDate&)
    {
        if (m_autoClose && !m_buttonsFrame->isVisible())
        {
            emit applied();
            closeDropdown();
        }
    });
    connect(m_calendar,&Calendar::selectionChanged,this,[this]()
    {
        if (m_autoClose && !m_buttonsFrame->isVisible()
            && m_calendar->mode()==CalendarMode::SingleSelection
            && m_calendar->hasSelection())
        {
            emit applied();
            closeDropdown();
        }
    });
    // rangeSelected(), unlike selectionChanged(), is only ever emitted once a range gesture
    // actually completes (its second endpoint click, or a finished drag) -- never on the first,
    // still-pending endpoint -- so it needs no extra hasSelection() check the way selectionChanged()
    // above does.
    connect(m_calendar,&Calendar::rangeSelected,this,[this](const QDate&, const QDate&)
    {
        if (m_autoClose && !m_buttonsFrame->isVisible()
            && m_calendar->mode()==CalendarMode::RangeSelection)
        {
            emit applied();
            closeDropdown();
        }
    });

    updateButtonsPolicy();
}

//--------------------------------------------------------------------------

void CalendarDropdown::setUpdateMode(CalendarUpdateMode mode)
{
    m_updateMode=mode;
    updateButtonsPolicy();
}

//--------------------------------------------------------------------------

void CalendarDropdown::setAutoClose(bool enable) noexcept
{
    m_autoClose=enable;
}

//--------------------------------------------------------------------------

void CalendarDropdown::updateButtonsPolicy()
{
    // CalendarMode::Activation has nothing an Apply step could stage -- a plain activation click
    // is already the whole answer -- so it always behaves like CalendarUpdateMode::Auto
    // regardless of what was configured.
    const bool show=m_updateMode==CalendarUpdateMode::Explicit
                     && m_calendar->mode()!=CalendarMode::Activation;
    if (m_buttonsFrame!=nullptr)
    {
        m_buttonsFrame->setVisible(show);
    }
}

//--------------------------------------------------------------------------

void CalendarDropdown::takeSnapshot()
{
    m_snapshotMonth=m_calendar->displayedMonth();
    m_snapshotSingle=m_calendar->selectedDate();
    m_snapshotFrom=m_calendar->rangeFrom();
    m_snapshotTo=m_calendar->rangeTo();
    m_snapshotDates=m_calendar->selectedDates();
}

//--------------------------------------------------------------------------

void CalendarDropdown::restoreSnapshot()
{
    m_calendar->setDisplayedMonth(m_snapshotMonth);
    m_calendar->setSelectedDate(m_snapshotSingle);
    m_calendar->setSelectedRange(m_snapshotFrom,m_snapshotTo);
    m_calendar->setSelectedDates(m_snapshotDates);
}

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

CalendarInput::CalendarInput(QWidget* parent)
    : CalendarInput(CalendarMode::Activation,parent)
{}

//--------------------------------------------------------------------------

CalendarInput::CalendarInput(CalendarMode mode, QWidget* parent)
    : LineEdit(parent)
{
    construct(mode);
}

//--------------------------------------------------------------------------

CalendarInput::~CalendarInput()
{
    destroyWidget(m_dropdown);
}

//--------------------------------------------------------------------------

void CalendarInput::construct(CalendarMode mode)
{
    setReadOnly(true);
    setCursor(Qt::ArrowCursor);

    m_pickerButton=addPushButton(Style::instance().svgIconLocator().icon(QStringLiteral("Calendar::input"),this),QLineEdit::TrailingPosition);
    m_pickerButton->setObjectName(QStringLiteral("pickerButton"));

    // no parent: DropdownFrame is always a top-level window regardless (see its docs) --
    // lifetime is managed explicitly via destroyWidget() in the destructor, matching the
    // DateTimeInput precedent
    m_dropdown=new CalendarDropdown(mode);
    m_dropdown->setTriggerWidget(this);

    connect(m_pickerButton,&PushButton::clicked,this,[this]()
    {
        m_dropdown->toggleBelow(this);
    });

    // Activation has no pending/committed distinction -- a click is already the final answer --
    // so it always lands in the input immediately, independent of updateMode().
    connect(m_dropdown->calendar(),&Calendar::dateActivated,this,[this](const QDate& d)
    {
        m_lastActivated=d;
        updateText();
        emit dateActivated(d);
    });
    // Calendar::selectionChanged is the umbrella signal for every other mode -- it always fires
    // alongside rangeSelected()/selectedDatesChanged() when either of those does (see
    // Calendar_p::emitSelectionChanged() callers), so this single connection is enough to catch
    // every kind of pick. In CalendarUpdateMode::Explicit those picks stay pending in the
    // calendar and must NOT reach the input yet -- commitFromCalendar() runs once instead, on
    // CalendarDropdown::applied(), below.
    connect(m_dropdown->calendar(),&Calendar::selectionChanged,this,[this]()
    {
        if (isLiveUpdating())
        {
            commitFromCalendar();
        }
    });
    connect(m_dropdown,&CalendarDropdown::applied,this,[this]()
    {
        if (!isLiveUpdating())
        {
            commitFromCalendar();
        }
    });

    connect(m_dropdown,&DropdownFrame::aboutToShow,this,[this]()
    {
        navigateToRelevantMonth();
    });

    updatePlaceholder();
    updateText();
}

//--------------------------------------------------------------------------

Calendar* CalendarInput::calendar() const noexcept
{
    return m_dropdown->calendar();
}

//--------------------------------------------------------------------------

void CalendarInput::setMode(CalendarMode mode)
{
    m_dropdown->calendar()->setMode(mode);
    updatePlaceholder();
    updateText();
}

//--------------------------------------------------------------------------

CalendarMode CalendarInput::mode() const
{
    return m_dropdown->calendar()->mode();
}

//--------------------------------------------------------------------------

void CalendarInput::setUpdateMode(CalendarUpdateMode mode)
{
    m_dropdown->setUpdateMode(mode);
}

//--------------------------------------------------------------------------

CalendarUpdateMode CalendarInput::updateMode() const noexcept
{
    return m_dropdown->updateMode();
}

//--------------------------------------------------------------------------

void CalendarInput::setDateRange(const QDate& min, const QDate& max)
{
    m_dropdown->calendar()->setDateRange(min,max);
}

//--------------------------------------------------------------------------

void CalendarInput::setDisplayFormat(const QString& format)
{
    m_displayFormat=format;
    updateText();
}

//--------------------------------------------------------------------------

QString CalendarInput::displayFormat() const
{
    return m_displayFormat;
}

//--------------------------------------------------------------------------

QDate CalendarInput::selectedDate() const
{
    return m_dropdown->calendar()->selectedDate();
}

//--------------------------------------------------------------------------

QDate CalendarInput::rangeFrom() const
{
    return m_dropdown->calendar()->rangeFrom();
}

//--------------------------------------------------------------------------

QDate CalendarInput::rangeTo() const
{
    return m_dropdown->calendar()->rangeTo();
}

//--------------------------------------------------------------------------

QList<QDate> CalendarInput::selectedDates() const
{
    return m_dropdown->calendar()->selectedDates();
}

//--------------------------------------------------------------------------

void CalendarInput::setSelectedDate(const QDate& date)
{
    m_dropdown->calendar()->setSelectedDate(date);
}

//--------------------------------------------------------------------------

void CalendarInput::setSelectedRange(const QDate& from, const QDate& to)
{
    m_dropdown->calendar()->setSelectedRange(from,to);
}

//--------------------------------------------------------------------------

void CalendarInput::setSelectedDates(const QList<QDate>& dates)
{
    m_dropdown->calendar()->setSelectedDates(dates);
}

//--------------------------------------------------------------------------

void CalendarInput::openPopup()
{
    m_dropdown->popupBelow(this);
}

//--------------------------------------------------------------------------

void CalendarInput::closePopup()
{
    m_dropdown->closeDropdown();
}

//--------------------------------------------------------------------------

void CalendarInput::updatePlaceholder()
{
    switch (m_dropdown->calendar()->mode())
    {
        case (CalendarMode::Activation):
        case (CalendarMode::SingleSelection):
            setPlaceholderText(tr("Select date"));
            break;

        default:
            setPlaceholderText(tr("Select dates"));
            break;
    }
}

//--------------------------------------------------------------------------

bool CalendarInput::isLiveUpdating() const
{
    return m_dropdown->updateMode()==CalendarUpdateMode::Auto
           || m_dropdown->calendar()->mode()==CalendarMode::Activation;
}

//--------------------------------------------------------------------------

void CalendarInput::commitFromCalendar()
{
    updateText();
    emit selectionChanged();

    auto cal=m_dropdown->calendar();
    switch (cal->effectiveMode())
    {
        case (CalendarMode::RangeSelection):
            if (cal->rangeFrom().isValid())
            {
                emit rangeSelected(cal->rangeFrom(),cal->rangeTo());
            }
            break;

        case (CalendarMode::MultipleSelection):
            emit selectedDatesChanged(cal->selectedDates());
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------

void CalendarInput::navigateToRelevantMonth()
{
    auto cal=m_dropdown->calendar();

    QDate target;
    switch (cal->effectiveMode())
    {
        case (CalendarMode::Activation):
            target=m_lastActivated;
            break;

        case (CalendarMode::SingleSelection):
            target=cal->selectedDate();
            break;

        case (CalendarMode::RangeSelection):
            target=cal->rangeTo().isValid() ? cal->rangeTo() : cal->rangeFrom();
            break;

        case (CalendarMode::MultipleSelection):
        {
            const auto dates=cal->selectedDates();
            if (!dates.isEmpty())
            {
                target=dates.last();
            }
        }
        break;

        default:
            break;
    }

    if (target.isValid())
    {
        cal->setDisplayedMonth(QDate{target.year(),target.month(),1});
    }
    else
    {
        cal->showToday();
    }
}

//--------------------------------------------------------------------------

void CalendarInput::updateText()
{
    auto cal=m_dropdown->calendar();

    auto fmt=[this](const QDate& d)
    {
        return m_displayFormat.isEmpty() ? locale().toString(d,QLocale::ShortFormat)
                                          : locale().toString(d,m_displayFormat);
    };

    QString text;
    switch (cal->effectiveMode())
    {
        case (CalendarMode::Activation):
            if (m_lastActivated.isValid())
            {
                text=fmt(m_lastActivated);
            }
            break;

        case (CalendarMode::SingleSelection):
            if (cal->selectedDate().isValid())
            {
                text=fmt(cal->selectedDate());
            }
            break;

        case (CalendarMode::RangeSelection):
            if (cal->rangeFrom().isValid())
            {
                text=(cal->rangeFrom()==cal->rangeTo())
                        ? fmt(cal->rangeFrom())
                        : QStringLiteral("%1 – %2").arg(fmt(cal->rangeFrom()),fmt(cal->rangeTo()));
            }
            break;

        case (CalendarMode::MultipleSelection):
        {
            QStringList parts;
            const auto dates=cal->selectedDates();
            const auto cap=cal->maxTitleDates();
            int n=0;
            for (auto&& d: dates)
            {
                if (cap>0 && n>=cap)
                {
                    parts.append(QStringLiteral("..."));
                    break;
                }
                parts.append(fmt(d));
                ++n;
            }
            text=parts.join(QStringLiteral(", "));
            break;
        }

        default:
            break;
    }

    setText(text);
    setToolTip(text);
}

//--------------------------------------------------------------------------

void CalendarInput::mousePressEvent(QMouseEvent* event)
{
    LineEdit::mousePressEvent(event);
    m_dropdown->toggleBelow(this);
}

//--------------------------------------------------------------------------

void CalendarInput::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
        case (Qt::Key_Down):
        case (Qt::Key_Space):
        case (Qt::Key_F4):
            openPopup();
            event->accept();
            return;

        case (Qt::Key_Escape):
            if (m_dropdown->isOpen())
            {
                closePopup();
                event->accept();
                return;
            }
            break;

        default:
            break;
    }

    LineEdit::keyPressEvent(event);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
