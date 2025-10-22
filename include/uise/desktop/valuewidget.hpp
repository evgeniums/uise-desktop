/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-comboed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/valuewidget.hpp
*
*  Declares value widget.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_VALUEWIDGET_HPP
#define UISE_DESKTOP_VALUEWIDGET_HPP

#include <map>

#include <QVariant>
#include <QWidget>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class ValueWidgetConfig
{
    public:

        ValueWidgetConfig()
        {}

        ValueWidgetConfig(std::map<int,QVariant> properties)
            : m_properties(std::move(properties))
        {}

        ValueWidgetConfig(std::initializer_list<std::map<int,QVariant>::value_type> properties)
            : m_properties(std::move(properties))
        {}

        bool hasProperty(int propertyId) const
        {
            auto it=m_properties.find(propertyId);
            if (it!=m_properties.end())
            {
                return true;
            }
            return false;
        }

        QVariant property(int propertyId, QVariant defaultValue={}) const
        {
            auto it=m_properties.find(propertyId);
            if (it!=m_properties.end())
            {
                return it->second;
            }
            return defaultValue;
        }

        template <typename T>
        bool hasProperty(T propertyId) const
        {
            return hasProperty(static_cast<int>(propertyId));
        }

        template <typename T>
        QVariant property(T propertyId, QVariant defaultValue={}) const
        {
            return property(static_cast<int>(propertyId),std::move(defaultValue));
        }

        void setProperty(int id, QVariant& value)
        {
            m_properties.emplace(id,std::move(value));
        }

    private:

        std::map<int,QVariant> m_properties;
};

enum class ValueWidgetProperty : int
{
    Label,
    Comment,
    Editable,
    InGroup,
    Alignment,
    ColumnSpan,
    RowSpan,
    Min,
    Max,
    ListItems,
    EditFocus,
    ExplicitType,

    User=0x1000
};

class AbstractEditablePanel;

class AbstractValueWidget : public QWidget
{
    Q_OBJECT

    public:

        using QWidget::QWidget;

        virtual void setVariantValue(const QVariant& val)=0;
        virtual QVariant variantValue() const=0;

        void setConfig(ValueWidgetConfig config)
        {
            m_config=std::move(config);
            updateConfig();
        }

        const ValueWidgetConfig& config() const
        {
            return m_config;
        }

        ValueWidgetConfig& config()
        {
            return m_config;
        }

        virtual void setEditablePanel(AbstractEditablePanel*)
        {}


        void setGroupEditingRequestEnabled(bool enable)
        {
            m_requestGroupEditingEnabled=enable;
        }

        bool isGroupEditingRequestEnabled() const noexcept
        {
            return m_requestGroupEditingEnabled;
        }

    signals:

        void valueEdited();
        void groupEditingRequested();
        void groupCancelRequested();

    protected:

        virtual void updateConfig()
        {}

    private:

        ValueWidgetConfig m_config;
        bool m_requestGroupEditingEnabled=false;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_VALUEWIDGET_HPP
