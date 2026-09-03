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

/** @file uise/desktop/calendar.hpp
*
*  Declares Calendar, a month-grid calendar widget in the style of Telegram Desktop's date
*  picker, and its CalendarDay grid cell.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CALENDAR_HPP
#define UISE_DESKTOP_CALENDAR_HPP

#include <memory>

#include <QDate>
#include <QList>
#include <QLocale>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>

class QLabel;
class QMouseEvent;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class Calendar_p;
class RippleOverlay;

/**
 * @brief Interaction mode of a Calendar.
 */
enum class CalendarMode
{
    Activation,         //!< a click just activates a date, nothing is kept selected
    SingleSelection,    //!< exactly one date stays selected
    RangeSelection,     //!< a contiguous [from,to] range stays selected
    MultipleSelection,  //!< an arbitrary set of dates stays selected
    Auto,               //!< starts as Activation, escalates on Ctrl/Cmd+click and Shift+click
    ExtendedSelection   //!< a plain click always (re)selects exactly one date -- the same
                         //!< SingleSelection behaviour as above -- Ctrl/Cmd+click adds/removes
                         //!< that date from a MultipleSelection-style set instead, and
                         //!< Shift+click (or a click-drag) replaces the whole selection with a
                         //!< RangeSelection-style range from the last-clicked date. Unlike Auto,
                         //!< which starts empty and only ever grows more permissive, a plain
                         //!< click always collapses back down to a single date, so effectiveMode()
                         //!< genuinely cycles between SingleSelection/RangeSelection/
                         //!< MultipleSelection as the user works, never Activation
};

/**
 * @brief First column of a Calendar's days grid / weekday row.
 */
enum class CalendarWeekStart
{
    Locale,   //!< QLocale::firstDayOfWeek() of the widget's current locale (default)
    Monday,
    Sunday
};

/**
 * @brief One cell of a Calendar's days grid.
 *
 * Exported so QSS can target `uise--CalendarDay`. Owns a single centred QLabel child named
 * `dayLabel`; the OUTER frame paints the continuous range band (properties `band`/`bandEdge`)
 * while the INNER label paints the point marker (property `marked`) inset from the cell, which
 * is what makes a range read as solid and a multiple selection read as separate dots.
 *
 * Cells are created once by Calendar and re-dated on every page change -- never recreated.
 */
class UISE_DESKTOP_EXPORT CalendarDay : public Frame
{
    Q_OBJECT

    public:

        enum class Marked { None, Point, Endpoint };
        enum class BandEdge { None, Left, Right, Both };

        explicit CalendarDay(QWidget* parent=nullptr);

        CalendarDay(const CalendarDay&)=delete;
        CalendarDay(CalendarDay&&)=delete;
        CalendarDay& operator=(const CalendarDay&)=delete;
        CalendarDay& operator=(CalendarDay&&)=delete;

        /** @brief Set the date this cell shows; updates the label text to date.day(). */
        void setDate(const QDate& date);
        QDate date() const noexcept;

        /** @brief Cell belongs to the previous/next month -- shown, dimmed, never clickable. */
        void setAdjacent(bool enable) noexcept;
        bool isAdjacent() const noexcept;

        /** @brief Date is outside the calendar's [minimumDate,maximumDate] -- shown, disabled. */
        void setOutOfRange(bool enable) noexcept;
        bool isOutOfRange() const noexcept;

        void setToday(bool enable) noexcept;
        bool isToday() const noexcept;

        void setWeekend(bool enable) noexcept;
        bool isWeekend() const noexcept;

        void setMarked(Marked marked) noexcept;
        Marked marked() const noexcept;

        void setBand(bool enable, BandEdge edge) noexcept;
        bool isBand() const noexcept;
        BandEdge bandEdge() const noexcept;

        /** @brief True when the cell can be hovered/clicked: not adjacent and not out of range. */
        bool isSelectable() const noexcept;

