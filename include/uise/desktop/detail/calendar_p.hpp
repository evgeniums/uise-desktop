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

/** @file uise/desktop/detail/calendar_p.hpp
*
*  Contains declaration of Calendar_p and the internal helper widgets it uses -- the clickable
*  header frame/title, and the multiple-selection dates-management dropdown and its rows.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DETAIL_CALENDAR_P_HPP
#define UISE_DESKTOP_DETAIL_CALENDAR_P_HPP

#include <array>
#include <set>

#include <QDate>
#include <QLocale>
#include <QList>
#include <QPointer>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/dropdownframe.hpp>
#include <uise/desktop/calendar.hpp>

class QLabel;
class QVBoxLayout;
class QGridLayout;
class QMouseEvent;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class Label;
class ElidedLabel;
class PushButton;
class DateTimePickerDropdown;
class ScrollArea;
class RippleOverlay;

/**
 * @brief Small frame wrapping a single child widget that unconditionally accepts and reports
 *  its own mouse press.
 *
 * Used for the header title area: it never relies on propagation to a parent, so the owner
 * decides per-click (based on the calendar's current effective mode) whether that means "open
 * the month picker" or "open the multiple-selection dates dropdown" -- see calendar.cpp.
 */
class ClickableFrame : public Frame
{
    Q_OBJECT

    public:

        using Frame::Frame;

    signals:

        void clicked();

    protected:

        void mousePressEvent(QMouseEvent* event) override;
};

/**
 * @brief Clickable header frame that opens the month picker.
 *
 * A press that lands on (or inside) one of the registered non-trigger widgets -- the prev/next
 * chevrons, the clear button, the range "from"/"to" chips, and the title frame -- is ignored
 * here, so their own click handling is never shadowed by the month-picker dropdown. This is
 * needed because PushButton is a QFrame wrapping a QPushButton: a press on the inner button
 * never reaches this frame (QAbstractButton accepts it), but a press in the PushButton frame's
 * own padding does, since a plain QFrame does not accept mouse presses on its own -- and
 * ClickableFrame (the title) always actively handles its own press regardless, so it is listed
 * here purely to stop this frame from ALSO reacting once ClickableFrame already has.
 *
 * A plain (non-text-interactive) child correctly falls through to here when it doesn't handle a
 * press -- the one thing that does NOT fall through is a Label/QLabel with a text-interaction
 * flag set (Qt::TextSelectableByMouse, the Label default), since QLabel then actively consumes
 * the press itself for text selection. The weekday-name and month-caption labels in this header
 * are decorative only, so buildHeader() clears their interaction flags back to
 * Qt::NoTextInteraction rather than adding them here as non-trigger widgets.
 */
class CalendarHeaderFrame : public Frame
{
    Q_OBJECT

    public:

        using Frame::Frame;

        void addNonTriggerWidget(QWidget* widget)
        {
            m_nonTrigger.append(QPointer<QWidget>{widget});
        }

    signals:

        void clicked();

    protected:

        void mousePressEvent(QMouseEvent* event) override;

    private:

        QList<QPointer<QWidget>> m_nonTrigger;
};

/**
 * @brief One row of the multiple-selection dates-management dropdown.
 *
 * A date label plus a trailing icon-only remove button that is only ever visible while the row
 * is hovered (toggled directly via setVisible(), not through QSS alone -- Qt stylesheets have
 * no reliable "display:none").
 */
class CalendarDateRow : public Frame
{
    Q_OBJECT

    public:

        CalendarDateRow(const QDate& date, const QString& text, QWidget* parent=nullptr);

        QDate date() const noexcept
        {
            return m_date;
        }

    signals:

        void clicked(const QDate& date);
        void removeRequested(const QDate& date);

    protected:

        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;

    private:

        QDate m_date;
        Label* m_label=nullptr;
        PushButton* m_removeButton=nullptr;
        RippleOverlay* m_ripple=nullptr;
};

/**
 * @brief Dropdown listing every date of a MultipleSelection selection, opened from the header
 *  title.
 *
 * Unlike CalendarDropdown/DateTimePickerDropdown, this dropdown's row set is not itself the
 * live state being edited -- it is a view over Calendar::selectedDates() that must reflect
 * additions/removals made elsewhere (e.g. a Ctrl+click on the grid) each time it opens, and
 * live while open. It therefore DOES override fillContent()/clearContent(), matching
 * DropdownMenu's pattern instead of DateTimePickerDropdown's.
 */
class CalendarDatesDropdown : public DropdownFrame
{
    Q_OBJECT

    public:

        explicit CalendarDatesDropdown(Calendar* owner, QWidget* parent=nullptr);

        /** @brief Rebuild the title and every row from owner()->selectedDates(), WITHOUT
         *  touching the scroll area's size. Safe to call while open -- this is the path used for
         *  live updates (e.g. a row's own remove button), since DropdownFrame sizes content once
         *  per opening and does not expect it to resize itself afterwards (see
         *  DropdownFrame::fullSize() docs). */
        void rebuildRows();

