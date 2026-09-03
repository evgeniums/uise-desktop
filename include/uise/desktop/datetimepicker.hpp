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

/** @file uise/desktop/datetimepicker.hpp
*
*  Declares DateTimePicker -- a wheel (iOS style) date/time picking widget built on top of
*  Spinner.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DATETIMEPICKER_HPP
#define UISE_DESKTOP_DATETIMEPICKER_HPP

#include <memory>

#include <QDate>
#include <QTime>
#include <QDateTime>
#include <QLocale>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class Spinner;
class DateTimePicker_p;

/**
 * @brief Field of a DateTimePicker that can be shown as a wheel.
 */
enum class DateTimeField
{
    None=0x00,
    Year=0x01,
    Month=0x02,
    Day=0x04,
    Hour=0x08,
    Minute=0x10,
    Second=0x20,

    YearMonth=Year|Month,          //!< year and month only -- month-selection mode
    Date=Year|Month|Day,
    Time=Hour|Minute,
    TimeSec=Hour|Minute|Second,
    DateTime=Date|Time
};
Q_DECLARE_FLAGS(DateTimeFields,DateTimeField)

/**
 * @brief Wheel (iOS style) date/time picking widget.
 *
 * DateTimePicker shows one or more of {year, month, day, hour, minute, second} as scrollable
 * wheels of a single Spinner, so a single selection band spans all of them at once. Which
 * wheels are shown is controlled by setFields(); DatePicker, MonthPicker and TimePicker are
 * thin presets of the most common combinations.
 *
 * Column order is derived from the current locale's short date/time format (see setLocale()),
 * so e.g. a German locale shows "day.month.year" while a US locale shows "month/day/year".
 * Separator labels between the columns (the locale's "." or "/" or ":") are available via
 * setSeparatorsVisible() and are hidden by default.
 */
class UISE_DESKTOP_EXPORT DateTimePicker : public Frame
{
    Q_OBJECT

    Q_PROPERTY(int itemHeight READ itemHeight WRITE setItemHeight)
    Q_PROPERTY(int itemWidth READ itemWidth WRITE setItemWidth)
    Q_PROPERTY(int visibleRows READ visibleRows WRITE setVisibleRows)
    Q_PROPERTY(int itemHPadding READ itemHPadding WRITE setItemHPadding)
    Q_PROPERTY(int itemVPadding READ itemVPadding WRITE setItemVPadding)
    Q_PROPERTY(bool separatorsVisible READ separatorsVisible WRITE setSeparatorsVisible)
    Q_PROPERTY(QString dateSeparator READ dateSeparator WRITE setDateSeparator)
    Q_PROPERTY(QString timeSeparator READ timeSeparator WRITE setTimeSeparator)

    public:

        /**
         * @brief Format used to render the month wheel's text.
         */
        enum class MonthFormat
        {
            LongName,
            ShortName,
            Number
        };

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         *
         * Fields default to DateTimeField::DateTime.
         */
        explicit DateTimePicker(QWidget* parent=nullptr);

        /**
         * @brief Constructor.
         * @param fields Fields to show.
         * @param parent Parent widget.
         */
        explicit DateTimePicker(DateTimeFields fields, QWidget* parent=nullptr);

        ~DateTimePicker();

        DateTimePicker(const DateTimePicker&)=delete;
        DateTimePicker(DateTimePicker&&)=delete;
        DateTimePicker& operator=(const DateTimePicker&)=delete;
        DateTimePicker& operator=(DateTimePicker&&)=delete;

        /**
         * @brief Get current value.
         * @return Query result.
         *
         * If DateTimeField::Day is not among fields() the day part is always 1.
         * If a time field is not among fields() the corresponding time part is always 0.
         */
        QDateTime dateTime() const;

        /**
         * @brief Get date part of the current value.
         * @return Query result.
         */
        QDate date() const;

        /**
         * @brief Get time part of the current value.
         * @return Query result.
         */
        QTime time() const;

        /**
         * @brief Set fields to show.
         * @param fields Fields.
         *
         * Rebuilds the wheels. The current value is preserved as closely as the new set of
         * fields allows (e.g. switching away from DateTimeField::Day pins the day to 1).
         */
        void setFields(DateTimeFields fields);

        DateTimeFields fields() const noexcept;

        /**
         * @brief Set allowed date range.
         * @param min Minimum date, inclusive.
         * @param max Maximum date, inclusive.
         *
         * Only the year wheel is bounded by this range; month/day/time wheels are always
         * complete and the current value is clamped into range instead (see class docs).
         * When DateTimeField::Day is not shown, clamping compares at month granularity.
         */
        void setDateRange(const QDate& min, const QDate& max);

        QDate minimumDate() const;
        QDate maximumDate() const;

        /**
         * @brief Set step of the minute wheel.
         * @param step One of 1, 5, 10, 15, 30.
         */
        void setMinuteStep(int step);

        int minuteStep() const noexcept;

