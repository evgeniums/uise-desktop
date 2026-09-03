/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/utils/atetime.hpp
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DATETIME_HPP
#define UISE_DESKTOP_DATETIME_HPP

#include <QDateTime>
#include <QList>
#include <QLocale>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

inline QString printCurrentDateTime()
{
    return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
}

namespace detail {

/**
 * @brief One piece of a parsed Qt date format: a field run, or the literal text between fields.
 *
 * Dropping a field from a locale's date pattern cannot be done by regex, because the punctuation
 * around a field is not uniform across locales and some of it is *quoted* text that must not be
 * read as format characters at all. Spanish long format is "dddd, d 'de' MMMM 'de' yyyy" and
 * Russian is "dddd, d MMMM yyyy 'г'." -- removing the year with a pattern that doesn't understand
 * quoting leaves a dangling "de" or "г." behind. So the format is tokenised properly instead.
 */
struct DateFormatPart
{
    QString text;         //!< field run verbatim, or literal text with its quoting already decoded
    QString replacement;  //!< when set, emitted (quoted) in place of a field -- see monthAndYear
    QChar field;          //!< field letter for a field part; null QChar for a literal
    bool attached=false;  //!< literal that is a tight suffix of the field in front of it
    bool dropped=false;

    bool isField() const noexcept
    {
        return !field.isNull();
    }
};

//! Wrap text so Qt's date formatter reproduces it verbatim: unquoted it would be read as fields
//! ("de" is a day field followed by nothing useful).
inline QString quoteDateLiteral(const QString& text)
{
    QString escaped=text;
    escaped.replace('\'',"''");
    return '\'' + escaped + '\'';
}

//! Split a date format into field runs and literals, honouring '...' quoting and '' escapes.
inline QList<DateFormatPart> parseDateFormat(const QString& format)
{
    QList<DateFormatPart> parts;
    QString literal;

    auto flushLiteral=[&parts,&literal]()
    {
        if (!literal.isEmpty())
        {
            DateFormatPart part;
            part.text=literal;
            parts.append(part);
            literal.clear();
        }
    };

    for (int i=0; i<format.size();)
    {
        const QChar c=format.at(i);

        if (c=='\'')
        {
            if (i+1<format.size() && format.at(i+1)=='\'')
            {
                literal+='\'';
                i+=2;
                continue;
            }
            for (++i; i<format.size() && format.at(i)!='\''; ++i)
            {
                literal+=format.at(i);
            }
            ++i;
            continue;
        }

        // Deliberately not QChar::isLetter(): that is true for CJK ideographs, and Japanese,
        // Chinese and Korean patterns spell their fields with them ("yyyy年M月d日", "yyyy년 MMMM").
        // Qt's own formatter only ever treats ASCII letters as fields, so treating 年/월 as a field
        // run would both mangle the output and hide the literal from the logic below.
        if ((c>='A' && c<='Z') || (c>='a' && c<='z'))
        {
            flushLiteral();
            int j=i;
            while (j<format.size() && format.at(j)==c)
            {
                ++j;
            }
            DateFormatPart part;
            part.text=format.mid(i,j-i);
            part.field=c;
            parts.append(part);
            i=j;
            continue;
        }

        literal+=c;
        ++i;
    }

    flushLiteral();
    return parts;
}

/**
 * @brief Mark punctuation that belongs to the field in front of it rather than separating fields.
 *
 * German and Hungarian write the day as "d.", Japanese writes "d日" -- that punctuation is part of
 * how the field itself is spelled and has to be removed with it, and kept with it. A comma is
 * deliberately never treated this way: it always separates ("MMMM d, yyyy"), so attaching it to the
 * day would strand it as "September 4," once the year is dropped. That holds for the locale's own
 * comma too, not just the ASCII one -- Arabic patterns separate with "،".
 */
inline bool isDateFieldSeparator(QChar c)
{
    return c.isSpace() || c==',' || c==QChar(0x060C) || c==QChar(0x3001) || c==';';
}

inline void splitAttachedSuffixes(QList<DateFormatPart>& parts)
{
    for (int i=1; i<parts.size(); ++i)
    {
        if (!parts.at(i-1).isField() || parts.at(i).isField() || parts.at(i).attached)
        {
            continue;
        }

        const QString text=parts.at(i).text;
        int n=0;
        while (n<text.size() && !isDateFieldSeparator(text.at(n)))
        {
            ++n;
        }
        if (n==0)
        {
            continue;
        }

        DateFormatPart suffix;
        suffix.text=text.left(n);
        suffix.attached=true;

        if (n==text.size())
        {
            parts[i]=suffix;
        }
        else
        {
            parts[i].text=text.mid(n);
            parts.insert(i,suffix);
            ++i;
        }
    }
}

/**
 * @brief Drop every field the predicate selects, plus the punctuation that only bound it.
 *
 * Each dropped field takes its own attached suffix and *exactly one* neighbouring separator -- the
 * following one when there is one, else the preceding one. Dropping one and not both is what keeps
 * the surviving neighbours apart: removing the day from "MMMM d, yyyy" must leave "MMMM yyyy", not
 * "MMMMyyyy".
 */
template <typename PredicateT>
inline void dropDateFields(QList<DateFormatPart>& parts, PredicateT&& drop)
{
    for (int i=0; i<parts.size(); ++i)
    {
        if (!parts.at(i).isField() || parts.at(i).dropped
            || !drop(parts.at(i).field,parts.at(i).text.size()))
        {
            continue;
        }
        parts[i].dropped=true;

        int next=i+1;
        if (next<parts.size() && parts.at(next).attached)
        {
            parts[next].dropped=true;
            ++next;
        }

        if (next<parts.size() && !parts.at(next).isField() && !parts.at(next).dropped)
        {
            parts[next].dropped=true;
            continue;
        }

        for (int prev=i-1; prev>=0; --prev)
        {
            if (parts.at(prev).dropped)
            {
                continue;
            }
            if (!parts.at(prev).isField() && !parts.at(prev).attached)
            {
                parts[prev].dropped=true;
            }
            break;
        }
    }
}

//! Whether a trailing literal carries meaning ("2026 г.") rather than just separating fields.
inline bool isDateQualifier(const QString& text)
{
    for (const QChar& c : text)
    {
        if (c.isLetterOrNumber())
        {
            return true;
        }
    }
    return false;
}

//! What to do with a qualifier the locale appends to the year -- Russian "г.", Ukrainian "р.".
enum class TrailingQualifier
{
    Keep,   //!< a full date reads "4 сентября 2026 г.", as CLDR spells it
    Drop    //!< a compact header reads "сентябрь 2026"
};

/**
 * @brief Reassemble a format, discarding dropped parts and anything left stranded at either end.
 *
 * A trailing *attached* literal is always kept -- it spells its own field, as in Japanese "M月d日".
 * A trailing free literal is kept only when it qualifies a surviving field and the caller asked to
 * keep it, which is how the Russian era marker survives in a full date but not in a month header.
 */
inline QString rebuildDateFormat(const QList<DateFormatPart>& parts,
                                 TrailingQualifier qualifier=TrailingQualifier::Drop)
{
    int first=0;
    int last=parts.size()-1;

    while (first<=last && (parts.at(first).dropped || !parts.at(first).isField()))
    {
        ++first;
    }
    while (last>=first)
    {
        const auto& part=parts.at(last);
        if (!part.dropped
            && (part.isField()
                || part.attached
                || (qualifier==TrailingQualifier::Keep && isDateQualifier(part.text))))
        {
            break;
        }
        --last;
    }

    // Adjacent literals must be emitted as ONE quoted run: "'.'' '" would be read back as
    // ". ' " -- Qt sees the two inner quotes as an escaped literal quote and the text runs on.
    QString format;
    QString pending;

    auto flushPending=[&format,&pending]()
    {
        if (!pending.isEmpty())
        {
            format+=quoteDateLiteral(pending);
            pending.clear();
        }
    };

    for (int i=first; i<=last; ++i)
    {
        const auto& part=parts.at(i);
        if (part.dropped)
        {
            continue;
        }
        if (!part.replacement.isEmpty())
        {
            pending+=part.replacement;
        }
        else if (part.isField())
        {
            flushPending();
            format+=part.text;
        }
        else
        {
            pending+=part.text;
        }
    }
    flushPending();
    return format;
}

//! Guard against a locale whose pattern leaves nothing behind: showing the full date beats an
//! empty label.
inline QString rebuildDateFormatOr(const QList<DateFormatPart>& parts, const QString& fallback,
                                   TrailingQualifier qualifier=TrailingQualifier::Drop)
{
    const QString format=rebuildDateFormat(parts,qualifier);
    return format.isEmpty() ? fallback : format;
}

} // namespace detail

