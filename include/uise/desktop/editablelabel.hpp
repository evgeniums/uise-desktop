/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/desktop/editablelabel.hpp
*
*  Declares EditableLabel.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_EDITABLELABEL_HPP
#define UISE_DESKTOP_EDITABLELABEL_HPP

#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QDateEdit>
#include <QTimeEdit>
#include <QDateTimeEdit>
#include <QCoreApplication>
#include <QPushButton>

#include <QBoxLayout>
#include <QEvent>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class EditableLabelFormatter;

/**
 * @brief Base class for editable labels.
 */
class UISE_DESKTOP_EXPORT EditableLabel : public QFrame
{
    Q_OBJECT

    public:

        /**
         * @brief Type of label content.
         */
        enum class Type : int
        {
            Text,
            Int,
            Double,
            List,
            Date,
            Time,
            DateTime,
            Custom
        };

        /**
         * @brief Constructor.
         * @param type Type of label content.
         * @param parent Parent widget.
         * @param inGroup Label is part of a group, see also setInGroup().
         */
        EditableLabel(Type type, QWidget* parent=nullptr, bool inGroup=false);

        /**
         * @brief Get type of label content.
         * @return Query result.
         */
        Type type() const noexcept
        {
            return m_type;
        }

        /**
         * @brief Get label object.
         * @return Query result.
         */
        QLabel* label() const noexcept
        {
            return m_label;
        }

        /**
         * @brief Get label text.
         * @return Query result.
         */
        QString text() const
        {
            return m_label->text();
        }

        /**
         * @brief Get label editor.
         * @return Query result.
         */
        virtual QWidget* editor() const noexcept =0;

        /**
         * @brief Set text formatter.
         * @param formatter Formatter.
         */
        void setFormatter(EditableLabelFormatter* formatter) noexcept
        {
            m_formatter=formatter;
        }
        /**
         * @brief Get text formatter.
         * @return Query result.
         */
        EditableLabelFormatter* formatter() const noexcept
        {
            return m_formatter;
        }

        /**
         * @brief Set group mode.
         * @param val If true then the label is part of a group.
         *
         * When label is in group then editing mode is controlled by that group so that buttons and corresponding events are disabled.
         */
        void setInGroup(bool val)
        {
            m_inGroup=val;
            setEditable(m_editable);
        }
        /**
         * @brief Check if label is in a group.
         * @return Query result.
         */
        bool isInGroup() const noexcept
        {
            return m_inGroup;
        }

        /**
         * @brief Check if label is in editing mode now.
         * @return Query result.
         */
        bool isEditable() const noexcept
        {
            return m_editable;
        }

    public slots:

        /**
         * @brief Set editing mode.
         * @param enable If trure then show editor, potherwise show text label.
         */
        void setEditable(bool enable)
        {
            m_editable=enable;
            m_label->setVisible(!m_inGroup && !enable);

            m_editButton->setVisible(!m_inGroup && !enable);
            m_cancelButton->setVisible(!m_inGroup && enable);
            m_applyButton->setVisible(!m_inGroup && enable);

            editor()->setMinimumWidth(m_label->width());
            editor()->setVisible(enable);
            editor()->setFocus();
        }

        /**
         * @brief Apply editor,
         */
        virtual void apply() = 0;

        /**
         * @brief Cancel editing.
         */
        void cancel()
        {
            setEditable(false);
            restoreWidgetValue();
        }

        /**
         * @brief Enable editing.
         */
        void edit()
        {
            setEditable(true);
        }

    signals:

        /**
         * @brief New value was set for the label.
         */
        void valueSet();

    protected:

        QBoxLayout* boxLayout() const noexcept
        {
            return m_layout;
        }

        virtual void restoreWidgetValue() =0;

        bool eventFilter(QObject *watched, QEvent *event) override;

    private:

        Type m_type;
        QLabel* m_label;
        QBoxLayout* m_layout;
        EditableLabelFormatter* m_formatter;
        bool m_editable;
        bool m_inGroup;

        QPushButton* m_editButton;
        QPushButton* m_applyButton;
        QPushButton* m_cancelButton;
};

/**
 * @brief Base class for formatters of editable label text.
 */
class UISE_DESKTOP_EXPORT EditableLabelFormatter
{
    public:

        //! Destructor.
        virtual ~EditableLabelFormatter() = default;

        EditableLabelFormatter()=default;
        EditableLabelFormatter(const EditableLabelFormatter&)=default;
        EditableLabelFormatter(EditableLabelFormatter&&)=default;
        EditableLabelFormatter& operator = (const EditableLabelFormatter&)=default;
        EditableLabelFormatter& operator = (EditableLabelFormatter&&)=default;