        /**
         * @brief Push all pending state to dynamic properties and repolish, once.
         *
         * No-op when nothing changed since the last call -- this is what keeps the
         * "restyle only" path cheap when only the selection changed.
         */
        void applyState();

        QLabel* dayLabel() const noexcept;

        /** @brief The click-ripple overlay installed on dayLabel(), see RippleOverlay. */
        RippleOverlay* rippleOverlay() const noexcept
        {
            return m_ripple;
        }

    signals:

        /** @brief Plain press+release on this cell with no drag in between. */
        void clicked(const QDate& date, Qt::KeyboardModifiers modifiers);

        void hovered(const QDate& date, bool enter);

        /** @brief The mouse was pressed down on this cell -- the possible start of a drag. */
        void dragStarted(const QDate& date, Qt::KeyboardModifiers modifiers);

        /** @brief A drag that started on some cell (not necessarily this one) has moved onto
         *  this cell. Emitted by whichever cell most recently received the press, tracking the
         *  cursor via its own mouseMoveEvent (Qt implicitly grabs the mouse to the pressed
         *  widget for the whole gesture). */
        void dragMoved(const QDate& date);

        /** @brief The mouse was released after a press on this cell.
         * @param date The cell actually under the cursor at release (may differ from date()).
         * @param dragged True if the cursor crossed into a different cell before release. */
        void dragFinished(const QDate& date, Qt::KeyboardModifiers modifiers, bool dragged);

    protected:

        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;

    private:

        QLabel* m_label=nullptr;
        RippleOverlay* m_ripple=nullptr;
        QDate m_date;
        bool m_adjacent=false;
        bool m_outOfRange=false;
        bool m_today=false;
        bool m_weekend=false;
        bool m_hovered=false;
        bool m_band=false;
        BandEdge m_bandEdge=BandEdge::None;
        Marked m_marked=Marked::None;
        bool m_dirty=true;
        bool m_pressed=false;
        bool m_dragged=false;
        QDate m_lastDragDate;
};

/**
 * @brief Month-grid calendar widget, in the style of Telegram Desktop's date picker.
 *
 * A header (title + prev/next month chevrons + weekday row + month caption) over a fixed
 * 6x7 grid of CalendarDay cells. Supports date activation and single / range / multiple date
 * selection, plus an Auto mode that escalates from activation to multiple selection on
 * Ctrl/Cmd+click and to range selection on Shift+click, and falls back to activation as soon
 * as the selection empties, and an ExtendedSelection mode that behaves like SingleSelection for
 * a plain click but layers Ctrl/Cmd+click (add/remove a date) and Shift+click or a click-drag
 * (replace the selection with a range) on top of it (see setMode()).
 *
 * Clicking the header opens a MonthPicker wheel in a dropdown, so navigation is never more
 * than one gesture away from any month within [minimumDate(), maximumDate()]. In SingleSelection
 * mode, once a date is selected, clicking the header title jumps the grid straight to that
 * date's month instead of opening the picker. In RangeSelection mode, once a range exists, the
 * header instead shows two independently clickable "from"/"to" controls that jump the grid
 * straight to their own month. In MultipleSelection mode, the header's elided date list opens a
 * scrollable management dropdown listing every selected date.
 */
class UISE_DESKTOP_EXPORT Calendar : public Frame
{
    Q_OBJECT

    Q_PROPERTY(int cellWidth READ cellWidth WRITE setCellWidth)
    Q_PROPERTY(int cellHeight READ cellHeight WRITE setCellHeight)
    Q_PROPERTY(int weekDayRowHeight READ weekDayRowHeight WRITE setWeekDayRowHeight)
    Q_PROPERTY(int monthLabelHeight READ monthLabelHeight WRITE setMonthLabelHeight)
    Q_PROPERTY(int headerSpacing READ headerSpacing WRITE setHeaderSpacing)
    Q_PROPERTY(int maxTitleDates READ maxTitleDates WRITE setMaxTitleDates)

