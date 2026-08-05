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

/** @file uise/desktop/src/datetimeinput.cpp
*
*  Defines DateTimePickerDropdown, DateTimeInput, DateInput, MonthInput and TimeInput.
*
*/

/****************************************************************************/

#include <QFrame>
#include <QKeyEvent>
#include <QMouseEvent>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/datetime.hpp>

#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>

#include <uise/desktop/datetimeinput.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

DateTimePickerDropdown::DateTimePickerDropdown(DateTimeFields fields, QWidget* parent)
    : DropdownFrame(parent)
{
    auto content=new QFrame(this);
    content->setObjectName(QStringLiteral("dateTimePickerDropdownContent"));
    auto l=Layout::vertical(content);

    m_picker=new DateTimePicker(fields,content);
    l->addWidget(m_picker);

    m_buttonsFrame=new QFrame(content);
    m_buttonsFrame->setObjectName(QStringLiteral("buttonsFrame"));
    auto bl=Layout::horizontal(m_buttonsFrame);
    bl->addStretch(1);

    m_cancelButton=new PushButton(Style::instance().svgIconLocator().icon("DateTimePicker::cancel",this),m_buttonsFrame);
    m_cancelButton->setObjectName(QStringLiteral("cancelButton"));
    bl->addWidget(m_cancelButton);
    connect(m_cancelButton,&PushButton::clicked,this,[this]()
    {
        m_picker->setDateTime(m_snapshot);
        emit cancelled();
        closeDropdown();
    });

    m_applyButton=new PushButton(Style::instance().svgIconLocator().icon("DateTimePicker::apply",this),m_buttonsFrame);
    m_applyButton->setObjectName(QStringLiteral("applyButton"));
    bl->addWidget(m_applyButton);
    connect(m_applyButton,&PushButton::clicked,this,[this]()
    {
        emit applied();
        closeDropdown();
    });

    l->addWidget(m_buttonsFrame);
    m_buttonsFrame->setVisible(m_buttonsVisible);

    setContent(content);

    connect(this,&DropdownFrame::aboutToShow,this,[this]()
    {
        m_snapshot=m_picker->dateTime();
    });
}

//--------------------------------------------------------------------------

void DateTimePickerDropdown::setButtonsVisible(bool enable)
{
    m_buttonsVisible=enable;
    if (m_buttonsFrame!=nullptr)
    {
        m_buttonsFrame->setVisible(enable);
    }
}

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

DateTimeInput::DateTimeInput(QWidget* parent)
    : DateTimeInput(DateTimeField::DateTime,parent)
{}

//--------------------------------------------------------------------------

DateTimeInput::DateTimeInput(DateTimeFields fields, QWidget* parent)
    : LineEdit(parent)
{
    construct(fields);
}

//--------------------------------------------------------------------------

DateTimeInput::~DateTimeInput()
{
    destroyWidget(m_dropdown);
}

//--------------------------------------------------------------------------

void DateTimeInput::construct(DateTimeFields fields)
{
    setReadOnly(true);
    setCursor(Qt::ArrowCursor);

    std::shared_ptr<SvgIcon> icon;
    if (fields.testFlag(DateTimeField::Day) && fields.testFlag(DateTimeField::Hour))
    {
        icon=Style::instance().svgIconLocator().icon("DateTimePicker::calendarTime",this);
    }
    else if (fields.testFlag(DateTimeField::Hour))
    {
        icon=Style::instance().svgIconLocator().icon("DateTimePicker::clock",this);
    }
    else if (fields.testFlag(DateTimeField::Day))
    {
        icon=Style::instance().svgIconLocator().icon("DateTimePicker::calendar",this);
    }
    else
    {
        icon=Style::instance().svgIconLocator().icon("DateTimePicker::calendarMonth",this);
    }

    m_pickerButton=addPushButton(std::move(icon),QLineEdit::TrailingPosition);
    m_pickerButton->setObjectName(QStringLiteral("pickerButton"));

    // no parent: DropdownFrame is always a top-level window regardless (see its docs) --
    // lifetime is managed explicitly via destroyWidget() in the destructor, matching the
    // FastSwitchButtonDropdown precedent
    m_dropdown=new DateTimePickerDropdown(fields);
    m_dropdown->setTriggerWidget(this);

    connect(m_pickerButton,&PushButton::clicked,this,[this]()
    {
        m_dropdown->toggleBelow(this);
    });

    connect(m_dropdown->picker(),&DateTimePicker::dateTimeChanged,this,[this](const QDateTime& v)
    {
        updateText();
        emit dateTimeChanged(v);
    });
    connect(m_dropdown->picker(),&DateTimePicker::dateChanged,this,&DateTimeInput::dateChanged);
    connect(m_dropdown->picker(),&DateTimePicker::timeChanged,this,&DateTimeInput::timeChanged);

    updateText();
}

