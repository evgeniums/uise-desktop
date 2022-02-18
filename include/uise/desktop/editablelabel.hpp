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

#include <QBoxLayout>
#include <QEvent>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class EditableLabelFormatter;

class UISE_DESKTOP_EXPORT EditableLabel : public QFrame
{
    Q_OBJECT

    public:

        enum class Type : int
        {
            Text,
            Int,
            Float,
            List,
            Date,
            Time,
            DateTime,
            Custom
        };

        EditableLabel(Type type, QWidget* parent=nullptr);

        Type type() const noexcept
        {
            return m_type;
        }

        QLabel* label() const noexcept
        {
            return m_label;
        }

        QString text() const
        {
            return m_label->text();
        }

        virtual QWidget* editableWidget() const noexcept =0;

        void setFormatter(EditableLabelFormatter* formatter) noexcept
        {
            m_formatter=formatter;
        }
        EditableLabelFormatter* formatter() const noexcept
        {
            return m_formatter;
        }

    public slots:

        void setEditable(bool enable)
        {
            m_label->setVisible(!enable);
            editableWidget()->setVisible(enable);
            editableWidget()->setFocus();
        }

        virtual void apply() = 0;

        void cancel()
        {
            setEditable(false);
            restoreWidgetValue();
        }

    signals:

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
};

class UISE_DESKTOP_EXPORT EditableLabelFormatter
{
    public:

        ~EditableLabelFormatter() = default;

        EditableLabelFormatter()=default;
        EditableLabelFormatter(const EditableLabelFormatter&)=default;
        EditableLabelFormatter(EditableLabelFormatter&&)=default;
        EditableLabelFormatter& operator = (const EditableLabelFormatter&)=default;
        EditableLabelFormatter& operator = (EditableLabelFormatter&&)=default;

        virtual QString format(EditableLabel::Type type, const QVariant& value) =0;

        template <EditableLabel::Type Type, typename ValueT>
        static void loadLabel(QLabel* label, EditableLabelFormatter* formatter, const std::function<ValueT ()>& valueGetter, const std::function<QString (const ValueT&)>& defaultFormatter)
        {
            if (formatter==nullptr)
            {
                label->setText(defaultFormatter(valueGetter()));
            }
            else
            {
                label->setText(formatter->format(Type,valueGetter()));
            }
        }
};

template <EditableLabel::Type Type>
struct EditableLabelTraits
{
};

template <>
struct EditableLabelTraits<EditableLabel::Type::Text>
{
    using type=QLineEdit;