    public:

        constexpr static const int GridRows=6;
        constexpr static const int GridColumns=7;

        explicit Calendar(QWidget* parent=nullptr);
        explicit Calendar(CalendarMode mode, QWidget* parent=nullptr);

        ~Calendar();

        Calendar(const Calendar&)=delete;
        Calendar(Calendar&&)=delete;
        Calendar& operator=(const Calendar&)=delete;
        Calendar& operator=(Calendar&&)=delete;

        /**
         * @brief Set configured interaction mode.
         * @param mode Mode. CalendarMode::Auto keeps mode()==Auto forever while
         *  effectiveMode() flips between Activation, MultipleSelection and RangeSelection at
         *  runtime; every other value pins effectiveMode() to itself.
         *
         * Switching mode clears the current selection.
         */
        void setMode(CalendarMode mode);
        CalendarMode mode() const noexcept;

        /** @brief Mode actually in force right now. Never returns CalendarMode::Auto or
         *  CalendarMode::ExtendedSelection -- both report one of Activation/SingleSelection/
         *  RangeSelection/MultipleSelection here depending on live state. */
        CalendarMode effectiveMode() const noexcept;

        void setWeekStart(CalendarWeekStart value);
        CalendarWeekStart weekStart() const noexcept;

        /** @brief Resolved first day of week -- weekStart(), or locale().firstDayOfWeek(). */
        Qt::DayOfWeek firstDayOfWeek() const;

        /**
         * @brief Set allowed date limits, inclusive.
         * @param min Minimum date; an invalid QDate means unbounded.
         * @param max Maximum date; an invalid QDate means unbounded. Default currentDate().
         *
         * Days outside the limits are still shown but disabled. Navigation is capped: the
         * next-month chevron is disabled on the month containing max, the previous-month
         * chevron on the month containing min, and the month-picker dropdown is range-limited
         * to the same interval. Any part of the current selection that falls outside the new
         * limits is dropped (a range is clamped), emitting selectionChanged().
         */
        void setDateRange(const QDate& min, const QDate& max);
        void setMinimumDate(const QDate& date);
        void setMaximumDate(const QDate& date);
        QDate minimumDate() const noexcept;
        QDate maximumDate() const noexcept;

        /** @brief First day of the month currently shown in the grid. */
        QDate displayedMonth() const noexcept;

        /**
         * @brief Set the date the widget treats as "today".
         * @param date Date to treat as today. Defaults to QDate::currentDate().
         *
         * Drives the today highlight, showToday(), and the initial displayedMonth()/
         * maximumDate() at construction time. Exposed as an explicit setter (rather than always
         * reading the wall clock) so embedders can pin it for tests, screenshots, or a
         * simulated-clock app. Changing it does not retroactively move an already-set
         * maximumDate() -- only the today highlight and showToday() target move immediately.
         */
        void setCurrentDate(const QDate& date);
        QDate currentDate() const noexcept;

        QDate selectedDate() const noexcept;      //!< SingleSelection; invalid when unset
        QDate rangeFrom() const noexcept;         //!< RangeSelection
        QDate rangeTo() const noexcept;           //!< RangeSelection
        QList<QDate> selectedDates() const;       //!< MultipleSelection, ascending
        bool hasSelection() const noexcept;

        void setSelectedDate(const QDate& date);
        void setSelectedRange(const QDate& from, const QDate& to);
        void setSelectedDates(const QList<QDate>& dates);

        /** @brief Set locale used for month/day names, short date form and first day of week. */
        void setLocale(const QLocale& locale);

        /** @brief Enable/disable the header's month-picker dropdown. Default true. */
        void setHeaderClickable(bool enable);
        bool isHeaderClickable() const noexcept;

        /** @brief Current header title text, e.g. "August 2026" or "Select day". Empty while
         *  the header shows the range chips or the multiple-selection list instead. */
        QString title() const;

