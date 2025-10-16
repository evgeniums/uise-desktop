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

/** @file uise/desktop/widget.hpp
*
*  Declares Widget.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_WIDGET_HPP
#define UISE_DESKTOP_WIDGET_HPP

#include <memory>

#include <QWidget>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class WidgetFactory;
class WidgetController;

class UISE_DESKTOP_EXPORT Widget
{
    public:

        Widget()=default;

        virtual ~Widget()=default;
        Widget(const Widget&)=delete;
        Widget(Widget&&)=delete;
        Widget& operator=(const Widget&)=delete;
        Widget& operator=(Widget&&)=delete;

        std::shared_ptr<WidgetFactory> widgetFactory() const
        {
            return m_factory;
        }

        void setWidgetFactory(std::shared_ptr<WidgetFactory> factory)
        {
            m_factory=std::move(factory);
        }

        QObject* makeWidget(const char* className, QString name={}, QWidget* parent=nullptr) const;

        QObject* makeWidget(const QMetaObject& metaObj, QString name={}, QWidget* parent=nullptr) const
        {
            return makeWidget(metaObj.className(),std::move(name),parent);
        }

        template <typename T>
        T* makeWidget(const QString& name={}, QWidget* parent=nullptr) const
        {
            auto w=qobject_cast<T*>(makeWidget(T::staticMetaObject,std::move(name),parent));
            if (w==nullptr)
            {
                if constexpr (std::is_constructible_v<T,QWidget*>)
                {
                    w=new T(parent);
                    w->setObjectName(name);
                }
            }
            if constexpr (std::is_base_of_v<Widget,T>)
            {
                if (w)
                {
                    w->setWidgetFactory(m_factory);
                    w->construct();
                }
            }

            return w;
        }

        template <typename T, typename DefaultT>
        T* makeWidget(const QString& name={}, QWidget* parent=nullptr) const
        {
            auto w=qobject_cast<T*>(makeWidget(T::staticMetaObject,std::move(name),parent));
            if (w==nullptr)
            {
                w=new DefaultT(parent);
                w->setObjectName(name);
            }
            if constexpr (std::is_base_of_v<Widget,T>)
            {
                if (w)
                {
                    w->setWidgetFactory(m_factory);
                    w->construct();
                }
            }

            return w;
        }

        template <typename T>
        T* makeWidget(QWidget* parent) const
        {
            return makeWidget<T>(QString{},parent);
        }

        template <typename T, typename DefaultT>
        T* makeWidget(QWidget* parent) const
        {
            return makeWidget<T,DefaultT>(QString{},parent);
        }

        virtual void construct()
        {}

        virtual WidgetController* controller()
        {
            return nullptr;
        }

    private:

        std::shared_ptr<WidgetFactory> m_factory;
};

class UISE_DESKTOP_EXPORT WidgetController : public QObject,
                                             public Widget
{
    Q_OBJECT

    public:

        using QObject::QObject;

        QWidget* widget()
        {
            return m_widget;
        }

    private:

        QWidget* m_widget;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_WIDGET_HPP
