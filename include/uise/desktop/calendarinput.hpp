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
 * @brief How a CalendarDropdown/CalendarInput reflects calendar picks.
 */
enum class CalendarUpdateMode : int
{
    /** Every pick lands immediately -- no Apply/Cancel row, CalendarInput's text updates live.
     *  Activation/SingleSelection/RangeSelection are each terminal at some single click (the
     *  activation itself, the date picked, or the range's second endpoint), so the dropdown also
     *  auto-closes on it (see CalendarDropdown::setAutoClose()); MultipleSelection and
     *  ExtendedSelection have no such terminal click -- every click is potentially just one of
     *  several -- and stay open until dismissed. */
    Auto,

    /** Picks stay pending in the calendar until Apply is clicked; Cancel (or dismissing the
     *  popup any other way -- Escape, outside click, re-clicking the trigger) discards them and
     *  reverts the calendar to the last applied state. The Apply/Cancel row is shown for this.
     *  CalendarMode::Activation is always forced to behave as Auto regardless of this setting --
     *  a plain activation click has nothing to stage, so there is nothing an Apply step could
     *  add. */
    Explicit
};

/**
 * @brief Animated dropdown that hosts a Calendar, with an Apply/Cancel buttons row in
 * CalendarUpdateMode::Explicit.
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
         * @brief Set whether picks apply immediately or stay pending until Apply is clicked.
         * @param mode Default CalendarUpdateMode::Auto.
         *
         * Drives the Apply/Cancel row's visibility -- see CalendarUpdateMode.
         */
        void setUpdateMode(CalendarUpdateMode mode);

        CalendarUpdateMode updateMode() const noexcept
        {
            return m_updateMode;
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
        CalendarUpdateMode m_updateMode=CalendarUpdateMode::Auto;
        bool m_autoClose=true;
        bool m_sessionCommitted=false;   //!< set by Apply/Cancel; see aboutToHide handling

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

        /** @brief See CalendarDropdown::setUpdateMode(). Forwarded to dropdown(); also governs
         *  whether this input's own text/signals track the calendar live or only on Apply. */
        void setUpdateMode(CalendarUpdateMode mode);
        CalendarUpdateMode updateMode() const noexcept;

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

        /**
         * @brief Navigate the popup calendar to the month relevant to the current input state.
         *
         * Activation/SingleSelection -> the activated/selected date's month. RangeSelection ->
         * the range's last (to) date's month. MultipleSelection -> the last (latest) selected
         * date's month. Falls back to Calendar::showToday() when there is nothing selected yet.
         * Called on CalendarDropdown::aboutToShow, i.e. every time the popup is about to open.
         */
        void navigateToRelevantMonth();

        /** @brief True while calendar picks should land in the input immediately: update mode
         *  is Auto, or (regardless of update mode) the calendar's configured mode is
         *  Activation -- see CalendarUpdateMode::Explicit. */
        bool isLiveUpdating() const;

        /** @brief Pull the calendar's current selection into the input: updateText() plus
         *  re-emitting whichever of selectionChanged()/rangeSelected()/selectedDatesChanged()
         *  applies to the calendar's effectiveMode(). Called live when isLiveUpdating(), or once
         *  on CalendarDropdown::applied() otherwise. */
        void commitFromCalendar();

        CalendarDropdown* m_dropdown=nullptr;
        PushButton* m_pickerButton=nullptr;
        QString m_displayFormat;
        QDate m_lastActivated;   //!< Activation mode keeps no state of its own -- this does
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CALENDARINPUT_HPP
