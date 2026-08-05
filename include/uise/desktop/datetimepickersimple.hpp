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

/** @file uise/desktop/datetimepickersimple.hpp
*
*  Declares DateTimePickerSimple.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DATETIMEPICKERSIMPLE_HPP
#define UISE_DESKTOP_DATETIMEPICKERSIMPLE_HPP

#include <QFrame>
#include <QDate>
#include <QList>

#include <uise/desktop/uisedesktop.hpp>

class QLabel;

UISE_DESKTOP_NAMESPACE_BEGIN

class Spinner;

/**
 * @brief Minimal month/day/year date picker built directly on top of Spinner.
 *
 * Deliberately as close as possible to the plain Spinner usage in demo/spinner/main.cpp: three
 * fixed-size QLabel sections (month name, day number, year number) built once and handed to
 * Spinner::setSections() a single time in the constructor -- no rebuilding, no font-metric-based
 * auto-sizing, no locale-driven column order. This is a deliberately reduced starting point to
 * validate the underlying Spinner-based approach in isolation before layering the fancier
 * DateTimePicker behaviour back on top of it.
 *
 * The day wheel does adapt to the selected month/year (28/29/30/31 days), via
 * Spinner::appendItems()/removeLastItems() on a fixed pool of 31 pre-built day labels -- see
 * adjustDayCount().
 */
class UISE_DESKTOP_EXPORT DateTimePickerSimple : public QFrame
{
    Q_OBJECT

    public:

        explicit DateTimePickerSimple(QWidget* parent=nullptr);

        /**
         * @brief Construct with optional separator labels between the columns.
         *
         * Separators are Spinner bar labels showing the locale's date separator character
         * (e.g. "/" for en_US, "." for de_DE) between month/day and day/year. They can only
         * be chosen at construction time because Spinner::setSections() is called exactly
         * once; there is no runtime toggle.
         */
        explicit DateTimePickerSimple(bool withSeparators, QWidget* parent=nullptr);

        QDate date() const noexcept
        {
            return m_date;
        }

        Spinner* spinner() const noexcept
        {
            return m_spinner;
        }

    public slots:

        void setDate(const QDate& value);

    signals:

        void dateChanged(const QDate& value);

    private:

        void onItemChanged(int sectionIndex, int itemIndex);
        void updateDateFromSelection();

        /**
         * @brief Grow/shrink the day wheel to match daysInMonth() of the currently selected
         *  month/year, reusing already-built labels from m_dayLabels (see Spinner::appendItems()
         *  / Spinner::removeLastItems() docs -- items are never destroyed, only hidden, so
         *  growing back to a previously-seen count is allocation-free).
         */
        void adjustDayCount();

        Spinner* m_spinner;

        QList<QLabel*> m_monthLabels;
        QList<QLabel*> m_dayLabels;
        QList<QLabel*> m_yearLabels;

        int m_loadedDayCount;

        QDate m_date;

        int m_minYear;
        int m_maxYear;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_DATETIMEPICKERSIMPLE_HPP
