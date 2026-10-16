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

/** @file uise/desktop/calendardialog.hpp
*
*  Declares AbstractCalendarDialog and CalendarDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CALENDAR_DIALOG_HPP
#define UISE_DESKTOP_CALENDAR_DIALOG_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/calendar.hpp>
#include <uise/desktop/dialog.hpp>

class QMouseEvent;
class QKeyEvent;
class QShowEvent;
class QHideEvent;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class CalendarDialog_p;

/**
 * @brief How CalendarDialog offers a way to close itself, on top of Calendar's own internal
 *  prev/next/clear/day-cell controls (always present, unaffected by this setting either way)
 *  and the dialog's title-bar close button / Escape (also always available either way).
 */
enum class CalendarDialogCloseMode
{
    //! A single Apply-styled button (a checkmark, like CalendarDropdown's own Apply button,
    //! not AbstractDialog's generic buttons) is shown below the calendar, regardless of
    //! Calendar's current selection mode or selection state. This is the default: picking a
    //! date never closes the dialog on its own, only that button does.
    ExplicitButton,

    //! That button is hidden; the dialog closes itself as soon as a selection settles, mirroring
    //! CalendarDropdown's own auto-close: in Activation mode, activating a date closes it; in
    //! SingleSelection, so does picking a date; in RangeSelection, so does completing the range.
    //! MultipleSelection has no single "done" moment, so it never auto-closes in this mode.
    AutoCloseOnSelection
};

/**
 * @brief Interface of a dialog hosting a Calendar, with an optional inactivity auto-close.
 *
 * The auto-close is driven entirely by Calendar::activity() (plus this dialog's own chrome --
 * see CalendarDialog): it is host-neutral, so the same behaviour works whether this dialog is
 * shown via FloatingDialog<>, ModalDialog<>, or a bare FloatingDialogFrame -- all of them treat
 * closeDialog()/closeRequested() as the close signal already.
 */
class UISE_DESKTOP_EXPORT AbstractCalendarDialog : public AbstractDialog
{
    Q_OBJECT

    public:

        using AbstractDialog::AbstractDialog;

        //! Default inactivity period before the dialog closes itself.
        constexpr static const int DefaultAutoCloseDelayMs=30000;

        virtual Calendar* calendar() const=0;

        /**
         * @brief Set the inactivity period before the dialog closes itself.
         * @param ms Inactivity period in milliseconds; 0 or negative disables auto-close.
         */
        virtual void setAutoCloseDelayMs(int ms)=0;
        virtual int autoCloseDelayMs() const noexcept=0;

        virtual void setAutoCloseEnabled(bool enable)=0;
        virtual bool isAutoCloseEnabled() const noexcept=0;

        //! Default CalendarDialogCloseMode::ExplicitButton.
        virtual void setCloseMode(CalendarDialogCloseMode mode)=0;
        virtual CalendarDialogCloseMode closeMode() const noexcept=0;

    signals:

        /** @brief Emitted immediately BEFORE closeRequested() when the close was caused by the
         *  inactivity timeout, letting an embedder tell a timeout apart from a user close. */
        void autoClosed();

        /** @brief Emitted when the user presses the Apply button, immediately BEFORE the dialog
         *  closes itself. Apply is the only close path that means "I confirm the selection": the
         *  title-bar X, Escape, an outside click and the inactivity timeout all close without it,
         *  so an embedder that must act only on a confirmed selection listens to this, not to
         *  closeRequested(). Not emitted in CalendarDialogCloseMode::AutoCloseOnSelection, where the
         *  Apply button is hidden. */
        void applied();

    public slots:

        /** @brief Restart the inactivity countdown. */
        virtual void notifyActivity()=0;

    public:

        // Declared after everything else on purpose: a new virtual appended last keeps the vtable
        // slots of the existing ones where a stale build of a dependent target expects them.

        /**
         * @brief Enable or disable the Apply button (enabled by default).
         *
         * Lets an embedder with a mode that needs a selection first -- e.g. RangeSelection -- keep
         * Apply inert until there is one.
         */
        virtual void setApplyEnabled(bool enable)=0;
        virtual bool isApplyEnabled() const noexcept=0;