/**
 * @brief Format a date as month and day in the locale's own idiom, without weekday or year.
 *
 * Derived from the locale's long date format, so field order and wording follow the locale:
 * "September 4" (en), "4 сентября" (ru), "4. September" (de), "4 de septiembre" (es).
 */
inline QString dateAsMonthAndDay(const QDateTime& dt, const QLocale& locale=QLocale{})
{
    const QString longFormat=locale.dateFormat(QLocale::LongFormat);
    auto parts=detail::parseDateFormat(longFormat);
    detail::splitAttachedSuffixes(parts);
    detail::dropDateFields(parts,[](QChar field, int count)
    {
        // 'd' repeated 3+ times is the weekday name, 1-2 times the day of month.
        return field=='y' || (field=='d' && count>=3);
    });
    return locale.toString(dt,detail::rebuildDateFormatOr(parts,longFormat));
}

/**
 * @brief Format a full date -- day, month and year -- with the weekday removed.
 *
 * For a date label that has to name the year. Do NOT build this by appending the year to
 * dateAsMonthAndDay(): the year is not a suffix in every language. Hungarian and the CJK locales
 * lead with it ("2026. szeptember 4.", "2026年9月4日"), and the separator is the locale's, not a
 * comma -- so a hand-built "%1, %2" is wrong everywhere except the handful of locales that happen
 * to match it, and it reads especially badly in right-to-left text, where a Latin comma between an
 * Arabic date and a number is not what the locale uses.
 *
 * The year keeps its qualifier here (Russian "4 сентября 2026 г."), unlike the compact month/year
 * header, because this is the locale's full date and that is how CLDR spells it.
 */