        /**
         * @brief Format text of the label.
         * @param type Type of the label content.
         * @param value Variant value from label editor.
         * @return Formatted text.
         */
        virtual QString format(EditableLabel::Type type, const QVariant& value) =0;

        /**
         * @brief Load lable with formatted text.
         * @param label Label to set text to.
         * @param formatter Formatter for text formatting.
         * @param value Value to format and to to label.
         * @param defaultFormatter Default text formatter.
         */
        template <EditableLabel::Type Type, typename ValueT>
        static void loadLabel(QLabel* label, EditableLabelFormatter* formatter, const ValueT& value, const std::function<QString (const ValueT&)>& defaultFormatter)
        {
            if (formatter==nullptr)
            {
                label->setText(defaultFormatter(value));
            }
            else
            {
                label->setText(formatter->format(Type,value));
            }
        }
};

/**
 * @brief Base template for traits of editable labels.
 */
template <EditableLabel::Type Type>
struct EditableLabelTraits
{
};

/**
 * @brief Traits of editable label of text type.
 */
template <>
struct EditableLabelTraits<EditableLabel::Type::Text>
{
    using type=QLineEdit;

    static void loadLabel(QLabel* label, type* widget, EditableLabelFormatter* formatter=nullptr)
    {
        EditableLabelFormatter::loadLabel<EditableLabel::Type::Text,QString>(label,formatter,widget->text(),[](const QString& val){return val;});
    }

    static auto value(const type* widget)
    {
        return widget->text();
    }

    static void setValue(type* widget, const QString& value)
    {
        widget->setText(value);
    }
};

/**
 * @brief Traits of editable label of int type.
 */
template <>
struct EditableLabelTraits<EditableLabel::Type::Int>
{
    using type=QSpinBox;

    static void loadLabel(QLabel* label, type* widget, EditableLabelFormatter* formatter=nullptr)
    {
        using valueType=decltype(widget->value());

        EditableLabelFormatter::loadLabel<EditableLabel::Type::Int,valueType>(label,formatter,widget->value(),[](const valueType& val){return QString::number(val);});
    }

    static auto value(const type* widget)
    {
        return widget->value();
    }

    static void setValue(type* widget, int value)
    {
        widget->setValue(value);
    }
};

/**
 * @brief Traits of editable label of double type.
 */
template <>
struct EditableLabelTraits<EditableLabel::Type::Double>
{
    using type=QDoubleSpinBox;

    static void loadLabel(QLabel* label, type* widget, EditableLabelFormatter* formatter=nullptr)
    {
        using valueType=decltype(widget->value());

        EditableLabelFormatter::loadLabel<EditableLabel::Type::Double,valueType>(label,formatter,widget->value(),[](const valueType& val){return QString::number(val);});
    }

    static auto value(const type* widget)
    {
        return widget->value();
    }

    static void setValue(type* widget, double value)
    {
        widget->setValue(value);
    }
};

/**
 * @brief Value of editable list.
 */
struct UISE_DESKTOP_EXPORT EditableLabelListValue
{
    int index;
    QString text;
    QVariant data;
};

/**
 * @brief Traits of editable label of list type.
 */
template <>
struct EditableLabelTraits<EditableLabel::Type::List>
{
    using type=QComboBox;

    static void loadLabel(QLabel* label, type* widget, EditableLabelFormatter* formatter=nullptr)
    {
        EditableLabelFormatter::loadLabel<EditableLabel::Type::List,QString>(label,formatter,widget->currentText(),[](const QString& val){return val;});
    }

    static auto value(const type* widget)
    {
        return EditableLabelListValue{widget->currentIndex(),widget->currentText(),widget->currentData()};
    }

    static void setValue(type* widget, int value)
    {
        widget->setCurrentIndex(value);
    }

    static void setValue(type* widget, const QString& value)
    {
        widget->setCurrentText(value);
    }

    static void setValue(type* widget, const EditableLabelListValue& value)
    {
        widget->setCurrentIndex(value.index);
    }
};

/**
 * @brief Traits of editable label of date type.
 */
template <>
struct EditableLabelTraits<EditableLabel::Type::Date>
{
    using type=QDateEdit;

