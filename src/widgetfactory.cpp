/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/widgetfactory.cpp
*
*  Defines WidgetFactory.
*
*/

/****************************************************************************/

#include <uise/desktop/stylecontext.hpp>
#include <uise/desktop/widget.hpp>
#include <uise/desktop/widgetfactory.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

QObject* WidgetFactory::makeWidget(const char* className, const QString& name, QWidget* parent) const
{
    QObject obj{parent};
    obj.setProperty(StyleContext::TypeProperty,className);
    if (!name.isEmpty())
    {
        obj.setObjectName(name);
    }
    StyleContext ctx{&obj};

    auto b=builder(className,ctx);
    if (b)
    {
        auto w=b(parent);
        auto wc=qobject_cast<WidgetController*>(w);
        if (wc!=nullptr)
        {
            wc->setParentWidget(parent);
        }
        return w;
    }

    return nullptr;
}

//--------------------------------------------------------------------------

WidgetController* WidgetFactory::makeWidgetController(const char* className, QString name, QWidget* parent) const
{
    auto wc=qobject_cast<WidgetController*>(makeWidget(className,std::move(name),parent));
    if (wc!=nullptr)
    {
        wc->setParentWidget(parent);
    }
    return nullptr;
}

//--------------------------------------------------------------------------

WidgetFactory::Builder WidgetFactory::builder(const char* className, const StyleContext& context) const
{
    auto it=m_builders.find(className);
    if (it==m_builders.end())
    {
        return Builder{};
    }

    const BuilderContext* ctx=nullptr;
    uint64_t bestMask=0;
    for (const auto& it1: it->second->contextBuilders)
    {
        auto mask=context.matches(it1.context);
        if (mask>bestMask)
        {
            bestMask=mask;
            ctx=&it1;
        }
    }

    if (ctx!=nullptr && ctx->builder)
    {
        return ctx->builder;
    }

    return it->second->defaultBuilder;
}

//--------------------------------------------------------------------------

void WidgetFactory::registerBuilder(Builder builder, std::string className, ContextSelector context)
{
    std::shared_ptr<WidgetBuilder> b;

    auto it=m_builders.find(className);
    if (it!=m_builders.end())
    {
        b=it->second;
    }
    else
    {
        b=std::make_shared<WidgetBuilder>();
        m_builders.emplace(std::move(className),b);
    }

    if (context.empty())
    {
        b->defaultBuilder=std::move(builder);
    }
    else
    {
        b->contextBuilders.emplace_back(std::move(context),std::move(builder));
    }
}

//--------------------------------------------------------------------------

std::vector<std::string> WidgetFactory::registeredTypes() const
{
    std::vector<std::string> r;
    for (auto&& it:m_builders)
    {
        r.push_back(it.first);
    }
    return r;
}

//--------------------------------------------------------------------------

void WidgetFactory::merge(const WidgetFactory& other)
{
    for (const auto& it: other.m_builders)
    {
        const auto& wb=it.second;
        registerBuilder(wb->defaultBuilder,it.first);
        for (const auto& cb: wb->contextBuilders)
        {
            registerBuilder(cb.builder,it.first,cb.context);
        }
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
