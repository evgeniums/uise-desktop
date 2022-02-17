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

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

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

    public slots:

        void setEditable(bool enable)
        {
            m_label->setVisible(!enable);
            editableWidget()->setVisible(enable);
        }

    signals:

        void valueSet();

    protected:

        void setText(const QString& text)
        {
            m_label->setText(text);
        }

    private:

        Type m_type;
        QLabel* m_label;
};

class EditableLabelFormat
{
};

template <EditableLabel::Type Type>
struct EditableLabelTraits
{
};

template <>
struct EditableLabelTraits<EditableLabel::Type::Text>
{
    using type=QLineEdit;

    static void loadLabel(QLabel* label, type* widget, EditableLabelFormat* format=nullptr)
    {
        std::ignore=format;
        label->setText(widget->text());
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

    static void loadLabel(QLabel* label, type* widget, EditableLabelFormat* format=nullptr)
    {
        std::ignore=format;
        label->setText(QString("%1").arg(widget->value()));
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

    static void loadLabel(QLabel* label, type* widget, EditableLabelFormat* format=nullptr)
    {
        std::ignore=format;
        label->setText(QString("%1").arg(widget->value()));
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

struct EditableLabelListValue
{
    int index;
    QString text;
    QVariant data;
};

template <>
struct EditableLabelTraits<EditableLabel::Type::List>
{
    using type=QComboBox;

    static void loadLabel(QLabel* label, type* widget, EditableLabelFormat* format=nullptr)
    {
        std::ignore=format;
        label->setText(widget->currentText());
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
};

template <>
struct EditableLabelTraits<EditableLabel::Type::Date>
{
    using type=QDateEdit;

    static void loadLabel(QLabel* label, const type* widget, EditableLabelFormat* format=nullptr)
    {
        std::ignore=format;
        label->setText(widget->date().toString());
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

    static void loadLabel(QLabel* label, const type* widget, EditableLabelFormat* format=nullptr)
    {
        label->setText(widget->time().toString());
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

    static void loadLabel(QLabel* label, const type* widget, EditableLabelFormat* format=nullptr)
    {
        label->setText(widget->dateTime().toString());
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

template <EditableLabel::Type Type>
struct EditableLabelHelper
{
    using traits = EditableLabelTraits<Type>;
    using type = typename traits::type;

    static auto createWidget(QWidget* parent=nullptr)
    {
        return new type(parent);
    }

    static void loadLabel(QLabel* label, type* widget, EditableLabelFormat* format=nullptr)
    {
        traits::loadLabel(label, widget, format);
    }

    static auto value(const type* widget)
    {
       return traits::value(widget);
    }

    template <typename ValueType>
    void setValue(type* widget, const ValueType& value)
    {
        traits::setValue(widget,value);
    }
};

}

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_EDITABLELABEL_HPP