    static void loadLabel(QLabel* label, const type* widget, EditableLabelFormatter* formatter=nullptr)
    {
        using valueType=decltype(widget->date());

        auto dateFormat = QLocale().dateFormat(QLocale::ShortFormat);
        EditableLabelFormatter::loadLabel<EditableLabel::Type::Date,valueType>(label,formatter,widget->date(),[dateFormat](const valueType& val){return val.toString(dateFormat);});
    }

    static auto value(const type* widget)
    {
        return widget->date();
    }

    static void setValue(type* widget, const QDate& value)
    {
        widget->setDate(value);
    }
};

/**
 * @brief Traits of editable label of time type.
 */
template <>
struct EditableLabelTraits<EditableLabel::Type::Time>
{
    using type=QTimeEdit;

    static void loadLabel(QLabel* label, const type* widget, EditableLabelFormatter* formatter=nullptr)
    {
        using valueType=decltype(widget->time());

        EditableLabelFormatter::loadLabel<EditableLabel::Type::Time,valueType>(label,formatter,widget->time(),[](const valueType& val){return val.toString("HH:mm");});
    }

    static auto value(const type* widget)
    {
        return widget->time();
    }

    static void setValue(type* widget, const QTime& value)
    {
        widget->setTime(value);
    }
};

/**
 * @brief Traits of editable label of datetime type.
 */
template <>
struct EditableLabelTraits<EditableLabel::Type::DateTime>
{
    using type=QDateTimeEdit;

    static void loadLabel(QLabel* label, const type* widget, EditableLabelFormatter* formatter=nullptr)
    {
        using valueType=decltype(widget->dateTime());

        auto dateTimeFormat = QLocale().dateTimeFormat(QLocale::ShortFormat);
        EditableLabelFormatter::loadLabel<EditableLabel::Type::DateTime,valueType>(label,formatter,widget->dateTime(),[dateTimeFormat](const valueType& val){return val.toString(dateTimeFormat);});
    }

    static auto value(const type* widget)
    {
        return widget->dateTime();
    }

    static void setValue(type* widget, const QDateTime& value)
    {
        widget->setDateTime(value);
    }
};

namespace detail
{

template <EditableLabel::Type TypeId>
struct EditableLabelHelper
{
    using traits = EditableLabelTraits<TypeId>;
    using type = typename traits::type;

    static auto createWidget(QWidget* parent=nullptr)
    {
        return new type(parent);
    }

    static void loadLabel(QLabel* label, type* widget, EditableLabelFormatter* formatter=nullptr)
    {
        traits::loadLabel(label, widget, formatter);
    }

    static auto value(const type* widget)
    {
       return traits::value(widget);
    }

    template <typename ValueType>
    static void setValue(type* widget, const ValueType& value)
    {
        traits::setValue(widget,value);
    }
};

/**
 * @brief Base template class for editable labels of various types.
 */
template <EditableLabel::Type TypeId>
class EditableLabelTmpl : public EditableLabel
{
    public:

        using helper = EditableLabelHelper<TypeId>;
        using widgetType = typename helper::type;

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         * @param inGroup Label is part of a group, see also setInGroup().
         */
        EditableLabelTmpl(QWidget* parent=nullptr, bool inGroup=false)
            : EditableLabel(TypeId,parent,inGroup),
              m_widget(helper::createWidget(parent))
        {
            boxLayout()->insertWidget(0,m_widget);
            m_widget->setVisible(false);

            m_widget->setObjectName("widget");
            m_widget->installEventFilter(this);
        }

        /**
         * @brief Get value of the label.
         */
        auto value() const
        {
           return helper::value(m_widget);
        }

        /**
         * @brief Set value of the label.
         * @param value Value to set.
         */
        template <typename ValueType>
        void setValue(const ValueType& value)
        {
            m_widget->blockSignals(true);
            helper::setValue(m_widget,value);
            helper::loadLabel(label(),m_widget,formatter());
            m_backupValue=helper::value(m_widget);
            m_widget->blockSignals(false);
        }

        /**
         * @brief Get editor widget.
         * @return Query result.
         */
        QWidget* editor() const noexcept override
        {
            return m_widget;
        }

        /**
         * @brief Get editor.
         * @return Query result.
         */
        auto widget() const noexcept
        {
            return m_widget;
        }

        /**
         * @brief Apply editor.
         */
        virtual void apply() override
        {
            setEditable(false);
            m_backupValue=helper::value(m_widget);
            helper::loadLabel(label(),m_widget,formatter());
            emit valueSet();
            notifyValueChanged();
        }

    protected:

        virtual void notifyValueChanged()=0;

        virtual void restoreWidgetValue() override
        {
            m_widget->blockSignals(true);
            helper::setValue(m_widget,m_backupValue);
            m_widget->blockSignals(false);
        }

