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

/** @file uise/desktop/src/datetimepickersimple.cpp
*
*  Defines DateTimePickerSimple.
*
*/

/****************************************************************************/

#include <vector>

#include <QLabel>
#include <QLineEdit>
#include <QLocale>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/spinner.hpp>
#include <uise/desktop/spinnersection.hpp>
#include <uise/desktop/datetimepickersimple.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

DateTimePickerSimple::DateTimePickerSimple(QWidget* parent)
    : DateTimePickerSimple(false,parent)
{
}

//--------------------------------------------------------------------------

DateTimePickerSimple::DateTimePickerSimple(bool withSeparators, QWidget* parent)
    : QFrame(parent),
      m_spinner(new Spinner(this)),
      m_loadedDayCount(0),
      m_minYear(1900),
      m_maxYear(2100)
{
    auto l=Layout::vertical(this);
    l->addWidget(m_spinner);

    // derive item geometry from the current font instead of hardcoded pixel sizes: row height
    // from the font height plus vertical padding, column widths from the widest text each
    // column can show plus horizontal padding
    const auto fm=fontMetrics();
    const int itemHPadding=10;
    const int itemVPadding=6;
    const int itemHeight=qMax(30,fm.height()+2*itemVPadding);

    auto today=QDate::currentDate();
    QLocale locale;

    // -- month section --
    auto monthSection=std::make_shared<SpinnerSection>();
    int monthTextWidth=0;
    for (int m=1;m<=12;++m)
    {
        monthTextWidth=qMax(monthTextWidth,fm.horizontalAdvance(locale.standaloneMonthName(m,QLocale::LongFormat)));
    }
    monthSection->setItemsWidth(monthTextWidth+2*itemHPadding);
    monthSection->setCircular(true);
    QList<QWidget*> monthItems;
    for (int m=1;m<=12;++m)
    {
        auto item=new QLabel(locale.standaloneMonthName(m,QLocale::LongFormat));
        item->setAlignment(Qt::AlignCenter);
        item->setFixedWidth(monthSection->itemsWidth());
        item->setFixedHeight(itemHeight);
        monthItems.append(item);
        m_monthLabels.append(item);
    }
    monthSection->setItems(monthItems);

    // -- day section -- build the full pool of 31 labels, but only load as many as the
    // starting month actually has; the rest stay in m_dayLabels for adjustDayCount() to draw
    // on later via Spinner::appendItems()
    auto daySection=std::make_shared<SpinnerSection>();
    daySection->setItemsWidth(fm.horizontalAdvance("00")+2*itemHPadding);
    daySection->setCircular(true);
    for (int d=1;d<=31;++d)
    {
        auto item=new QLabel(QString::number(d));
        item->setAlignment(Qt::AlignCenter);
        item->setFixedWidth(daySection->itemsWidth());
        item->setFixedHeight(itemHeight);
        m_dayLabels.append(item);
    }
    m_loadedDayCount=today.daysInMonth();
    QList<QWidget*> dayItems;
    for (int d=0;d<m_loadedDayCount;++d)
    {
        dayItems.append(m_dayLabels.at(d));
    }
    daySection->setItems(dayItems);

    // -- year section --
    auto yearSection=std::make_shared<SpinnerSection>();
    yearSection->setItemsWidth(fm.horizontalAdvance("0000")+2*itemHPadding);
    QList<QWidget*> yearItems;
    for (int y=m_minYear;y<=m_maxYear;++y)
    {
        auto item=new QLabel(QString::number(y));
        item->setAlignment(Qt::AlignCenter);
        item->setFixedWidth(yearSection->itemsWidth());
        item->setFixedHeight(itemHeight);
        yearItems.append(item);
        m_yearLabels.append(item);
    }
    yearSection->setItems(yearItems);

    // -- optional separators between columns (construction-time only, see header docs) --
    if (withSeparators)
    {
        // use the locale's date separator: first character of the short date format that is
        // not a format letter, space or quote (e.g. '/' for en_US, '.' for de_DE)
        QString sepText("/");
        const auto shortFormat=locale.dateFormat(QLocale::ShortFormat);
        for (const auto& ch:shortFormat)
        {
            if (!ch.isLetter() && !ch.isSpace() && ch!=QChar('\''))
            {
                sepText=ch;
                break;
            }
        }

        const int sepWidth=fm.horizontalAdvance(sepText)+2*4;
        auto makeSeparator=[&sepText,sepWidth,itemHeight]()
        {
            auto sep=new QLabel(sepText);
            sep->setAlignment(Qt::AlignCenter);
            sep->setFixedWidth(sepWidth);
            sep->setFixedHeight(itemHeight);
            return sep;
        };

        monthSection->setRightBarLabel(makeSeparator());
        monthSection->setRightBarWidth(sepWidth);
        daySection->setRightBarLabel(makeSeparator());
        daySection->setRightBarWidth(sepWidth);
    }

    std::vector<std::shared_ptr<SpinnerSection>> sections{monthSection,daySection,yearSection};
    m_spinner->setItemHeight(itemHeight);
    m_spinner->setSections(sections);

    m_spinner->selectItem(0,today.month()-1);
    m_spinner->selectItem(1,today.day()-1);
    m_spinner->selectItem(2,today.year()-m_minYear);
    m_date=today;

    m_spinner->setFixedSize(monthSection->width()+daySection->width()+yearSection->width()+10,itemHeight*5);

    auto styleSample=new QLineEdit();
    m_spinner->setStyleSample(styleSample);

    connect(m_spinner,&Spinner::itemChanged,this,[this](int sectionIndex,int itemIndex)
    {
        onItemChanged(sectionIndex,itemIndex);
    });
}