inline QString dateWithoutWeekday(const QDateTime& dt, const QLocale& locale=QLocale{})
{
    const QString longFormat=locale.dateFormat(QLocale::LongFormat);
    auto parts=detail::parseDateFormat(longFormat);
    detail::splitAttachedSuffixes(parts);
    detail::dropDateFields(parts,[](QChar field, int count)
    {
        return field=='d' && count>=3;
    });
    return locale.toString(dt,detail::rebuildDateFormatOr(parts,longFormat,
                                                          detail::TrailingQualifier::Keep));
}

/**
 * @brief Format a date as month and year, without weekday or day of month.
 *
 * The month is emitted through QLocale::standaloneMonthName(), not through an MMMM field. In a
 * format string Qt always renders MMMM with the *formatting* month name, which in Russian and other
 * inflected languages is the genitive form used when a day is present -- "сентября". With the day
 * gone the month has to stand on its own, which is the nominative "сентябрь". Languages without
 * that distinction are unaffected, as the two forms are identical there.
 */
inline QString dateAsMonthAndYear(const QDate& date, const QLocale& locale=QLocale{})
{
    const QString longFormat=locale.dateFormat(QLocale::LongFormat);
    auto parts=detail::parseDateFormat(longFormat);
    detail::splitAttachedSuffixes(parts);
    detail::dropDateFields(parts,[](QChar field, int)
    {
        // every 'd' run goes: the weekday name and the day of month alike.
        return field=='d';
    });

    for (auto& part : parts)
    {
        if (part.dropped || part.field!='M' || part.text.size()<3)
        {
            continue;
        }
        part.replacement=locale.standaloneMonthName(
            date.month(),
            part.text.size()>=4 ? QLocale::LongFormat : QLocale::ShortFormat
        );
    }

    return locale.toString(date,detail::rebuildDateFormatOr(parts,longFormat));
}

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_DATETIME_HPP