//--------------------------------------------------------------------------

QDateTime DateTimeInput::dateTime() const
{
    return m_dropdown->picker()->dateTime();
}

//--------------------------------------------------------------------------

QDate DateTimeInput::date() const
{
    return m_dropdown->picker()->date();
}

//--------------------------------------------------------------------------

QTime DateTimeInput::time() const
{
    return m_dropdown->picker()->time();
}

//--------------------------------------------------------------------------

void DateTimeInput::setDisplayFormat(const QString& format)
{
    m_displayFormat=format;
    updateText();
}

//--------------------------------------------------------------------------

QString DateTimeInput::displayFormat() const
{
    return m_displayFormat;
}

//--------------------------------------------------------------------------

DateTimePicker* DateTimeInput::picker() const noexcept
{
    return m_dropdown->picker();
}

//--------------------------------------------------------------------------

void DateTimeInput::setDateRange(const QDate& min, const QDate& max)
{
    m_dropdown->picker()->setDateRange(min,max);
}

//--------------------------------------------------------------------------

void DateTimeInput::setButtonsVisible(bool enable)
{
    m_dropdown->setButtonsVisible(enable);
}

//--------------------------------------------------------------------------

void DateTimeInput::setDateTime(const QDateTime& value)
{
    m_dropdown->picker()->setDateTime(value);
}

//--------------------------------------------------------------------------

void DateTimeInput::setDate(const QDate& value)
{
    m_dropdown->picker()->setDate(value);
}

//--------------------------------------------------------------------------

void DateTimeInput::setTime(const QTime& value)
{
    m_dropdown->picker()->setTime(value);
}

//--------------------------------------------------------------------------

void DateTimeInput::openPopup()
{
    m_dropdown->popupBelow(this);
}

//--------------------------------------------------------------------------

void DateTimeInput::closePopup()
{
    m_dropdown->closeDropdown();
}

//--------------------------------------------------------------------------

void DateTimeInput::updateText()
{
    auto picker=m_dropdown->picker();
    auto fields=picker->fields();

    if (!m_displayFormat.isEmpty())
    {
        setText(locale().toString(picker->dateTime(),m_displayFormat));
        return;
    }

    bool hasDay=fields.testFlag(DateTimeField::Day);
    bool hasYearMonth=fields.testFlag(DateTimeField::Year) && fields.testFlag(DateTimeField::Month);
    bool hasTime=fields.testFlag(DateTimeField::Hour);

    if (hasYearMonth && !hasDay && !hasTime)
    {
        setText(dateAsMonthAndYear(picker->date(),locale()));
        return;
    }

    QString fmt;
    if (hasDay)
    {
        fmt=locale().dateFormat(QLocale::ShortFormat);
    }
    if (hasTime)
    {
        if (!fmt.isEmpty())
        {
            fmt+=QStringLiteral(" ");
        }
        fmt+=locale().timeFormat(QLocale::ShortFormat);
    }
    if (fmt.isEmpty())
    {
        fmt=locale().dateFormat(QLocale::ShortFormat);
    }

    setText(locale().toString(picker->dateTime(),fmt));
}

//--------------------------------------------------------------------------

void DateTimeInput::mousePressEvent(QMouseEvent* event)
{
    LineEdit::mousePressEvent(event);
    m_dropdown->toggleBelow(this);
}

//--------------------------------------------------------------------------

void DateTimeInput::keyPressEvent(QKeyEvent* event)
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
//--------------------------------------------------------------------------

DateInput::DateInput(QWidget* parent)
    : DateTimeInput(DateTimeField::Date,parent)
{}

//--------------------------------------------------------------------------

MonthInput::MonthInput(QWidget* parent)
    : DateTimeInput(DateTimeField::YearMonth,parent)
{}

//--------------------------------------------------------------------------

TimeInput::TimeInput(QWidget* parent)
    : DateTimeInput(DateTimeField::Time,parent)
{}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
