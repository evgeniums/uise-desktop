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
        emit cancelled();
        closeDropdown();
    });

    m_applyButton=new PushButton(Style::instance().svgIconLocator().icon(QStringLiteral("Calendar::apply"),this),m_buttonsFrame);
    m_applyButton->setObjectName(QStringLiteral("applyButton"));
    bl->addWidget(m_applyButton);
    connect(m_applyButton,&PushButton::clicked,this,[this]()
    {
        emit applied();
        closeDropdown();
    });

    l->addWidget(m_buttonsFrame);

    setContent(content);

    connect(this,&DropdownFrame::aboutToShow,this,[this]()
    {
        takeSnapshot();
        updateButtonsPolicy();
    });

    // Activation/SingleSelection: a click IS the whole answer, so close as soon as it lands --
    // but only while the buttons row is not showing (RangeSelection/MultipleSelection/Auto keep
    // an explicit Apply/Cancel and never auto-close on a mere click).
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
            && m_calendar->effectiveMode()==CalendarMode::SingleSelection
            && m_calendar->hasSelection())
        {
            emit applied();
            closeDropdown();
        }
    });

    updateButtonsPolicy();
}

//--------------------------------------------------------------------------

void CalendarDropdown::setAutoButtons(bool enable)
{
    m_autoButtons=enable;
    updateButtonsPolicy();
}

//--------------------------------------------------------------------------

void CalendarDropdown::setButtonsVisible(bool enable)
{
    m_autoButtons=false;
    m_buttonsVisible=enable;
    if (m_buttonsFrame!=nullptr)
    {
        m_buttonsFrame->setVisible(enable);
    }
}

//--------------------------------------------------------------------------

void CalendarDropdown::setAutoClose(bool enable) noexcept
{
    m_autoClose=enable;
}

//--------------------------------------------------------------------------

void CalendarDropdown::updateButtonsPolicy()
{
    if (m_autoButtons)
    {
        bool show=true;
        switch (m_calendar->mode())
        {
            case (CalendarMode::Activation):
            case (CalendarMode::SingleSelection):
                show=false;
                break;

            default:
                break;
        }
        m_buttonsVisible=show;
    }
    if (m_buttonsFrame!=nullptr)
    {
        m_buttonsFrame->setVisible(m_buttonsVisible);
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

    connect(m_dropdown->calendar(),&Calendar::dateActivated,this,[this](const QDate& d)
    {
        m_lastActivated=d;
        updateText();
        emit dateActivated(d);
    });
    connect(m_dropdown->calendar(),&Calendar::selectionChanged,this,[this]()
    {
        updateText();
        emit selectionChanged();
    });
    connect(m_dropdown->calendar(),&Calendar::rangeSelected,this,&CalendarInput::rangeSelected);
    connect(m_dropdown->calendar(),&Calendar::selectedDatesChanged,this,&CalendarInput::selectedDatesChanged);

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
