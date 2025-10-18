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
#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class WidgetFactory;
class Widget;

class UISE_DESKTOP_EXPORT WidgetBase
{
    public:

        virtual ~WidgetBase()=default;

        std::shared_ptr<const WidgetFactory> widgetFactory() const
        {
            return m_factory;
        }

        void setWidgetFactory(std::shared_ptr<const WidgetFactory> factory)
        {
            m_factory=factory;
        }

        virtual void construct()
        {}

        QObject* makeWidget(const char* className, QString name={}, QWidget* parent=nullptr) const;

    protected:

        std::shared_ptr<const WidgetFactory> m_factory;
};

class WidgetController;

struct WidgetControllerTraits
{
    template <typename T>
    static void preConstruct(T* createdObject, QWidget* parent)
    {
        auto wc=qobject_cast<WidgetController*>(createdObject);
        if (wc!=nullptr)
        {
            wc->setParentWidget(parent);
            wc->createActualWidget();
        }
    }
};

template <typename Traits=WidgetControllerTraits>
class WidgetT : public WidgetBase
{
    public:

        using WidgetBase::makeWidget;

        QObject* makeWidget(const QMetaObject& metaObj, QString name={}, QWidget* parent=nullptr) const
        {
            return WidgetBase::makeWidget(metaObj.className(),std::move(name),parent);
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
            if constexpr (std::is_base_of_v<WidgetBase,T>)
            {
                if (w)
                {
                    w->setWidgetFactory(widgetFactory());
                    Traits::preConstruct(w,parent);
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
            if constexpr (std::is_base_of_v<WidgetBase,T>)
            {
                if (w)
                {
                    w->setWidgetFactory(widgetFactory());
                    Traits::preConstruct(w,parent);
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
};

class UISE_DESKTOP_EXPORT WidgetController : public QObject,
                                             public WidgetT<WidgetControllerTraits>
{
    Q_OBJECT

    public:

        WidgetController(QObject* parent=nullptr);

        QWidget* qWidget();

        Widget* widget()
        {
            return m_widget;
        }

    protected:

        virtual Widget* doCreateActualWidget(QWidget* parent) = 0;

    private:

        void createActualWidget();
        void setParentWidget(QWidget* parent)
        {
            m_parentWidget=parent;
        }

        Widget* m_widget=nullptr;
        QWidget* m_parentWidget=nullptr;

        friend class WidgetControllerTraits;
};

class UISE_DESKTOP_EXPORT Widget : public WidgetT<>
{
    public:

        virtual QWidget* qWidget() =0;

    private:

        void setController(WidgetController* ctrl)
        {
            m_ctrl=ctrl;
            m_ctrl->setParent(qWidget());
        }

        friend class WidgetController;
        WidgetController* m_ctrl=nullptr;
};

class UISE_DESKTOP_EXPORT WidgetQFrame : public QFrame,
                                         public Widget
{
    Q_OBJECT

    public:

        using QFrame::QFrame;

        QWidget* qWidget() override
        {
            return this;
        }
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_WIDGET_HPP
