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

/** @file uise/desktop/datetimeinput.hpp
*
*  Declares DateTimePickerDropdown, DateTimeInput, DateInput, MonthInput and TimeInput.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DATETIMEINPUT_HPP
#define UISE_DESKTOP_DATETIMEINPUT_HPP

#include <QDate>
#include <QTime>
#include <QDateTime>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/dropdownframe.hpp>
#include <uise/desktop/lineedit.hpp>
#include <uise/desktop/datetimepicker.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class PushButton;

/**
 * @brief Animated dropdown that hosts a DateTimePicker, with an optional OK/Cancel buttons row.
 *
 * The picker is created once and kept alive across openings -- unlike DropdownMenu, this frame
 * never rebuilds its content in fillContent(), since the wheels' scroll position is itself the
 * live state being edited (see DropdownFrame::fillContent() docs on externally/persistently
 * owned content).
 */
class UISE_DESKTOP_EXPORT DateTimePickerDropdown : public DropdownFrame
{
    Q_OBJECT

    public:

        explicit DateTimePickerDropdown(DateTimeFields fields, QWidget* parent=nullptr);

        DateTimePicker* picker() const noexcept
        {
            return m_picker;
        }

        /**
         * @brief Show/hide the Apply/Cancel buttons row.
         * @param enable Default false -- the picker applies live, which is what an embedding
         *  EditableLabel row wants (it already owns apply/cancel for the whole row). Standalone
         *  use of DateTimeInput typically wants this enabled.
         */
        void setButtonsVisible(bool enable);

        bool isButtonsVisible() const noexcept
        {
            return m_buttonsVisible;
        }

    signals:

        void applied();
        void cancelled();

    private:

        DateTimePicker* m_picker=nullptr;
        QFrame* m_buttonsFrame=nullptr;
        PushButton* m_applyButton=nullptr;
        PushButton* m_cancelButton=nullptr;
        bool m_buttonsVisible=false;
        QDateTime m_snapshot;
};

/**
 * @brief Read-only line-edit style field that opens a DateTimePickerDropdown on click.
 *
 * API mirrors QDateEdit (date()/setDate()/dateChanged()) closely enough to be a near-drop-in
 * replacement in code written against it -- see EditableLabelTraits<EditableLabel::Type::Date>
 * and friends in editablelabel.hpp.
 */
class UISE_DESKTOP_EXPORT DateTimeInput : public LineEdit
{
    Q_OBJECT

    public:

        explicit DateTimeInput(QWidget* parent=nullptr);
        explicit DateTimeInput(DateTimeFields fields, QWidget* parent=nullptr);

        ~DateTimeInput();

        QDateTime dateTime() const;
        QDate date() const;
        QTime time() const;

        /**
         * @brief Set display text format.
         * @param format Qt date/time format string (see QLocale::toString()).
         *
         * Default (empty format) derives the format from the current locale and fields(),
         * using dateAsMonthAndYear() when fields() is DateTimeField::YearMonth.
         */
        void setDisplayFormat(const QString& format);

        QString displayFormat() const;

        DateTimePicker* picker() const noexcept;

        void setDateRange(const QDate& min, const QDate& max);

        /**
         * @brief Show/hide the popup's Apply/Cancel buttons row. See
         *  DateTimePickerDropdown::setButtonsVisible().
         */
        void setButtonsVisible(bool enable);

    public slots:

        void setDateTime(const QDateTime& value);
        void setDate(const QDate& value);
        void setTime(const QTime& value);

        void openPopup();
        void closePopup();

    signals:

        void dateTimeChanged(const QDateTime& value);
        void dateChanged(const QDate& value);
        void timeChanged(const QTime& value);

    protected:

        void mousePressEvent(QMouseEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;

    private:

        void construct(DateTimeFields fields);
        void updateText();

        DateTimePickerDropdown* m_dropdown=nullptr;
        PushButton* m_pickerButton=nullptr;
        QString m_displayFormat;
};

/**
 * @brief DateTimeInput preset to DateTimeField::Date.
 */
class UISE_DESKTOP_EXPORT DateInput : public DateTimeInput
{
    Q_OBJECT

    public:

        explicit DateInput(QWidget* parent=nullptr);
};

/**
 * @brief DateTimeInput preset to DateTimeField::YearMonth -- month-selection mode.
 */
class UISE_DESKTOP_EXPORT MonthInput : public DateTimeInput
{
    Q_OBJECT

    public:

        explicit MonthInput(QWidget* parent=nullptr);
};

/**
 * @brief DateTimeInput preset to DateTimeField::Time.
 */
class UISE_DESKTOP_EXPORT TimeInput : public DateTimeInput
{
    Q_OBJECT

    public:

        explicit TimeInput(QWidget* parent=nullptr);
};

}

#endif // UISE_DESKTOP_DATETIMEINPUT_HPP