    static void loadLabel(QLabel* label, type* widget, EditableLabelFormatter* formatter=nullptr)
    {
        EditableLabelFormatter::loadLabel<EditableLabel::Type::Text,QString>(label,formatter,[widget](){return widget->text();},[](const QString& val){return val;});
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

template <>
struct EditableLabelTraits<EditableLabel::Type::Int>
{
    using type=QSpinBox;

    static void loadLabel(QLabel* label, type* widget, EditableLabelFormatter* formatter=nullptr)
    {
        using valueType=decltype(widget->value());

        EditableLabelFormatter::loadLabel<EditableLabel::Type::Int,valueType>(label,formatter,[widget](){return widget->value();},[](const valueType& val){return QString::number(val);});
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

template <>
struct EditableLabelTraits<EditableLabel::Type::Float>
{
    using type=QDoubleSpinBox;

    static void loadLabel(QLabel* label, type* widget, EditableLabelFormatter* formatter=nullptr)
    {
        using valueType=decltype(widget->value());

        EditableLabelFormatter::loadLabel<EditableLabel::Type::Float,valueType>(label,formatter,[widget](){return widget->value();},[](const valueType& val){return QString::number(val);});
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

struct UISE_DESKTOP_EXPORT EditableLabelListValue
{
    int index;
    QString text;
    QVariant data;
};

template <>
struct EditableLabelTraits<EditableLabel::Type::List>
{
    using type=QComboBox;

    static void loadLabel(QLabel* label, type* widget, EditableLabelFormatter* formatter=nullptr)
    {
        EditableLabelFormatter::loadLabel<EditableLabel::Type::List,QString>(label,formatter,[widget](){return widget->currentText();},[](const QString& val){return val;});
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

template <>
struct EditableLabelTraits<EditableLabel::Type::Date>
{
    using type=QDateEdit;

    static void loadLabel(QLabel* label, const type* widget, EditableLabelFormatter* formatter=nullptr)
    {
        using valueType=decltype(widget->date());

        auto dateFormat = QLocale().dateFormat(QLocale::ShortFormat);
        EditableLabelFormatter::loadLabel<EditableLabel::Type::Date,valueType>(label,formatter,[widget](){return widget->date();},[dateFormat](const valueType& val){return val.toString(dateFormat);});
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

template <>
struct EditableLabelTraits<EditableLabel::Type::Time>
{
    using type=QTimeEdit;

    static void loadLabel(QLabel* label, const type* widget, EditableLabelFormatter* formatter=nullptr)
    {
        using valueType=decltype(widget->time());

        EditableLabelFormatter::loadLabel<EditableLabel::Type::Time,valueType>(label,formatter,[widget](){return widget->time();},[](const valueType& val){return val.toString("HH:mm");});
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

template <>
struct EditableLabelTraits<EditableLabel::Type::DateTime>
{
    using type=QDateTimeEdit;

    static void loadLabel(QLabel* label, const type* widget, EditableLabelFormatter* formatter=nullptr)
    {
        using valueType=decltype(widget->dateTime());

        auto dateTimeFormat = QLocale().dateTimeFormat(QLocale::ShortFormat);
        EditableLabelFormatter::loadLabel<EditableLabel::Type::DateTime,valueType>(label,formatter,[widget](){return widget->dateTime();},[dateTimeFormat](const valueType& val){return val.toString(dateTimeFormat);});
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

template <EditableLabel::Type TypeId>
class EditableLabelTmpl : public EditableLabel
{
    public:

        using helper = EditableLabelHelper<TypeId>;
        using widgetType = typename helper::type;

        EditableLabelTmpl(QWidget* parent=nullptr)
            : EditableLabel(TypeId),
              m_widget(helper::createWidget(parent))
        {
            boxLayout()->addWidget(m_widget);
            m_widget->setVisible(false);

            m_widget->setObjectName("widget");
            m_widget->installEventFilter(this);
        }

        auto value() const
        {
           return helper::value(m_widget);
        }

        template <typename ValueType>
        void setValue(const ValueType& value)
        {
            m_widget->blockSignals(true);
            helper::setValue(m_widget,value);
            helper::loadLabel(label(),m_widget,formatter());
            m_backupValue=helper::value(m_widget);
            m_widget->blockSignals(false);
        }

        QWidget* editableWidget() const noexcept override
        {
            return m_widget;
        }

        auto widget() const noexcept
        {
            return m_widget;
        }

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

class UISE_DESKTOP_EXPORT EditableLabelInt : public detail::EditableLabelTmpl<EditableLabel::Type::Int>
{
    Q_OBJECT

    public:

        using baseType = detail::EditableLabelTmpl<EditableLabel::Type::Int>;

        using baseType::baseType;

    signals:

        void valueChanged(int value);

    protected:

        virtual void notifyValueChanged() override
        {
            emit valueChanged(widget()->value());
        }
};

class UISE_DESKTOP_EXPORT EditableLabelFloat : public detail::EditableLabelTmpl<EditableLabel::Type::Float>
{
    Q_OBJECT

    public:

        using baseType = detail::EditableLabelTmpl<EditableLabel::Type::Float>;

        using baseType::baseType;

    signals:

        void valueChanged(double value);

    protected:

        virtual void notifyValueChanged() override
        {
            emit valueChanged(widget()->value());
        }
};

class UISE_DESKTOP_EXPORT EditableLabelDate : public detail::EditableLabelTmpl<EditableLabel::Type::Date>
{
    Q_OBJECT

    public:

        using baseType = detail::EditableLabelTmpl<EditableLabel::Type::Date>;

        using baseType::baseType;

    signals:

        void valueChanged(const QDate& value);

    protected:

        virtual void notifyValueChanged() override
        {
            emit valueChanged(widget()->date());
        }
};

class UISE_DESKTOP_EXPORT EditableLabelTime: public detail::EditableLabelTmpl<EditableLabel::Type::Time>
{
    Q_OBJECT

    public:

        using baseType = detail::EditableLabelTmpl<EditableLabel::Type::Time>;

        using baseType::baseType;

    signals:

        void valueChanged(const QTime& value);

    protected:

        virtual void notifyValueChanged() override
        {
            emit valueChanged(widget()->time());
        }
};

class UISE_DESKTOP_EXPORT EditableLabelDateTime: public detail::EditableLabelTmpl<EditableLabel::Type::DateTime>
{
    Q_OBJECT

    public:

        using baseType = detail::EditableLabelTmpl<EditableLabel::Type::DateTime>;

        using baseType::baseType;

    signals:

        void valueChanged(const QDateTime& value);

    protected:

        virtual void notifyValueChanged() override
        {
            emit valueChanged(widget()->dateTime());
        }
};

class UISE_DESKTOP_EXPORT EditableLabelList: public detail::EditableLabelTmpl<EditableLabel::Type::List>
{
    Q_OBJECT

    public:

        using baseType = detail::EditableLabelTmpl<EditableLabel::Type::List>;

        using baseType::baseType;

    signals:

        void indexChanged(int index);
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