        CalendarDay* dayCell(const QDate& date) const;   //!< nullptr when not on the current page
        CalendarDay* dayCell(int row, int column) const;
        QWidget* headerFrame() const noexcept;
        QWidget* daysFrame() const noexcept;

        /** @brief True while one of the calendar's own top-level popups (the month picker or
         *  the multiple-selection dates dropdown) is open. Interaction inside those popups
         *  never reaches this widget's event stream, so an embedder running an inactivity
         *  countdown must treat an open popup as "busy" -- see CalendarDialog. */
        bool isPopupOpen() const noexcept;

        void setCellWidth(int val) noexcept;
        int cellWidth() const noexcept;

        void setCellHeight(int val) noexcept;
        int cellHeight() const noexcept;

        void setWeekDayRowHeight(int val) noexcept;
        int weekDayRowHeight() const noexcept;

        void setMonthLabelHeight(int val) noexcept;
        int monthLabelHeight() const noexcept;

        void setHeaderSpacing(int val) noexcept;
        int headerSpacing() const noexcept;

        /** @brief Max dates listed in the multiple-selection title before ", ...". 0 = unlimited. */
        void setMaxTitleDates(int val) noexcept;
        int maxTitleDates() const noexcept;

    public slots:

        void setDisplayedMonth(const QDate& month);   //!< clamped into the date limits
        void showPreviousMonth();
        void showNextMonth();
        void showToday();      //!< navigates to the month of currentDate(), clamped into the limits
        void clearSelection();
        void openMonthPicker();
        void closeMonthPicker();

        /** @brief Emit activity(). Public so an embedder can fold external interaction into
         *  the calendar's own activity stream (mirrors ImageViewerWidget::notifyActivity()). */
        void notifyActivity();

    signals:

        /** @brief A selectable day was clicked while effectiveMode()==Activation. */
        void dateActivated(const QDate& date);

        /** @brief Any click on a selectable day, before mode-specific handling. */
        void dateClicked(const QDate& date, Qt::KeyboardModifiers modifiers);

        /** @brief The selection changed in any mode, including becoming empty. */
        void selectionChanged();

        /** @brief A complete range was settled (second click of a range gesture). */
        void rangeSelected(const QDate& from, const QDate& to);

        /** @brief The multiple-selection set changed. */
        void selectedDatesChanged(const QList<QDate>& dates);

        /** @brief The selection became empty. */
        void selectionCleared();

        /** @brief The displayed page (month) changed. */
        void displayedMonthChanged(const QDate& month);

        /** @brief effectiveMode() changed -- only ever emitted when mode() is Auto or
         *  ExtendedSelection, the two modes where effectiveMode() can differ from mode(). */
        void effectiveModeChanged(CalendarMode mode);

        /** @brief Any user interaction: a click on any control or day cell, a drag, a wheel
         *  step, a key press, or opening/closing one of the popups. Never emitted for
         *  programmatic changes (setSelectedDate(), setDisplayedMonth(), ...). */
        void activity();

    protected:

        void changeEvent(QEvent* event) override;

        /** @brief Mouse wheel over the widget steps to the previous/next month. */
        void wheelEvent(QWheelEvent* event) override;

        /** @brief PageUp/ArrowUp step to the previous month, PageDown/ArrowDown to the next. */
        void keyPressEvent(QKeyEvent* event) override;

        /** @brief Catch-all: a press landing on the calendar's own background (or on a child
         *  that ignored it) still counts as activity. Never consumes the event -- do not
         *  "optimise" this into event->accept(), embedders such as CalendarDropdown rely on it
         *  propagating. */
        void mousePressEvent(QMouseEvent* event) override;

    private:

        void construct(CalendarMode mode);

        std::unique_ptr<Calendar_p> pimpl;
};

}

#endif // UISE_DESKTOP_CALENDAR_HPP
