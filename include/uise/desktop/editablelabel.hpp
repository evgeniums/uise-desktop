/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

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
#include <uise/desktop/pushbutton.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class EditableLabelFormatter;

class EditableLabelText;
class EditableLabelInt;
class EditableLabelDouble;
class EditableLabelList;
class EditableLabelDate;
class EditableLabelTime;
class EditableLabelDateTime;

/**
 * @brief Type of label content.
 */
enum class EditableLabelType : int
{
    Text,
    Int,
    Double,
    List,
    Date,
    Time,
    DateTime,

    Custom=0x100
};

struct EditableLabelConfig
{
    int type;
    QString id;
    QVariant value;
};

/**
 * @brief Base class for editable labels.
 */
class UISE_DESKTOP_EXPORT EditableLabel : public QFrame
{
    Q_OBJECT

    public:

        using Type=EditableLabelType;

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
        void setFormatter(const EditableLabelFormatter* formatter) noexcept
        {
            m_formatter=formatter;
        }
        /**
         * @brief Get text formatter.
         * @return Query result.
         */
        const EditableLabelFormatter* formatter() const noexcept
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
            setEditing(m_editing);
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
        bool isEditing() const noexcept
        {
            return m_editing;
        }

        void setId(QString id)
        {
            m_id=std::move(id);
        }

        QString id() const
        {
            return m_id;
        }

        virtual void setVariantValue(const QVariant& val)=0;
        virtual QVariant variantValue() const=0;

    public slots:

        /**
         * @brief Set editing mode.
         * @param enable If trure then show editor, potherwise show text label.
         */
        void setEditing(bool enable)
        {
            m_editing=enable;
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
            setEditing(false);
            restoreWidgetValue();
        }

        /**
         * @brief Enable editing.
         */
        void edit()
        {
            setEditing(true);
        }

    signals:

        /**
         * @brief New value was set for the label.
         */
        void valueUpdated();

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
        const EditableLabelFormatter* m_formatter;
        bool m_editing;
        bool m_inGroup;

        QString m_id;

        PushButton* m_editButton;
        PushButton* m_applyButton;
        PushButton* m_cancelButton;
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
        virtual QString format(EditableLabel::Type type, const QVariant& value) const =0;

        /**
         * @brief Load lable with formatted text.
         * @param label Label to set text to.
         * @param formatter Formatter for text formatting.
         * @param value Value to format and to to label.
         * @param defaultFormatter Default text formatter.
         */
        template <EditableLabel::Type Type, typename ValueT>
        static void loadLabel(QLabel* label, const EditableLabelFormatter* formatter, const ValueT& value, const std::function<QString (const ValueT&)>& defaultFormatter)
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
    using type=EditableLabelText;
    using widgetType=QLineEdit;

    static void loadLabel(QLabel* label, widgetType* widget, const EditableLabelFormatter* formatter=nullptr)
    {
        EditableLabelFormatter::loadLabel<EditableLabel::Type::Text,QString>(label,formatter,widget->text(),[](const QString& val){return val;});
    }

    static auto value(const widgetType* widget)
    {
        return widget->text();
    }

    static void setValue(widgetType* widget, const QString& value)
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
    using type=EditableLabelInt;
    using widgetType=QSpinBox;

    static void loadLabel(QLabel* label, widgetType* widget, const EditableLabelFormatter* formatter=nullptr)
    {
        using valueType=decltype(widget->value());

        EditableLabelFormatter::loadLabel<EditableLabel::Type::Int,valueType>(label,formatter,widget->value(),[](const valueType& val){return QString::number(val);});
    }

    static auto value(const widgetType* widget)
    {
        return widget->value();
    }

    static void setValue(widgetType* widget, int value)
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
    using type=EditableLabelDouble;
    using widgetType=QDoubleSpinBox;

    static void loadLabel(QLabel* label, widgetType* widget, const EditableLabelFormatter* formatter=nullptr)
    {
        using valueType=decltype(widget->value());

        EditableLabelFormatter::loadLabel<EditableLabel::Type::Double,valueType>(label,formatter,widget->value(),[](const valueType& val){return QString::number(val);});
    }

    static auto value(const widgetType* widget)
    {
        return widget->value();
    }

    static void setValue(widgetType* widget, double value)
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
    using type=EditableLabelList;
    using widgetType=QComboBox;

    static void loadLabel(QLabel* label, widgetType* widget, const EditableLabelFormatter* formatter=nullptr)
    {
        EditableLabelFormatter::loadLabel<EditableLabel::Type::List,QString>(label,formatter,widget->currentText(),[](const QString& val){return val;});
    }

    static auto value(const widgetType* widget)
    {
        return EditableLabelListValue{widget->currentIndex(),widget->currentText(),widget->currentData()};
    }

    static void setValue(widgetType* widget, int value)
    {
        widget->setCurrentIndex(value);
    }

    static void setValue(widgetType* widget, const QString& value)
    {
        widget->setCurrentText(value);
    }

    static void setValue(widgetType* widget, const EditableLabelListValue& value)
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
    using type=EditableLabelDate;
    using widgetType=QDateEdit;

    static void loadLabel(QLabel* label, const widgetType* widget, const EditableLabelFormatter* formatter=nullptr)
    {
        using valueType=decltype(widget->date());

        auto dateFormat = QLocale().dateFormat(QLocale::ShortFormat);
        EditableLabelFormatter::loadLabel<EditableLabel::Type::Date,valueType>(label,formatter,widget->date(),[dateFormat](const valueType& val){return val.toString(dateFormat);});
    }

