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

/** @file uise/desktop/detail/datetimepicker_p.hpp
*
*  Contains declaration of DateTimePicker_p and the date-agnostic column/pool helper it uses.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DETAIL_DATETIMEPICKER_P_HPP
#define UISE_DESKTOP_DETAIL_DATETIMEPICKER_P_HPP

#include <vector>

#include <QList>
#include <QString>
#include <QStringList>
#include <QPointer>
#include <QDate>
#include <QTime>
#include <QDateTime>
#include <QLocale>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/datetimepicker.hpp>

class QWidget;
class QFontMetrics;

UISE_DESKTOP_NAMESPACE_BEGIN

class Spinner;
class SingleShotTimer;

namespace detail
{

/**
 * @brief Internal wheel identity, one level more granular than DateTimeField -- adds the
 *  AM/PM wheel, which is not part of the public field mask (it always accompanies Hour when
 *  the locale is 12-hour).
 */
enum class Token : int
{
    Year,
    Month,
    Day,
    Hour,
    Minute,
    Second,
    AmPm
};

/**
 * @brief Order and inter-column separator of one wheel, as derived from the current locale's
 *  date/time format.
 */
struct ColumnPlan
{
    Token token=Token::Year;
    QString rightBarText;
};

/**
 * @brief One wheel of a DateTimePicker -- a date-agnostic column of text items with a widget
 *  pool.
 *
 * The pool keeps every label ever created for this column alive as a hidden child of the
 * Spinner (see Spinner::removeLastItems()), so growing/shrinking the visible list back and
 * forth within one field/locale configuration (e.g. day count 28..31) never allocates after
 * the first pass through each length.
 */
struct PickerColumn
{
    Token token=Token::Year;
    QList<QWidget*> pool;
    QStringList texts;
    int loaded=0;
    int width=0;
    bool circular=false;
    QString rightBarText;
    QPointer<QWidget> rightBarLabel;
};

}

/**
 * @brief Private implementation of DateTimePicker.
 */
class DateTimePicker_p
{
    public:

        explicit DateTimePicker_p(DateTimePicker* picker);

        DateTimePicker* q;

        Spinner* spinner=nullptr;

        DateTimeFields fields=DateTimeField::DateTime;
        DateTimeFields circularFields=DateTimeField::Month|DateTimeField::Day|DateTimeField::Hour|DateTimeField::Minute|DateTimeField::Second;

        QDate minDate{1900,1,1};
        QDate maxDate{2100,12,31};

        int minuteStep=1;
        int secondStep=1;
        DateTimePicker::MonthFormat monthFormat=DateTimePicker::MonthFormat::LongName;
        bool separatorsVisible=false;
        QString dateSeparatorOverride;
        QString timeSeparatorOverride;

        QLocale locale;

        QDateTime value{QDate::currentDate(),QTime(0,0)};

        int visibleRows=5;
        int itemHeightProp=0;
        int itemWidthProp=0;
        int itemHPadding=10;
        int itemVPadding=6;
        int effectiveItemHeight=0;

        bool rebuilding=false;

        std::vector<detail::PickerColumn> columns;

        SingleShotTimer* syncTimer=nullptr;

        // --- lifecycle -------------------------------------------------

        void rebuild();
        void updateMetrics();

        // --- value <-> columns -------------------------------------------

        void setValue(const QDateTime& dt);
        void applyValueToColumns();
        void verifyColumns();
        void reconcile();
        void onItemChanged(int sectionIndex,int itemIndex);

        QDateTime clampToRange(const QDateTime& dt) const;
        QDateTime composeFromColumns() const;
        void updateOnValueChanged(bool changed, const QDateTime& previous);
        void updateOutOfRangeMarkers();

        // --- columns -------------------------------------------------------

        std::vector<detail::ColumnPlan> buildColumnPlan() const;
        QStringList columnTextsFor(detail::Token token) const;
        int indexForValue(detail::Token token, const QDateTime& dt) const;
        void setColumnTexts(int columnIndex, const QStringList& texts);
        QWidget* poolLabel(detail::PickerColumn& col, int index, const QString& text);
        int measureColumnWidth(detail::Token token, const QStringList& texts, const QFontMetrics& fm) const;
        bool isCircular(detail::Token token) const;
        bool is12Hour() const;
        int dayColumnIndex() const;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_DETAIL_DATETIMEPICKER_P_HPP
