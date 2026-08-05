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

/** @file uise/desktop/calendarinput.hpp
*
*  Declares CalendarDropdown and CalendarInput.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CALENDARINPUT_HPP
#define UISE_DESKTOP_CALENDARINPUT_HPP

#include <QDate>
#include <QList>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/dropdownframe.hpp>
#include <uise/desktop/lineedit.hpp>
#include <uise/desktop/calendar.hpp>

class QFrame;

UISE_DESKTOP_NAMESPACE_BEGIN

class PushButton;

/**
 * @brief Animated dropdown that hosts a Calendar, with an optional Apply/Cancel buttons row.
 *
 * The calendar is created once and kept alive across openings -- like DateTimePickerDropdown and
 * unlike DropdownMenu, this frame never rebuilds its content in fillContent(), since the
 * calendar's own page and selection are the live state being edited (see DropdownFrame::
 * fillContent() docs on externally/persistently owned content).
 */
class UISE_DESKTOP_EXPORT CalendarDropdown : public DropdownFrame
{
    Q_OBJECT

    public:

        explicit CalendarDropdown(QWidget* parent=nullptr);
        explicit CalendarDropdown(CalendarMode mode, QWidget* parent=nullptr);

        Calendar* calendar() const noexcept
        {
            return m_calendar;
        }

        /**
         * @brief Let the buttons row follow the calendar's configured mode automatically.
         * @param enable Default true.
         *
         * A click is inherently terminal in Activation/SingleSelection, so the row is hidden
         * and the dropdown auto-closes on the corresponding signal instead; Range/Multiple
         * selection need an explicit commit, so the row is shown and auto-close is off. In
         * Auto mode the row is shown (the user may escalate at any moment) but auto-close still
         * applies while effectiveMode() is Activation. See setButtonsVisible() to override.
         */
        void setAutoButtons(bool enable);

        bool isAutoButtons() const noexcept
        {
            return m_autoButtons;
        }

        /**
         * @brief Show/hide the Apply/Cancel buttons row explicitly.
         *
         * Turns setAutoButtons() off.
         */
        void setButtonsVisible(bool enable);

        bool isButtonsVisible() const noexcept
        {
            return m_buttonsVisible;
        }

        /**
         * @brief Close the dropdown as soon as a click is terminal (no buttons row showing).
         * @param enable Default true.
         */
        void setAutoClose(bool enable) noexcept;

        bool isAutoClose() const noexcept
        {
            return m_autoClose;
        }

    signals:

        void applied();
        void cancelled();

    private:

        void construct(CalendarMode mode);
        void updateButtonsPolicy();
        void takeSnapshot();
        void restoreSnapshot();

        Calendar* m_calendar=nullptr;
        QFrame* m_buttonsFrame=nullptr;
        PushButton* m_applyButton=nullptr;
        PushButton* m_cancelButton=nullptr;
        bool m_buttonsVisible=false;
        bool m_autoButtons=true;
        bool m_autoClose=true;

        QDate m_snapshotMonth;
        QDate m_snapshotSingle;
        QDate m_snapshotFrom;
        QDate m_snapshotTo;
        QList<QDate> m_snapshotDates;
};

/**
 * @brief Read-only line-edit style field that opens a CalendarDropdown on click.
 *
 * API mirrors DateTimeInput closely enough to be a near-drop-in replacement in analogous code.
 */
class UISE_DESKTOP_EXPORT CalendarInput : public LineEdit
{
    Q_OBJECT

    public:

        explicit CalendarInput(QWidget* parent=nullptr);
        explicit CalendarInput(CalendarMode mode, QWidget* parent=nullptr);

        ~CalendarInput();

        Calendar* calendar() const noexcept;

        CalendarDropdown* dropdown() const noexcept
        {
            return m_dropdown;
        }

        void setMode(CalendarMode mode);
        CalendarMode mode() const;

        void setDateRange(const QDate& min, const QDate& max);

        /**
         * @brief Set display text format.
         * @param format Qt date format string (see QLocale::toString()).
         *
         * Default (empty format) uses the current locale's short date form for every date
         * rendered.
         */
        void setDisplayFormat(const QString& format);

        QString displayFormat() const;

        QDate selectedDate() const;
        QDate rangeFrom() const;
        QDate rangeTo() const;
        QList<QDate> selectedDates() const;

    public slots:

        void setSelectedDate(const QDate& date);
        void setSelectedRange(const QDate& from, const QDate& to);
        void setSelectedDates(const QList<QDate>& dates);

        void openPopup();
        void closePopup();

    signals:

        void dateActivated(const QDate& date);
        void selectionChanged();
        void rangeSelected(const QDate& from, const QDate& to);
        void selectedDatesChanged(const QList<QDate>& dates);

    protected:

        void mousePressEvent(QMouseEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;

    private:

        void construct(CalendarMode mode);
        void updateText();
        void updatePlaceholder();

        CalendarDropdown* m_dropdown=nullptr;
        PushButton* m_pickerButton=nullptr;
        QString m_displayFormat;
        QDate m_lastActivated;   //!< Activation mode keeps no state of its own -- this does
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CALENDARINPUT_HPP