        /**
         * @brief Put a widget of the embedder's own (a tab bar, a hint) above the calendar.
         *
         * The dialog takes ownership; a previous header widget is deleted. nullptr removes it.
         * Prefer this to Dialog<>::setTitleControl() when the widget should span the dialog's
         * width: the title label keeps the stretch there and pushes a title control aside.
         */
        virtual void setHeaderWidget(QWidget* widget)=0;
        virtual QWidget* headerWidget() const noexcept=0;
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4661)
#endif

/**
 * @brief Dialog hosting a Calendar, closing itself after a configurable period of inactivity.
 *
 * Activity is anything Calendar::activity() reports (day clicks/drags, prev/next/clear, the
 * header, wheel/keys, and opening/closing the month picker or the multiple-selection dates
 * dropdown) plus a press anywhere on the dialog itself, which also covers a drag by its title
 * bar when hosted in a FloatingDialogFrame. Bare mouse movement/hover is deliberately not
 * activity.
 *
 * Hosted as `FloatingDialog<AbstractCalendarDialog,CalendarDialog>`, this becomes a floating,
 * auto-closing calendar popup; the same class works unchanged under `ModalDialog<>`.
 *
 * AbstractDialog's own generic button row is never used (setButtons({}) always) -- instead a
 * single Apply-styled button below the calendar, built the same way as CalendarDropdown's own
 * apply button, is shown or hidden per closeMode() -- see CalendarDialogCloseMode.
 *
 * No selection accessors are duplicated here -- use calendar() for selectedDate()/rangeFrom()/
 * rangeTo()/selectedDates() and every Calendar signal. A visible countdown indicator is out of
 * scope; it can be added later via Dialog<>::setTitleControl() without changing this API.
 */
class UISE_DESKTOP_EXPORT CalendarDialog : public Dialog<AbstractCalendarDialog>
{
    Q_OBJECT

    public:

        using Base=Dialog<AbstractCalendarDialog>;

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        explicit CalendarDialog(QWidget* parent=nullptr);

        /**
         * @brief Constructor.
         * @param mode Calendar interaction mode.
         * @param parent Parent widget.
         */
        CalendarDialog(CalendarMode mode, QWidget* parent=nullptr);

        ~CalendarDialog();

        CalendarDialog(const CalendarDialog&)=delete;
        CalendarDialog(CalendarDialog&&)=delete;
        CalendarDialog& operator=(const CalendarDialog&)=delete;
        CalendarDialog& operator=(CalendarDialog&&)=delete;

        Calendar* calendar() const override;

        void setAutoCloseDelayMs(int ms) override;
        int autoCloseDelayMs() const noexcept override;

        void setAutoCloseEnabled(bool enable) override;
        bool isAutoCloseEnabled() const noexcept override;

        void setCloseMode(CalendarDialogCloseMode mode) override;
        CalendarDialogCloseMode closeMode() const noexcept override;

        void notifyActivity() override;

        void setApplyEnabled(bool enable) override;
        bool isApplyEnabled() const noexcept override;

        /**
         * @brief Keep the Apply row's height while it is hidden (off by default).
         *
         * The row is hidden in CalendarDialogCloseMode::AutoCloseOnSelection, so the dialog is one row
         * shorter than in ExplicitButton mode. An embedder that switches between the two modes while the
         * dialog is on screen (tabs) reserves the space, otherwise the dialog changes height on every
         * switch and the content is laid out twice, visibly. Not virtual: a plain look setting of this
         * implementation.
         */
        void setApplyRowRetainsSpace(bool enable);

        void setHeaderWidget(QWidget* widget) override;
        QWidget* headerWidget() const noexcept override;

        void setDialogFocus() override;

        //! Always false -- the calendar's own content has no meaningful size beyond its
        //! natural one, so a host (e.g. FloatingDialogFrame) should never offer mouse resize.
        bool isResizable() const override;

    protected:

        void mousePressEvent(QMouseEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;

    private:

        void constructCalendar(CalendarMode mode);
        void applyCloseMode();
        void armAutoClose();
        void onAutoCloseTimeout();

        std::unique_ptr<CalendarDialog_p> pimpl;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}

#endif // UISE_DESKTOP_CALENDAR_DIALOG_HPP
