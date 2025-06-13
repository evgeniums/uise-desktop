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

/** @file uise/desktop/widgetfactory.hpp
*
*  Declares widget factory.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_WIDGET_FACTORY_HPP
#define UISE_DESKTOP_WIDGET_FACTORY_HPP

#include <map>
#include <QWidget>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/stylecontext.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT WidgetFactory
{
    public:

        using Builder=std::function<QWidget* (QWidget* parent)>;

        struct BuilderContext
        {
            ContextSelector context;
            Builder builder;

            BuilderContext(
                    ContextSelector context,
                    Builder builder
                ) : context(std::move(context)),
                    builder(std::move(builder))
            {}
        };

        struct WidgetBuilder
        {
            Builder defaultBuilder;
            std::vector<BuilderContext> contextBuilders;
        };

        QWidget* makeWidget(const char* className, QString name={}, QWidget* parent=nullptr) const;

        QWidget* makeWidget(const QMetaObject& metaObj, QString name={}, QWidget* parent=nullptr) const
        {
            return makeWidget(metaObj.className(),std::move(name),parent);
        }

        template <typename T>
        T* makeWidget(QString name={}, QWidget* parent=nullptr) const
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
            return w;
        }

        void registerBuilder(Builder builder, std::string className, ContextSelector context={});

        template <typename T>
        void registerBuilder(Builder builder, ContextSelector context={})
        {
            registerBuilder(std::move(builder),T::staticMetaObject.className(),std::move(context));
        }

        Builder builder(const char* className, const StyleContext& context={}) const;

        std::vector<std::string> registeredTypes() const;

        const std::map<std::string,std::shared_ptr<WidgetBuilder>,std::less<>>& builders() const
        {
            return m_builders;
        }

    private:

        std::map<std::string,std::shared_ptr<WidgetBuilder>,std::less<>> m_builders;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_WIDGET_FACTORY_HPP