        /**
         * @brief Set step of the second wheel.
         * @param step One of 1, 5, 10, 15, 30.
         */
        void setSecondStep(int step);

        int secondStep() const noexcept;

        /**
         * @brief Set text format of the month wheel.
         */
        void setMonthFormat(MonthFormat format);

        MonthFormat monthFormat() const noexcept;

        /**
         * @brief Set which fields scroll as circular (wrap-around) wheels.
         * @param fields Fields to make circular.
         *
         * Default is Month|Day|Hour|Minute|Second. Year is never circular.
         */
        void setCircularFields(DateTimeFields fields);

        DateTimeFields circularFields() const noexcept;

        /**
         * @brief Show or hide separator labels between the columns.
         * @param enable Whether to show the separators.
         *
         * Separator text is taken from the locale's short date/time format (e.g. "/" or "."
         * between date columns, ":" between hour and minute). Hidden by default.
         */
        void setSeparatorsVisible(bool enable);

        bool separatorsVisible() const noexcept;

        /**
         * @brief Override the separator text between date columns.
         * @param text Separator text; empty string (default) means derive from the locale's
         *  short date format (e.g. "/" for en_US, "." for de_DE).
         *
         * Only rendered when separators are visible, see setSeparatorsVisible().
         */
        void setDateSeparator(const QString& text);

        QString dateSeparator() const;

        /**
         * @brief Override the separator text between hour/minute/second columns.
         * @param text Separator text; empty string (default) means derive from the locale's
         *  short time format (usually ":"). The separator around the AM/PM column always
         *  comes from the locale format.
         *
         * Only rendered when separators are visible, see setSeparatorsVisible().
         */
        void setTimeSeparator(const QString& text);

        QString timeSeparator() const;

        /**
         * @brief Set locale used to derive column order, month/AM-PM names and 12/24-hour mode.
         */
        void setLocale(const QLocale& locale);

        /**
         * @brief Set fixed height of a wheel row.
         * @param val Height in pixels, or 0 to derive it from the item font metrics.
         */
        void setItemHeight(int val) noexcept;

        int itemHeight() const noexcept;

        /**
         * @brief Set fixed width applied to every wheel's items.
         * @param val Width in pixels, or 0 to derive it per-column from the item font metrics.
         *
         * A single fixed width is coarser than per-column measurement (year/day/month all end
         * up the same width), but it is simple and avoids any dependency on font metrics
         * matching between this widget and the actual item labels; see setItemHeight().
         */
        void setItemWidth(int val) noexcept;

        int itemWidth() const noexcept;

        /**
         * @brief Set number of visible rows in each wheel.
         * @param val Number of rows, should be odd. Default is 5.
         */
        void setVisibleRows(int val) noexcept;

        int visibleRows() const noexcept;

        void setItemHPadding(int val) noexcept;

        int itemHPadding() const noexcept;

        void setItemVPadding(int val) noexcept;

        int itemVPadding() const noexcept;

        /**
         * @brief Get the underlying Spinner.
         * @return Query result.
         *
         * Escape hatch for advanced styling/testing; the picker owns the Spinner's sections and
         * rebuilds them as needed, so callers must not call Spinner::setSections() on it.
         */
        Spinner* spinner() const noexcept;

    public slots:

        void setDateTime(const QDateTime& value);
        void setDate(const QDate& value);
        void setTime(const QTime& value);

    signals:

        /**
         * @brief Emitted when the value actually changes.
         */
        void dateTimeChanged(const QDateTime& value);

        /**
         * @brief Emitted when the date part of the value actually changes.
         */
        void dateChanged(const QDate& value);

        /**
         * @brief Emitted when the time part of the value actually changes.
         */
        void timeChanged(const QTime& value);

    protected:

        void changeEvent(QEvent* event) override;
        void showEvent(QShowEvent* event) override;

    private:

        void construct(DateTimeFields fields);

        std::unique_ptr<DateTimePicker_p> pimpl;
};

/**
 * @brief DateTimePicker preset to DateTimeField::Date.
 */
class UISE_DESKTOP_EXPORT DatePicker : public DateTimePicker
{
    Q_OBJECT

    public:

        explicit DatePicker(QWidget* parent=nullptr);
};

/**
 * @brief DateTimePicker preset to DateTimeField::YearMonth -- month-selection mode.
 */
class UISE_DESKTOP_EXPORT MonthPicker : public DateTimePicker
{
    Q_OBJECT

    public:

        explicit MonthPicker(QWidget* parent=nullptr);
};

/**
 * @brief DateTimePicker preset to DateTimeField::Time.
 */
class UISE_DESKTOP_EXPORT TimePicker : public DateTimePicker
{
    Q_OBJECT

    public:

        explicit TimePicker(QWidget* parent=nullptr);
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(UISE_DESKTOP_NAMESPACE::DateTimeFields)

#endif // UISE_DESKTOP_DATETIMEPICKER_HPP
