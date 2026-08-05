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

/** @file uise/desktop/src/datetimepicker.cpp
*
*  Defines DateTimePicker, DatePicker, MonthPicker and TimePicker.
*
*/

/****************************************************************************/

#include <algorithm>
#include <limits>

#include <QEvent>
#include <QShowEvent>
#include <QFontMetrics>
#include <QStyle>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>

#include <uise/desktop/spinner.hpp>
#include <uise/desktop/spinnersection.hpp>
#include <uise/desktop/label.hpp>
#include <uise/desktop/lineedit.hpp>

#include <uise/desktop/datetimepicker.hpp>
#include <uise/desktop/detail/datetimepicker_p.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

//--------------------------------------------------------------------------

QString tokenName(detail::Token token)
{
    switch (token)
    {
        case detail::Token::Year: return QStringLiteral("year");
        case detail::Token::Month: return QStringLiteral("month");
        case detail::Token::Day: return QStringLiteral("day");
        case detail::Token::Hour: return QStringLiteral("hour");
        case detail::Token::Minute: return QStringLiteral("minute");
        case detail::Token::Second: return QStringLiteral("second");
        case detail::Token::AmPm: return QStringLiteral("ampm");
    }
    return QString();
}

//--------------------------------------------------------------------------

void markOutOfRange(QWidget* widget, bool outOfRange)
{
    if (widget==nullptr)
    {
        return;
    }

    auto current=widget->property("outOfRange");
    if (current.isValid() && current.toBool()==outOfRange)
    {
        return;
    }

    widget->setProperty("outOfRange",outOfRange);
    if (widget->style()!=nullptr)
    {
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }
}

//--------------------------------------------------------------------------

/**
 * @brief Find the first run of a format character in a Qt date/time format string, skipping
 *  over single-quoted literal sections (Qt convention: '' inside a literal is a literal quote).
 * @param format Format string, e.g. "dd.MM.yyyy" or "h:mm AP".
 * @param ch Format character to look for, e.g. 'y', 'M', 'd', 'h', 'H', 'm', 's', 'A', 'a'.
 * @param endOut If not null, receives the offset one past the end of the matched run.
 * @return Offset of the first character of the run, or -1 if not found.
 */
int findFormatTokenRun(const QString& format, QChar ch, int* endOut=nullptr)
{
    bool inLiteral=false;
    int i=0;
    int n=format.size();
    while (i<n)
    {
        auto c=format.at(i);
        if (c==QChar('\''))
        {
            if (i+1<n && format.at(i+1)==QChar('\''))
            {
                i+=2;
                continue;
            }
            inLiteral=!inLiteral;
            ++i;
            continue;
        }
        if (!inLiteral && c==ch)
        {
            int start=i;
            while (i<n && format.at(i)==ch)
            {
                ++i;
            }
            if (endOut!=nullptr)
            {
                *endOut=i;
            }
            return start;
        }
        ++i;
    }
    return -1;
}

} // anonymous namespace

//--------------------------------------------------------------------------

DateTimePicker_p::DateTimePicker_p(DateTimePicker* picker)
    : q(picker)
{}

//--------------------------------------------------------------------------

bool DateTimePicker_p::is12Hour() const
{
    auto fmt=locale.timeFormat(QLocale::ShortFormat);
    int end=-1;
    if (findFormatTokenRun(fmt,QChar('A'),&end)>=0)
    {
        return true;
    }
    if (findFormatTokenRun(fmt,QChar('a'),&end)>=0)
    {
        return true;
    }
    return false;
}

//--------------------------------------------------------------------------

bool DateTimePicker_p::isCircular(detail::Token token) const
{
    switch (token)
    {
        case detail::Token::Year: return false;
        case detail::Token::Month: return circularFields.testFlag(DateTimeField::Month);
        case detail::Token::Day: return circularFields.testFlag(DateTimeField::Day);
        case detail::Token::Hour: return circularFields.testFlag(DateTimeField::Hour);
        case detail::Token::Minute: return circularFields.testFlag(DateTimeField::Minute);
        case detail::Token::Second: return circularFields.testFlag(DateTimeField::Second);
        case detail::Token::AmPm: return false;
    }
    return false;
}

//--------------------------------------------------------------------------

