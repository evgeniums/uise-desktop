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

/** @file uise/desktop/svgiconlocator.hpp
*
*  Declares SVG icon locator class.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_SVG_ICON_LOCATOR_HPP
#define UISE_DESKTOP_SVG_ICON_LOCATOR_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/stylecontext.hpp>
#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/svgiconcontext.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT SvgIconLocator
{
    private:

        using colorMapsT=SvgIconColorMaps;

        struct SelectorContext
        {
            ContextSelector m_selector;

            std::map<QString,std::shared_ptr<SvgIcon>> m_icons;

            std::map<QString,QString> m_namesMap;
            std::map<QString,QString> m_namePaths;
            std::map<QString,colorMapsT> m_contextColorMaps;

            colorMapsT* m_defaultColorMapsPtr=nullptr;
        };

        template <typename ContextT>
        void addNameMapping(
            ContextT* ctx,
            QString alias,
            QString name
            )
        {
            ctx->m_namesMap.emplace(std::move(alias),std::move(name));
        }

        template <typename ContextT>
        QString nameMapping(const ContextT* ctx, const QString& alias) const
        {
            auto it=ctx->m_namesMap.find(alias);
            if (it!=ctx->m_namesMap.end())
            {
                return it->second;
            }
            return alias;
        }

        template <typename ContextT>
        void addNamePath(
            ContextT* ctx,
            QString name,
            QString path
            )
        {
            ctx->m_namePaths.emplace(std::move(name),std::move(path));
        }

        template <typename ContextT>
        QString namePath(ContextT* ctx, const QString& name) const
        {
            auto it=ctx->m_namePaths.find(name);
            if (it!=ctx->m_namePaths.end())
            {
                return it->second;
            }
            return QString{};
        }

        template <typename ContextT>
        void addColorMap(ContextT* ctx, SvgIcon::ColorMap map, const QString& context={}, IconVariant mode=IconMode::Normal)
        {
            if (context.isEmpty())
            {
                ctx->m_defaultColorMapsPtr->emplace(mode,std::move(map));
            }
            else
            {
                colorMapsT* maps=nullptr;
                auto it=ctx->m_contextColorMaps.find(context);
                if (it!=ctx->m_contextColorMaps.end())
                {
                    maps=&it->second;
                }
                else
                {
                    auto it1=ctx->m_contextColorMaps.emplace(context,colorMapsT{});
                    maps=&it1.first->second;
                }
                maps->emplace(mode,std::move(map));
            }
        }

        template <typename ContextT>
        const SvgIcon::ColorMap* colorMaps(const ContextT* ctx, IconVariant mode=IconMode::Normal, const QString& context={}) const
        {
            auto maps=contextColorMaps(ctx,context);
            if (maps==nullptr)
            {
                return nullptr;
            }

            auto it=maps->find(mode);
            if (it!=maps->end())
            {
                return &it->second;
            }
            return nullptr;
        }

        template <typename ContextT>
        const colorMapsT* contextColorMaps(const ContextT* ctx, const QString& context={}) const
        {
            if (context.isNull())
            {
                return ctx->m_defaultColorMapsPtr;
            }
            else
            {
                auto it=ctx->m_contextColorMaps.find(context);
                if (it!=ctx->m_contextColorMaps.end())
                {
                    return &it->second;
                }
            }
            return contextColorMaps();
        }

        template <typename ContextT>
        colorMapsT* contextColorMaps(ContextT* ctx, const QString& context={})
        {
            if (context.isNull())
            {
                return ctx->m_defaultColorMapsPtr;
            }
            else
            {
                auto it=ctx->m_contextColorMaps.find(context);
                if (it!=ctx->m_contextColorMaps.end())
                {
                    return &it->second;
                }
            }
            return nullptr;
        }

        template <typename ContextT>
        void reload(ContextT* ctx)
        {
            for (auto& it:ctx->m_icons)
            {
                auto maps=contextColorMaps(ctx,it.second->context());
                if (maps!=nullptr)
                {
                    it.second->reload(*maps);
                }
            }
        }

    public:

        using IconConfig=SvgIconConfig;

        SvgIconLocator() : m_defaultColorMapsPtr(&m_defaultColorMaps)
        {}

        std::shared_ptr<SvgIcon> icon(const QString& name, const StyleContext& ={}) const;

        void loadIcons(const std::vector<IconConfig>& iconConfigs, const ContextSelector& selector={});

        void addNameMapping(
            QString alias,
            QString name
        )
        {
            return addNameMapping(this,std::move(alias),std::move(name));
        }

        QString nameMapping(const QString& alias) const
        {
            return nameMapping(this,alias);
        }

        void addNamePath(
                QString name,
                QString path
            )
        {
            addNamePath(this,std::move(name),std::move(path));
        }

        QString namePath(const QString& name) const
        {
            return namePath(this,name);
        }

        void addColorMap(SvgIcon::ColorMap map, const QString& context={}, IconVariant mode=IconMode::Normal)
        {
            addColorMap(this,std::move(map),context,mode);
        }

        const SvgIcon::ColorMap* colorMaps(IconVariant mode=IconMode::Normal, const QString& context={}) const
        {
            return colorMaps(this,mode,context);
        }

        const colorMapsT* contextColorMaps(const QString& context={}) const
        {
            return contextColorMaps(this,context);
        }

        colorMapsT* contextColorMaps(const QString& context={})
        {
            return contextColorMaps(this,context);
        }

        void reset();

        void clearIconDirs()
        {
            m_iconDirs.clear();
            m_iconDirSubstitutions.clear();
        }

        void addIconDir(QString path)
        {
            m_iconDirs.emplace_back(std::move(path));
        }

        const std::vector<QString>& iconDirss() const
        {
            return m_iconDirs;
        }

        void addIconDirSubstitution(QString original, QString subst)
        {
            m_iconDirSubstitutions.emplace(std::move(original),std::move(subst));
        }

        QString iconDirSubstitution(const QString& original) const
        {
            auto it=m_iconDirSubstitutions.find(original);
            if (it!=m_iconDirSubstitutions.end())
            {
                return it->second;
            }
            return original;
        }

        QString substituteIconDir(QString name) const
        {
            for (const auto& it: m_iconDirSubstitutions)
            {
                name.replace(it.first,it.second);
            }
            return name;
        }

        const std::map<QString,QString>& iconDirSubstitutions() const
        {
            return m_iconDirSubstitutions;
        }

        void setDefaultSizes(SizeSet defaultSizes)
        {
            m_defaultSizes=std::move(defaultSizes);
        }

        const SizeSet& defaultSizes() const noexcept
        {
            return m_defaultSizes;
        }

        void setFallbackIcon(const QString& name)
        {
            m_fallbackIcon=iconPriv(name,true);
        }

        void setFallbackIcon(std::shared_ptr<SvgIcon> icon)
        {
            m_fallbackIcon=std::move(icon);
        }

        std::shared_ptr<SvgIcon> fallbackIcon() const
        {
            return m_fallbackIcon;
        }

        QString extension() const
        {
            return "svg";
        }

        void reload()
        {
            clearCache();
            reload(this);

            for (auto& it:m_selectorContexts)
            {
                reload(&it);
            }
        }

        void clearCache()
        {
            m_contextIconCache.clear();
        }

        void loadIconTheme(const SvgIconTheme& theme);

        void reloadIconThemes(const std::vector<SvgIconTheme>& themes);

    private:

        struct IconSelectorCacheItem
        {
            QString name;
            QString path;
            std::shared_ptr<SvgIcon> icon;
            ContextSelector selector;

            IconSelectorCacheItem(
                QString name,
                QString path,
                std::shared_ptr<SvgIcon> icon,
                ContextSelector selector
                ) : name(std::move(name)),
                path(std::move(path)),
                icon(std::move(icon)),
                selector(std::move(selector))
            {}
        };

        std::shared_ptr<SvgIcon> iconPriv(const QString& name, bool autocreate) const;
        std::shared_ptr<SvgIcon> iconForContext(const QString& name, const StyleContext& context, bool autocreate) const;
        std::shared_ptr<SvgIcon> recreateContextIcon(size_t hash, const IconSelectorCacheItem& prevIcon);

        void clearBeforeReload()
        {
            m_icons.clear();
            m_selectorContexts.clear();
            m_contextIconCache.clear();
            m_namesMap.clear();
            m_namePaths.clear();
            m_contextColorMaps.clear();
        }

        SelectorContext* findSelectorContext(const ContextSelector& selector)
        {
            for (auto&& ctx: m_selectorContexts)
            {
                if (ctx.m_selector==selector)
                {
                    return &ctx;
                }
            }
            return nullptr;
        }

        SelectorContext* addSelectorContext(ContextSelector selector)
        {
            auto inserted=m_selectorContexts.emplace_back(SelectorContext{});
            inserted.m_defaultColorMapsPtr=m_defaultColorMapsPtr;
            inserted.m_selector=std::move(selector);
            return &m_selectorContexts.back();
        }

        template <typename T>
        void loadSvgIconContext(T* tagCtx, const SvgIconContext& iconContext);

        std::vector<QString> m_iconDirs;
        std::map<QString,QString> m_iconDirSubstitutions;

        mutable std::vector<SelectorContext> m_selectorContexts;
        mutable std::map<size_t,IconSelectorCacheItem> m_contextIconCache;

        mutable std::map<QString,std::shared_ptr<SvgIcon>> m_icons;

        std::map<QString,QString> m_namesMap;
        std::map<QString,QString> m_namePaths;
        std::map<QString,colorMapsT> m_contextColorMaps;

        colorMapsT m_defaultColorMaps;
        colorMapsT* m_defaultColorMapsPtr=nullptr;
        SizeSet m_defaultSizes;

        std::shared_ptr<SvgIcon> m_fallbackIcon;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_SVG_ICON_LOCATOR_HPP