    static auto value(const widgetType* widget)
    {
        return widget->date();
    }

    static void setValue(widgetType* widget, const QDate& value)
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
    using type=EditableLabelTime;
    using widgetType=QTimeEdit;

    static void loadLabel(QLabel* label, const widgetType* widget, const EditableLabelFormatter* formatter=nullptr)
    {
        using valueType=decltype(widget->time());

        EditableLabelFormatter::loadLabel<EditableLabel::Type::Time,valueType>(label,formatter,widget->time(),[](const valueType& val){return val.toString("HH:mm");});
    }

    static auto value(const widgetType* widget)
    {
        return widget->time();
    }

    static void setValue(widgetType* widget, const QTime& value)
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
    using type=EditableLabelDateTime;
    using widgetType=QDateTimeEdit;

    static void loadLabel(QLabel* label, const widgetType* widget, const EditableLabelFormatter* formatter=nullptr)
    {
        using valueType=decltype(widget->dateTime());

        auto dateTimeFormat = QLocale().dateTimeFormat(QLocale::ShortFormat);
        EditableLabelFormatter::loadLabel<EditableLabel::Type::DateTime,valueType>(label,formatter,widget->dateTime(),[dateTimeFormat](const valueType& val){return val.toString(dateTimeFormat);});
    }

    static auto value(const widgetType* widget)
    {
        return widget->dateTime();
    }

    static void setValue(widgetType* widget, const QDateTime& value)
    {
        widget->setDateTime(value);
    }
};

template <EditableLabel::Type TypeId>
struct EditableLabelHelper
{
    using traits = EditableLabelTraits<TypeId>;

    using type = typename traits::type;
    using widgetType = typename traits::widgetType;
    using valueType = decltype(traits::value(std::declval<widgetType*>()));

    static auto createWidget(QWidget* parent=nullptr)
    {
        return new widgetType(parent);
    }

    static void loadLabel(QLabel* label, widgetType* widget, const EditableLabelFormatter* formatter=nullptr)
    {
        traits::loadLabel(label, widget, formatter);
    }

    static auto value(const widgetType* widget)
    {
       return traits::value(widget);
    }

    template <typename ValueType>
    static void setValue(widgetType* widget, const ValueType& value)
    {
        traits::setValue(widget,value);
    }
};

namespace detail
{

/**
 * @brief Base template class for editable labels of various types.
 */
template <EditableLabel::Type TypeId>
class EditableLabelTmpl : public EditableLabel
{
    public:

        using helper = EditableLabelHelper<TypeId>;
        using widgetType = typename helper::widgetType;
        using valueType = typename helper::valueType;

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         * @param inGroup Label is part of a group, see also setInGroup().
         */
        EditableLabelTmpl(QWidget* parent=nullptr, bool inGroup=false)
            : EditableLabel(TypeId,parent,inGroup),
              m_editor(helper::createWidget(parent))
        {
            boxLayout()->insertWidget(0,m_editor);
            m_editor->setVisible(false);

            m_editor->setObjectName("editor");
            m_editor->installEventFilter(this);
        }

        /**
         * @brief Get value of the label.
         */
        auto value() const
        {
           return helper::value(m_editor);
        }

        /**
         * @brief Set value of the label.
         * @param value Value to set.
         */
        template <typename ValueType>
        void setValue(const ValueType& value)
        {
            m_editor->blockSignals(true);
            helper::setValue(m_editor,value);
            helper::loadLabel(label(),m_editor,formatter());
            m_backupValue=helper::value(m_editor);
            m_editor->blockSignals(false);
        }

        void setVariantValue(const QVariant& val) override
        {
            setValue(val.value<valueType>());
        }

        QVariant variantValue() const override
        {
            return QVariant::fromValue(value());
        }

        /**
         * @brief Get editor widget.
         * @return Query result.
         */
        QWidget* editor() const noexcept override
        {
            return m_editor;
        }

        /**
         * @brief Get editor widget.
         * @return Query result.
         */
        auto editorWidget() const noexcept
        {
            return m_editor;
        }

        /**
         * @brief Apply editor.
         */
        virtual void apply() override
        {
            setEditing(false);
            m_backupValue=helper::value(m_editor);
            helper::loadLabel(label(),m_editor,formatter());
            emit valueUpdated();
            notifyValueChanged();
        }

    protected:

        virtual void notifyValueChanged()=0;

        virtual void restoreWidgetValue() override
        {
            m_editor->blockSignals(true);
            helper::setValue(m_editor,m_backupValue);
            m_editor->blockSignals(false);
        }

    private:

        typename helper::widgetType *m_editor;
        typename helper::valueType m_backupValue;
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
            emit valueChanged(editorWidget()->text());
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
            emit valueChanged(editorWidget()->value());
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
            emit valueChanged(editorWidget()->value());
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
            emit valueChanged(editorWidget()->date());
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
            emit valueChanged(editorWidget()->time());
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
            emit valueChanged(editorWidget()->dateTime());
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
            emit indexChanged(editorWidget()->currentIndex());
            emit textChanged(editorWidget()->currentText());
        }
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_EDITABLELABEL_HPP