int DateTimePicker_p::dayColumnIndex() const
{
    for (size_t i=0;i<columns.size();++i)
    {
        if (columns[i].token==detail::Token::Day)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

//--------------------------------------------------------------------------

std::vector<detail::ColumnPlan> DateTimePicker_p::buildColumnPlan() const
{
    struct Offset
    {
        detail::Token token;
        int start=-1;
        int end=-1;
    };

    auto scanGroup=[](const QString& fmt, const std::vector<std::pair<detail::Token,QList<QChar>>>& wanted)
    {
        std::vector<Offset> offs;
        offs.reserve(wanted.size());
        for (auto&& w: wanted)
        {
            int end=-1;
            int start=-1;
            for (auto ch: w.second)
            {
                start=findFormatTokenRun(fmt,ch,&end);
                if (start>=0)
                {
                    break;
                }
            }
            offs.push_back(Offset{w.first,start,end});
        }
        std::stable_sort(offs.begin(),offs.end(),[](const Offset& a, const Offset& b)
        {
            auto ra=a.start<0 ? std::numeric_limits<int>::max() : a.start;
            auto rb=b.start<0 ? std::numeric_limits<int>::max() : b.start;
            return ra<rb;
        });

        std::vector<detail::ColumnPlan> plan;
        plan.reserve(offs.size());
        for (size_t i=0;i<offs.size();++i)
        {
            detail::ColumnPlan p;
            p.token=offs[i].token;
            if (i+1<offs.size() && offs[i].end>=0 && offs[i+1].start>offs[i].end)
            {
                p.rightBarText=fmt.mid(offs[i].end,offs[i+1].start-offs[i].end);
            }
            plan.push_back(p);
        }
        return plan;
    };

    std::vector<detail::ColumnPlan> plan;

    std::vector<std::pair<detail::Token,QList<QChar>>> dateWanted;
    if (fields.testFlag(DateTimeField::Year))
    {
        dateWanted.push_back({detail::Token::Year,{QChar('y')}});
    }
    if (fields.testFlag(DateTimeField::Month))
    {
        dateWanted.push_back({detail::Token::Month,{QChar('M')}});
    }
    if (fields.testFlag(DateTimeField::Day))
    {
        dateWanted.push_back({detail::Token::Day,{QChar('d')}});
    }
    if (!dateWanted.empty())
    {
        auto datePlan=scanGroup(locale.dateFormat(QLocale::ShortFormat),dateWanted);
        plan.insert(plan.end(),datePlan.begin(),datePlan.end());
    }

    std::vector<std::pair<detail::Token,QList<QChar>>> timeWanted;
    if (fields.testFlag(DateTimeField::Hour))
    {
        timeWanted.push_back({detail::Token::Hour,{QChar('h'),QChar('H')}});
    }
    if (fields.testFlag(DateTimeField::Minute))
    {
        timeWanted.push_back({detail::Token::Minute,{QChar('m')}});
    }
    if (fields.testFlag(DateTimeField::Second))
    {
        timeWanted.push_back({detail::Token::Second,{QChar('s')}});
    }
    if (fields.testFlag(DateTimeField::Hour) && is12Hour())
    {
        timeWanted.push_back({detail::Token::AmPm,{QChar('A'),QChar('a')}});
    }
    if (!timeWanted.empty())
    {
        auto timePlan=scanGroup(locale.timeFormat(QLocale::ShortFormat),timeWanted);
        plan.insert(plan.end(),timePlan.begin(),timePlan.end());
    }

    return plan;
}

//--------------------------------------------------------------------------

QStringList DateTimePicker_p::columnTextsFor(detail::Token token) const
{
    QStringList list;
    switch (token)
    {
        case (detail::Token::Year):
        {
            for (int y=minDate.year();y<=maxDate.year();++y)
            {
                list << QString::number(y);
            }
            break;
        }
        case (detail::Token::Month):
        {
            for (int m=1;m<=12;++m)
            {
                switch (monthFormat)
                {
                    case (DateTimePicker::MonthFormat::LongName):
                        list << locale.standaloneMonthName(m,QLocale::LongFormat);
                        break;
                    case (DateTimePicker::MonthFormat::ShortName):
                        list << locale.standaloneMonthName(m,QLocale::ShortFormat);
                        break;
                    case (DateTimePicker::MonthFormat::Number):
                        list << QString::number(m);
                        break;
                }
            }
            break;
        }
        case (detail::Token::Day):
        {
            auto d=value.date();
            int year=d.isValid() ? d.year() : QDate::currentDate().year();
            int month=d.isValid() ? d.month() : QDate::currentDate().month();
            int days=QDate(year,month,1).daysInMonth();
            for (int day=1;day<=days;++day)
            {
                list << QString::number(day);
            }
            break;
        }
        case (detail::Token::Hour):
        {
            if (is12Hour())
            {
                for (int h=1;h<=12;++h)
                {
                    list << QString::number(h);
                }
            }
            else
            {
                for (int h=0;h<24;++h)
                {
                    list << QStringLiteral("%1").arg(h,2,10,QChar('0'));
                }
            }
            break;
        }
        case (detail::Token::Minute):
        {
            for (int m=0;m<60;m+=qMax(1,minuteStep))
            {
                list << QStringLiteral("%1").arg(m,2,10,QChar('0'));
            }
            break;
        }
        case (detail::Token::Second):
        {
            for (int s=0;s<60;s+=qMax(1,secondStep))
            {
                list << QStringLiteral("%1").arg(s,2,10,QChar('0'));
            }
            break;
        }
        case (detail::Token::AmPm):
        {
            list << locale.amText() << locale.pmText();
            break;
        }
    }
    return list;
}

//--------------------------------------------------------------------------

int DateTimePicker_p::indexForValue(detail::Token token, const QDateTime& dt) const
{
    switch (token)
    {
        case (detail::Token::Year):
            return qBound(0,dt.date().year()-minDate.year(),qMax(0,maxDate.year()-minDate.year()));
        case (detail::Token::Month):
            return dt.date().month()-1;
        case (detail::Token::Day):
            return dt.date().day()-1;
        case (detail::Token::Hour):
        {
            int h=dt.time().hour();
            if (is12Hour())
            {
                int h12=h%12;
                if (h12==0)
                {
                    h12=12;
                }
                return h12-1;
            }
            return h;
        }
        case (detail::Token::Minute):
            return dt.time().minute()/qMax(1,minuteStep);
        case (detail::Token::Second):
            return dt.time().second()/qMax(1,secondStep);
        case (detail::Token::AmPm):
            return dt.time().hour()>=12 ? 1 : 0;
    }
    return 0;
}

//--------------------------------------------------------------------------

int DateTimePicker_p::measureColumnWidth(detail::Token token, const QStringList& texts, const QFontMetrics& fm) const
{
    if (itemWidthProp>0)
    {
        return itemWidthProp;
    }

    switch (token)
    {
        case (detail::Token::Year):
            return fm.horizontalAdvance(QStringLiteral("0000"))+2*itemHPadding;
        case (detail::Token::Day):
        case (detail::Token::Hour):
        case (detail::Token::Minute):
        case (detail::Token::Second):
            return fm.horizontalAdvance(QStringLiteral("00"))+2*itemHPadding;
        case (detail::Token::Month):
        case (detail::Token::AmPm):
        {
            int maxW=0;
            for (auto&& t: texts)
            {
                maxW=qMax(maxW,fm.horizontalAdvance(t));
            }
            return maxW+2*itemHPadding;
        }
    }
    return fm.horizontalAdvance(QStringLiteral("00"))+2*itemHPadding;
}

//--------------------------------------------------------------------------

QWidget* DateTimePicker_p::poolLabel(detail::PickerColumn& col, int index, const QString& text)
{
    if (index<col.pool.size())
    {
        auto widget=col.pool.at(index);
        widget->setFixedWidth(col.width);
        widget->setFixedHeight(effectiveItemHeight);
        widget->resize(col.width,effectiveItemHeight);
        return widget;
    }

    auto label=new Label(text,spinner);
    label->setAlignment(Qt::AlignCenter);
    label->setProperty("field",tokenName(col.token));
    label->setFixedWidth(col.width);
    label->setFixedHeight(effectiveItemHeight);
    // setFixed{Width,Height} only clamps min/max size -- it does not force an immediate
    // resize() for a widget that has never been shown or placed in a layout (these item
    // labels are painted via Spinner::paintEvent()'s render(), never shown themselves), so the
    // widget's actual rect() can stay at its constructor-time default size. Resize explicitly.
    label->resize(col.width,effectiveItemHeight);
    col.pool.append(label);
    return label;
}

//--------------------------------------------------------------------------

void DateTimePicker_p::setColumnTexts(int columnIndex, const QStringList& newTexts)
{
    auto& col=columns[static_cast<size_t>(columnIndex)];
    if (col.texts==newTexts)
    {
        return;
    }

    int minSize=qMin(col.texts.size(),newTexts.size());
    int commonPrefix=0;
    while (commonPrefix<minSize && col.texts.at(commonPrefix)==newTexts.at(commonPrefix))
    {
        ++commonPrefix;
    }

    if (commonPrefix==col.texts.size() && commonPrefix<newTexts.size())
    {
        // new list extends the old one -- append the tail, creating pool entries lazily
        QList<QWidget*> toAppend;
        for (int i=commonPrefix;i<newTexts.size();++i)
        {
            toAppend.append(poolLabel(col,i,newTexts.at(i)));
        }
        col.texts=newTexts;
        col.loaded=newTexts.size();
        spinner->appendItems(columnIndex,toAppend);
    }
    else if (commonPrefix==newTexts.size() && commonPrefix<col.texts.size())
    {
        // new list is a strict prefix of the old one -- drop the tail
        int removeCount=col.texts.size()-commonPrefix;
        col.texts=newTexts;
        col.loaded=newTexts.size();
        spinner->removeLastItems(columnIndex,removeCount);
    }
    else
    {
        // genuine content change: not reachable by any column generator this picker uses today
        // (year/day/hour/minute/second lists are always a growing or shrinking numeric run, and
        // month/AM-PM lists only ever change via a full rebuild()) -- kept as a safe fallback
        // that at least keeps bookkeeping consistent if that assumption is ever broken.
        col.texts=newTexts;
        col.loaded=newTexts.size();
    }
}

//--------------------------------------------------------------------------

QDateTime DateTimePicker_p::composeFromColumns() const
{
    auto d=value.date().isValid() ? value.date() : QDate::currentDate();
    int year=d.year();
    int month=d.month();
    int day=d.day();
    int hour=value.time().hour();
    int minute=value.time().minute();
    int second=value.time().second();
    bool pm=hour>=12;

    for (size_t i=0;i<columns.size();++i)
    {
        if (columns[i].loaded<=0)
        {
            continue;
        }
        auto idx=spinner->selectedItemIndex(static_cast<int>(i));
        idx=qBound(0,idx,columns[i].loaded-1);

        switch (columns[i].token)
        {
            case (detail::Token::Year):
                year=minDate.year()+idx;
                break;
            case (detail::Token::Month):
                month=idx+1;
                break;
            case (detail::Token::Day):
                day=idx+1;
                break;
            case (detail::Token::Hour):
                if (is12Hour())
                {
                    int h12=idx+1;
                    hour=(h12%12)+(pm?12:0);
                }
                else
                {
                    hour=idx;
                }
                break;
            case (detail::Token::Minute):
                minute=idx*minuteStep;
                break;
            case (detail::Token::Second):
                second=idx*secondStep;
                break;
            case (detail::Token::AmPm):
            {
                pm=(idx==1);
                if (fields.testFlag(DateTimeField::Hour))
                {
                    int h12=hour%12;
                    if (h12==0)
                    {
                        h12=12;
                    }
                    hour=(h12%12)+(pm?12:0);
                }
                break;
            }
        }
    }

    if (!fields.testFlag(DateTimeField::Day))
    {
        day=1;
    }
    int maxDay=QDate(year,month,1).daysInMonth();
    day=qBound(1,day,maxDay);

    return QDateTime(QDate(year,month,day),QTime(hour,minute,second));
}

//--------------------------------------------------------------------------

QDateTime DateTimePicker_p::clampToRange(const QDateTime& dt) const
{
    if (!fields.testFlag(DateTimeField::Year))
    {
        return dt;
    }

    QDate d=dt.date();
    QDate lo=minDate;
    QDate hi=maxDate;

    if (!fields.testFlag(DateTimeField::Day))
    {
        // month-granularity clamp: compare/clamp at the first day of the month so a range
        // starting mid-month (e.g. 2020-06-15) does not push a valid 2020-06-01 forward
        lo=QDate(minDate.year(),minDate.month(),1);
        hi=QDate(maxDate.year(),maxDate.month(),1);
        d=QDate(d.year(),d.month(),1);
    }

    if (d<lo)
    {
        d=lo;
    }
    else if (d>hi)
    {
        d=hi;
    }

    int maxDay=QDate(d.year(),d.month(),1).daysInMonth();
    int day=fields.testFlag(DateTimeField::Day) ? qBound(1,d.day(),maxDay) : 1;

    return QDateTime(QDate(d.year(),d.month(),day),dt.time());
}

//--------------------------------------------------------------------------

void DateTimePicker_p::updateOutOfRangeMarkers()
{
    if (columns.empty())
    {
        return;
    }

    int yearIdx=-1;
    int monthIdx=-1;
    int dayIdx=-1;
    for (size_t i=0;i<columns.size();++i)
    {
        if (columns[i].token==detail::Token::Year)
        {
            yearIdx=static_cast<int>(i);
        }
        else if (columns[i].token==detail::Token::Month)
        {
            monthIdx=static_cast<int>(i);
        }
        else if (columns[i].token==detail::Token::Day)
        {
            dayIdx=static_cast<int>(i);
        }
    }
    if (yearIdx<0)
    {
        return;
    }

    int year=value.date().year();
    bool atMin=(year==minDate.year());
    bool atMax=(year==maxDate.year());

    if (monthIdx>=0)
    {
        auto& col=columns[static_cast<size_t>(monthIdx)];
        for (int i=0;i<col.pool.size();++i)
        {
            bool outOfRange=(atMin && (i+1)<minDate.month()) || (atMax && (i+1)>maxDate.month());
            markOutOfRange(col.pool.at(i),outOfRange);
        }
    }

    if (dayIdx>=0)
    {
        auto& col=columns[static_cast<size_t>(dayIdx)];
        int month=value.date().month();
        for (int i=0;i<col.pool.size() && i<col.loaded;++i)
        {
            bool outOfRange=false;
            if (atMin && month==minDate.month() && (i+1)<minDate.day())
            {
                outOfRange=true;
            }
            if (atMax && month==maxDate.month() && (i+1)>maxDate.day())
            {
                outOfRange=true;
            }
            markOutOfRange(col.pool.at(i),outOfRange);
        }
    }
}

//--------------------------------------------------------------------------

void DateTimePicker_p::applyValueToColumns()
{
    if (spinner==nullptr || columns.empty())
    {
        return;
    }

    // resize the day column first so every other column's target index is guaranteed in range
    auto dayIdx=dayColumnIndex();
    if (dayIdx>=0)
    {
        setColumnTexts(dayIdx,columnTextsFor(detail::Token::Day));
    }

    for (size_t i=0;i<columns.size();++i)
    {
        if (columns[i].loaded<=0)
        {
            continue;
        }
        auto idx=indexForValue(columns[i].token,value);
        idx=qBound(0,idx,columns[i].loaded-1);
        if (spinner->selectedItemIndex(static_cast<int>(i))!=idx)
        {
            spinner->selectItem(static_cast<int>(i),idx);
        }
    }

    updateOutOfRangeMarkers();

    if (syncTimer!=nullptr)
    {
        // Defensive re-assertion pass: each freshly-built SpinnerSection settles its initial
        // index asynchronously via its own 0 ms timer (see Spinner::updateCurrentIndex()), and
        // a mid-drag selectItem() call can also leave currentItemIndex briefly stale. A short
        // positive delay (rather than 0 ms, which would race those per-section 0 ms timers with
        // no guaranteed ordering) guarantees this runs in a later event-loop pass, after
        // everything else has settled.
        syncTimer->shot(20,[this](){ verifyColumns(); });
    }
}

//--------------------------------------------------------------------------

void DateTimePicker_p::verifyColumns()
{
    if (spinner==nullptr)
    {
        return;
    }

    for (size_t i=0;i<columns.size();++i)
    {
        if (columns[i].loaded<=0)
        {
            continue;
        }
        auto idx=indexForValue(columns[i].token,value);
        idx=qBound(0,idx,columns[i].loaded-1);
        if (spinner->selectedItemIndex(static_cast<int>(i))!=idx)
        {
            spinner->selectItem(static_cast<int>(i),idx);
        }
    }
}

//--------------------------------------------------------------------------

void DateTimePicker_p::updateOnValueChanged(bool changed, const QDateTime& previous)
{
    if (!changed)
    {
        return;
    }

    if (value.date()!=previous.date())
    {
        emit q->dateChanged(value.date());
    }
    if (value.time()!=previous.time())
    {
        emit q->timeChanged(value.time());
    }
    emit q->dateTimeChanged(value);
}

//--------------------------------------------------------------------------

void DateTimePicker_p::setValue(const QDateTime& dt)
{
    auto clamped=clampToRange(dt);
    auto previous=value;
    bool changed=(clamped!=value);
    value=clamped;
    applyValueToColumns();
    updateOnValueChanged(changed,previous);
}

//--------------------------------------------------------------------------

void DateTimePicker_p::reconcile()
{
    auto composed=composeFromColumns();
    auto clamped=clampToRange(composed);
    auto previous=value;
    bool changed=(clamped!=value);
    value=clamped;
    applyValueToColumns();
    updateOnValueChanged(changed,previous);
}

//--------------------------------------------------------------------------

void DateTimePicker_p::onItemChanged(int /*sectionIndex*/, int /*itemIndex*/)
{
    if (rebuilding)
    {
        return;
    }
    reconcile();
}

//--------------------------------------------------------------------------

void DateTimePicker_p::rebuild()
{
    if (spinner==nullptr)
    {
        return;
    }

    rebuilding=true;

    auto plan=buildColumnPlan();

    columns.clear();
    columns.resize(plan.size());
    for (size_t i=0;i<plan.size();++i)
    {
        columns[i].token=plan[i].token;
        columns[i].rightBarText=plan[i].rightBarText;
        columns[i].circular=isCircular(columns[i].token);
        columns[i].texts=columnTextsFor(columns[i].token);
        columns[i].loaded=columns[i].texts.size();
    }

    q->ensurePolished();
    QFontMetrics fm(q->font());
    effectiveItemHeight = itemHeightProp>0 ? itemHeightProp : (fm.height()+2*itemVPadding);
    for (auto&& col: columns)
    {
        col.width=measureColumnWidth(col.token,col.texts,fm);
    }

    spinner->setItemHeight(effectiveItemHeight);

    std::vector<std::shared_ptr<SpinnerSection>> sections;
    sections.reserve(columns.size());

    for (auto&& col: columns)
    {
        QList<QWidget*> items;
        items.reserve(col.texts.size());
        for (int t=0;t<col.texts.size();++t)
        {
            items.append(poolLabel(col,t,col.texts.at(t)));
        }

        auto section=std::make_shared<SpinnerSection>();
        section->setItemsWidth(col.width);
        section->setCircular(col.circular);
        section->setItems(items);

        if (!col.rightBarText.isEmpty())
        {
            auto barLabel=new Label(col.rightBarText,spinner);
            barLabel->setObjectName(QStringLiteral("barLabel"));
            barLabel->setAlignment(Qt::AlignCenter);
            barLabel->setFixedHeight(effectiveItemHeight);
            auto barWidth=fm.horizontalAdvance(col.rightBarText)+2*itemHPadding;
            barLabel->setFixedWidth(barWidth);
            barLabel->resize(barWidth,effectiveItemHeight);
            section->setRightBarLabel(barLabel);
            section->setRightBarWidth(barWidth);
            col.rightBarLabel=barLabel;
        }

        sections.push_back(section);
    }

    spinner->setSections(sections);

    int totalWidth=10;
    for (auto&& s: sections)
    {
        totalWidth+=s->width();
    }
    spinner->setFixedSize(totalWidth,effectiveItemHeight*visibleRows);
    q->updateGeometry();

    applyValueToColumns();

    rebuilding=false;
}

//--------------------------------------------------------------------------

void DateTimePicker_p::updateMetrics()
{
    if (spinner==nullptr || columns.empty())
    {
        return;
    }

    q->ensurePolished();
    QFontMetrics fm(q->font());
    int newItemHeight = itemHeightProp>0 ? itemHeightProp : (fm.height()+2*itemVPadding);

    bool widthsChanged=false;
    for (auto&& col: columns)
    {
        auto w=measureColumnWidth(col.token,col.texts,fm);
        if (w!=col.width)
        {
            col.width=w;
            widthsChanged=true;
        }
    }

    if (newItemHeight!=effectiveItemHeight)
    {
        rebuild();
        return;
    }

    if (widthsChanged)
    {
        for (size_t i=0;i<columns.size();++i)
        {
            auto section=spinner->section(static_cast<int>(i));
            section->setItemsWidth(columns[i].width);
            for (auto&& w: columns[i].pool)
            {
                w->setFixedWidth(columns[i].width);
                w->resize(columns[i].width,effectiveItemHeight);
            }
        }
        int totalWidth=10;
        for (size_t i=0;i<columns.size();++i)
        {
            totalWidth+=spinner->section(static_cast<int>(i))->width();
        }
        spinner->setFixedSize(totalWidth,effectiveItemHeight*visibleRows);
        spinner->updateGeometry();
    }
}

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

DateTimePicker::DateTimePicker(QWidget* parent)
    : DateTimePicker(DateTimeField::DateTime,parent)
{}

//--------------------------------------------------------------------------

DateTimePicker::DateTimePicker(DateTimeFields fields, QWidget* parent)
    : Frame(parent),
      pimpl(std::make_unique<DateTimePicker_p>(this))
{
    construct(fields);
}

//--------------------------------------------------------------------------

DateTimePicker::~DateTimePicker()
{}

//--------------------------------------------------------------------------

void DateTimePicker::construct(DateTimeFields fields)
{
    pimpl->fields=fields;
    pimpl->locale=locale();

    auto l=Layout::vertical(this);
    pimpl->spinner=new Spinner(this);
    l->addWidget(pimpl->spinner);

    auto sample=new LineEdit();
    sample->setObjectName(QStringLiteral("styleSample"));
    pimpl->spinner->setStyleSample(sample);

    pimpl->syncTimer=new SingleShotTimer(this);

    connect(pimpl->spinner,&Spinner::itemChanged,this,[this](int section,int index)
    {
        pimpl->onItemChanged(section,index);
    });

    pimpl->rebuild();
}

//--------------------------------------------------------------------------

QDateTime DateTimePicker::dateTime() const
{
    return pimpl->value;
}

//--------------------------------------------------------------------------

QDate DateTimePicker::date() const
{
    return pimpl->value.date();
}

//--------------------------------------------------------------------------

QTime DateTimePicker::time() const
{
    return pimpl->value.time();
}

//--------------------------------------------------------------------------

void DateTimePicker::setFields(DateTimeFields fields)
{
    if (pimpl->fields==fields)
    {
        return;
    }

    pimpl->fields=fields;

    auto clamped=pimpl->clampToRange(pimpl->value);
    auto previous=pimpl->value;
    bool changed=(clamped!=pimpl->value);
    pimpl->value=clamped;

    pimpl->rebuild();

    pimpl->updateOnValueChanged(changed,previous);
}

//--------------------------------------------------------------------------

DateTimeFields DateTimePicker::fields() const noexcept
{
    return pimpl->fields;
}

//--------------------------------------------------------------------------

void DateTimePicker::setDateRange(const QDate& min, const QDate& max)
{
    pimpl->minDate=min;
    pimpl->maxDate=max;

    auto clamped=pimpl->clampToRange(pimpl->value);
    auto previous=pimpl->value;
    bool changed=(clamped!=pimpl->value);
    pimpl->value=clamped;

    pimpl->rebuild();

    pimpl->updateOnValueChanged(changed,previous);
}

//--------------------------------------------------------------------------

QDate DateTimePicker::minimumDate() const
{
    return pimpl->minDate;
}

//--------------------------------------------------------------------------

QDate DateTimePicker::maximumDate() const
{
    return pimpl->maxDate;
}

//--------------------------------------------------------------------------

void DateTimePicker::setMinuteStep(int step)
{
    step=qBound(1,step,59);
    if (pimpl->minuteStep==step)
    {
        return;
    }
    pimpl->minuteStep=step;
    pimpl->rebuild();
}

//--------------------------------------------------------------------------

int DateTimePicker::minuteStep() const noexcept
{
    return pimpl->minuteStep;
}

//--------------------------------------------------------------------------

void DateTimePicker::setSecondStep(int step)
{
    step=qBound(1,step,59);
    if (pimpl->secondStep==step)
    {
        return;
    }
    pimpl->secondStep=step;
    pimpl->rebuild();
}

//--------------------------------------------------------------------------

int DateTimePicker::secondStep() const noexcept
{
    return pimpl->secondStep;
}

//--------------------------------------------------------------------------

void DateTimePicker::setMonthFormat(MonthFormat format)
{
    if (pimpl->monthFormat==format)
    {
        return;
    }
    pimpl->monthFormat=format;
    pimpl->rebuild();
}

//--------------------------------------------------------------------------

DateTimePicker::MonthFormat DateTimePicker::monthFormat() const noexcept
{
    return pimpl->monthFormat;
}

//--------------------------------------------------------------------------

void DateTimePicker::setCircularFields(DateTimeFields fields)
{
    if (pimpl->circularFields==fields)
    {
        return;
    }
    pimpl->circularFields=fields;
    pimpl->rebuild();
}

//--------------------------------------------------------------------------

DateTimeFields DateTimePicker::circularFields() const noexcept
{
    return pimpl->circularFields;
}

//--------------------------------------------------------------------------

void DateTimePicker::setLocale(const QLocale& locale)
{
    QWidget::setLocale(locale);
}

//--------------------------------------------------------------------------

void DateTimePicker::setItemHeight(int val) noexcept
{
    if (pimpl->itemHeightProp==val)
    {
        return;
    }
    pimpl->itemHeightProp=val;
    pimpl->updateMetrics();
}

//--------------------------------------------------------------------------

int DateTimePicker::itemHeight() const noexcept
{
    return pimpl->itemHeightProp;
}

//--------------------------------------------------------------------------

void DateTimePicker::setItemWidth(int val) noexcept
{
    if (pimpl->itemWidthProp==val)
    {
        return;
    }
    pimpl->itemWidthProp=val;
    pimpl->updateMetrics();
}

//--------------------------------------------------------------------------

int DateTimePicker::itemWidth() const noexcept
{
    return pimpl->itemWidthProp;
}

//--------------------------------------------------------------------------

void DateTimePicker::setVisibleRows(int val) noexcept
{
    if (val<1)
    {
        val=1;
    }
    if (pimpl->visibleRows==val)
    {
        return;
    }
    pimpl->visibleRows=val;
    if (pimpl->spinner!=nullptr && pimpl->effectiveItemHeight>0)
    {
        pimpl->spinner->setFixedSize(pimpl->spinner->width(),pimpl->effectiveItemHeight*pimpl->visibleRows);
    }
}

//--------------------------------------------------------------------------

int DateTimePicker::visibleRows() const noexcept
{
    return pimpl->visibleRows;
}

//--------------------------------------------------------------------------

void DateTimePicker::setItemHPadding(int val) noexcept
{
    if (pimpl->itemHPadding==val)
    {
        return;
    }
    pimpl->itemHPadding=val;
    pimpl->rebuild();
}

//--------------------------------------------------------------------------

int DateTimePicker::itemHPadding() const noexcept
{
    return pimpl->itemHPadding;
}

//--------------------------------------------------------------------------

void DateTimePicker::setItemVPadding(int val) noexcept
{
    if (pimpl->itemVPadding==val)
    {
        return;
    }
    pimpl->itemVPadding=val;
    pimpl->rebuild();
}

//--------------------------------------------------------------------------

int DateTimePicker::itemVPadding() const noexcept
{
    return pimpl->itemVPadding;
}

//--------------------------------------------------------------------------

Spinner* DateTimePicker::spinner() const noexcept
{
    return pimpl->spinner;
}

//--------------------------------------------------------------------------

void DateTimePicker::setDateTime(const QDateTime& value)
{
    pimpl->setValue(value);
}

//--------------------------------------------------------------------------

void DateTimePicker::setDate(const QDate& value)
{
    pimpl->setValue(QDateTime(value,pimpl->value.time()));
}

//--------------------------------------------------------------------------

void DateTimePicker::setTime(const QTime& value)
{
    pimpl->setValue(QDateTime(pimpl->value.date(),value));
}

//--------------------------------------------------------------------------

void DateTimePicker::changeEvent(QEvent* event)
{
    Frame::changeEvent(event);

    if (event==nullptr)
    {
        return;
    }

    switch (event->type())
    {
        case (QEvent::FontChange):
        case (QEvent::StyleChange):
        case (QEvent::ApplicationFontChange):
            pimpl->updateMetrics();
            break;

        case (QEvent::LocaleChange):
        case (QEvent::LanguageChange):
            pimpl->locale=locale();
            pimpl->rebuild();
            break;

        case (QEvent::PaletteChange):
            if (pimpl->spinner!=nullptr)
            {
                pimpl->spinner->update();
            }
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------

void DateTimePicker::showEvent(QShowEvent* event)
{
    Frame::showEvent(event);
    pimpl->updateMetrics();
}

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

DatePicker::DatePicker(QWidget* parent)
    : DateTimePicker(DateTimeField::Date,parent)
{}

//--------------------------------------------------------------------------

MonthPicker::MonthPicker(QWidget* parent)
    : DateTimePicker(DateTimeField::YearMonth,parent)
{}

//--------------------------------------------------------------------------

TimePicker::TimePicker(QWidget* parent)
    : DateTimePicker(DateTimeField::Time,parent)
{}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