        /** @brief rebuildRows() plus (re)computing the scroll area's height cap. Only valid
         *  before/during the initial measurement of a fresh opening -- see fillContent(). */
        void refreshRows();

    protected:

        void fillContent() override;
        void clearContent() override;

    private:

        void updateMaxHeight();

        Calendar* m_owner=nullptr;
        Label* m_titleLabel=nullptr;
        QFrame* m_separator=nullptr;
        ScrollArea* m_scrollArea=nullptr;
        QWidget* m_listFrame=nullptr;
        QVBoxLayout* m_listLayout=nullptr;
};

class Calendar_p
{
    public:

        Calendar* self=nullptr;

        // configuration
        CalendarMode mode=CalendarMode::Activation;
        CalendarMode effective=CalendarMode::Activation;
        CalendarWeekStart weekStart=CalendarWeekStart::Locale;
        QLocale locale;
        QDate minDate;                                  // invalid == unbounded
        QDate maxDate;
        QDate currentDate;
        bool headerClickable=true;

        // state
        QDate displayed;                                // always day==1
        QDate single;
        QDate rangeFrom;
        QDate rangeTo;
        QDate rangeAnchor;
        bool rangePending=false;                        // first click done, waiting for second
        std::set<QDate> multiple;
        QDate dragAnchor;                               // cell a drag gesture started on
        bool dragActive=false;                          // a real (cross-cell) drag is underway

        // metrics (QSS-tunable)
        int cellWidth=34;
        int cellHeight=34;
        int weekDayRowHeight=22;
        int monthLabelHeight=18;
        int headerSpacing=4;
        int maxTitleDates=0;

        // widgets
        CalendarHeaderFrame* header=nullptr;
        QFrame* headerTop=nullptr;
        QFrame* headerTopLeft=nullptr;
        QFrame* headerTopRight=nullptr;
        ClickableFrame* titleFrame=nullptr;
        ElidedLabel* titleLabel=nullptr;
        PushButton* rangeFromButton=nullptr;
        Label* rangeDashLabel=nullptr;
        PushButton* rangeToButton=nullptr;
        PushButton* clearButton=nullptr;
        PushButton* prevButton=nullptr;
        PushButton* nextButton=nullptr;
        QFrame* weekDaysFrame=nullptr;
        std::array<Label*,Calendar::GridColumns> weekDayLabels{};
        Label* monthLabel=nullptr;
        QFrame* daysFrame=nullptr;
        std::array<CalendarDay*,Calendar::GridRows*Calendar::GridColumns> cells{};
        DateTimePickerDropdown* monthDropdown=nullptr;
        CalendarDatesDropdown* datesDropdown=nullptr;

        // wheel-to-scroll-months support: raw angleDelta().y() accumulated until it reaches a
        // full physical notch (120 units), so light/fractional trackpad scrolling doesn't step
        // through several months at once -- see Calendar::wheelEvent()
        float wheelAccumulated=0.0f;

        // helpers
        Qt::DayOfWeek resolvedFirstDayOfWeek() const;
        bool withinLimits(const QDate& date) const;
        QDate clampMonth(const QDate& month) const;
        void activity() const { self->notifyActivity(); }   //!< shorthand used by every wiring site

        void buildHeader();
        void buildGrid();
        void applyMetrics();          //!< push QSS metrics onto the child widgets
        void rebuildWeekDays();       //!< weekday captions + weekend flags
        void rebuildPage();           //!< full re-date of all 42 cells + restyle
        void restyleSelection();      //!< cheap path: only band/marked properties
        void updateTitle();
        void updateRangeChips();
        void updateMonthLabel();
        void updateNavButtons();
        void updateClearButton();
        void applyLimits();           //!< prune selection, sync month picker range, nav buttons
        void setEffectiveMode(CalendarMode m);
        void onDayClicked(const QDate& date, Qt::KeyboardModifiers modifiers);
        void toggleMultiple(const QDate& date);   //!< toggle date in the multiple set + notify
        void clickRange(const QDate& date);       //!< first/second click of a range gesture
        void emitSelectionChanged();  //!< also reverts Auto mode to Activation when now empty
        QString shortDate(const QDate& d) const;

        // range drag-selection: press starts a potential drag, move grows a live preview, and
        // release either commits it (if it was a real cross-cell drag) or defers to the normal
        // click flow (onDayClicked(), via CalendarDay's separate clicked() signal)
        bool isRangeDragEligible() const;
        void onDayDragStarted(const QDate& date, Qt::KeyboardModifiers modifiers);
        void onDayDragMoved(const QDate& date);
        void onDayDragFinished(const QDate& date, Qt::KeyboardModifiers modifiers, bool dragged);
};

}

#endif // UISE_DESKTOP_DETAIL_CALENDAR_P_HPP