    private:

        typename helper::type *m_widget;

        using keepValueType=decltype(helper::value(std::declval<typename helper::type*>()));

        keepValueType m_backupValue;
};

}

/**
 * @brief Editable text label.
 */
class UISE_DESKTOP_EXPORT EditableLabelText : public detail::EditableLabelTmpl<EditableLabel::Type::Text>
{
    Q_OBJECT

    public:

        using baseType = detail::EditableLabelTmpl<EditableLabel::Type::Text>;

        using baseType::baseType;

    signals:

        void valueChanged(const QString& text);

    protected:

        virtual void notifyValueChanged() override
        {
            emit valueChanged(widget()->text());
        }
};

/**
 * @brief Editable integer label.
 */
class UISE_DESKTOP_EXPORT EditableLabelInt : public detail::EditableLabelTmpl<EditableLabel::Type::Int>
{
    Q_OBJECT

    public:

        using baseType = detail::EditableLabelTmpl<EditableLabel::Type::Int>;

        using baseType::baseType;

    signals:

        /**
         * @brief Signal that value of the label was changed.
         * @param value New value.
         */
        void valueChanged(int value);

    protected:

        virtual void notifyValueChanged() override
        {
            emit valueChanged(widget()->value());
        }
};

/**
 * @brief Editable double label.
 */
class UISE_DESKTOP_EXPORT EditableLabelDouble : public detail::EditableLabelTmpl<EditableLabel::Type::Double>
{
    Q_OBJECT

    public:

        using baseType = detail::EditableLabelTmpl<EditableLabel::Type::Double>;

        using baseType::baseType;

    signals:

        /**
         * @brief Signal that value of the label was changed.
         * @param value New value.
         */
        void valueChanged(double value);

    protected:

        virtual void notifyValueChanged() override
        {
            emit valueChanged(widget()->value());
        }
};

/**
 * @brief Editable date label.
 */
class UISE_DESKTOP_EXPORT EditableLabelDate : public detail::EditableLabelTmpl<EditableLabel::Type::Date>
{
    Q_OBJECT

    public:

        using baseType = detail::EditableLabelTmpl<EditableLabel::Type::Date>;

        using baseType::baseType;

    signals:

        /**
         * @brief Signal that value of the label was changed.
         * @param value New value.
         */
        void valueChanged(const QDate& value);

    protected:

        virtual void notifyValueChanged() override
        {
            emit valueChanged(widget()->date());
        }
};

/**
 * @brief Editable time label.
 */
class UISE_DESKTOP_EXPORT EditableLabelTime: public detail::EditableLabelTmpl<EditableLabel::Type::Time>
{
    Q_OBJECT

    public:

        using baseType = detail::EditableLabelTmpl<EditableLabel::Type::Time>;

        using baseType::baseType;

    signals:

        /**
         * @brief Signal that value of the label was changed.
         * @param value New value.
         */
        void valueChanged(const QTime& value);

    protected:

        virtual void notifyValueChanged() override
        {
            emit valueChanged(widget()->time());
        }
};

/**
 * @brief Editable datetime label.
 */
class UISE_DESKTOP_EXPORT EditableLabelDateTime: public detail::EditableLabelTmpl<EditableLabel::Type::DateTime>
{
    Q_OBJECT

    public:

        using baseType = detail::EditableLabelTmpl<EditableLabel::Type::DateTime>;

        using baseType::baseType;

    signals:

        /**
         * @brief Signal that value of the label was changed.
         * @param value New value.
         */
        void valueChanged(const QDateTime& value);

    protected:

        virtual void notifyValueChanged() override
        {
            emit valueChanged(widget()->dateTime());
        }
};

/**
 * @brief Editable list label.
 */
class UISE_DESKTOP_EXPORT EditableLabelList: public detail::EditableLabelTmpl<EditableLabel::Type::List>
{
    Q_OBJECT

    public:

        using baseType = detail::EditableLabelTmpl<EditableLabel::Type::List>;

        using baseType::baseType;

    signals:

        /**
         * @brief Signal that current index of the list was changed.
         * @param index Current index.
         */
        void indexChanged(int index);

        /**
         * @brief Signal that current text of the list was changed.
         * @param text Current text.
         */
        void textChanged(const QString& text);

    protected:

        virtual void notifyValueChanged() override
        {
            emit indexChanged(widget()->currentIndex());
            emit textChanged(widget()->currentText());
        }
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_EDITABLELABEL_HPP