//--------------------------------------------------------------------------

void DateTimePickerSimple::onItemChanged(int sectionIndex, int /*itemIndex*/)
{
    if (sectionIndex==0 || sectionIndex==2)
    {
        adjustDayCount();
    }
    updateDateFromSelection();
}

//--------------------------------------------------------------------------

void DateTimePickerSimple::adjustDayCount()
{
    int month=m_spinner->selectedItemIndex(0)+1;
    int year=m_minYear+m_spinner->selectedItemIndex(2);
    int days=QDate(year,month,1).daysInMonth();

    if (days==m_loadedDayCount)
    {
        return;
    }

    if (days>m_loadedDayCount)
    {
        QList<QWidget*> toAppend;
        for (int d=m_loadedDayCount;d<days;++d)
        {
            toAppend.append(m_dayLabels.at(d));
        }
        m_spinner->appendItems(1,toAppend);
    }
    else
    {
        m_spinner->removeLastItems(1,m_loadedDayCount-days);
    }

    m_loadedDayCount=days;
}

//--------------------------------------------------------------------------

void DateTimePickerSimple::updateDateFromSelection()
{
    int month=m_spinner->selectedItemIndex(0)+1;
    int day=m_spinner->selectedItemIndex(1)+1;
    int year=m_minYear+m_spinner->selectedItemIndex(2);

    int maxDay=QDate(year,month,1).daysInMonth();
    day=qBound(1,day,maxDay);

    QDate newDate(year,month,day);
    if (newDate!=m_date)
    {
        m_date=newDate;
        emit dateChanged(m_date);
    }
}

//--------------------------------------------------------------------------

void DateTimePickerSimple::setDate(const QDate& value)
{
    if (!value.isValid() || value==m_date)
    {
        return;
    }

    m_date=value;

    m_spinner->selectItem(0,value.month()-1);
    m_spinner->selectItem(2,qBound(0,value.year()-m_minYear,m_maxYear-m_minYear));

    // the day wheel must be resized for the TARGET month/year before selecting the day index,
    // otherwise selectItem() below can throw std::out_of_range if the current (pre-adjustment)
    // day count is smaller than value.day() -- e.g. switching from February to a 31-day month
    adjustDayCount();

    m_spinner->selectItem(1,value.day()-1);

    emit dateChanged(m_date);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
