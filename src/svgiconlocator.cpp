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

/** @file uise/desktop/icontheme.hpp
*
*  Defines SvgIconLocator.
*
*/

/****************************************************************************/

#include <filesystem>

#include <QDebug>
#include <QFile>

#include <uise/desktop/svgiconlocator.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

QString existingFileName(const QString& name, const QString& extension, const std::vector<QString>& iconDirs)
{
    std::filesystem::path p{name.toStdString()};
    if (!p.has_extension())
    {
        p.replace_extension(extension.toStdString());
    }

    for (const auto& dir: iconDirs)
    {
        auto path=dir+"/"+QString::fromStdString(p.string());
        if (QFile::exists(path))
        {
            return path;
        }
    }

    return QString{};
}

QString nameContext(const QString& name)
{
    QString context;
    if (name.contains("::"))
    {
        auto parts=name.split("::");
        if (parts.count()>1)
        {
            context=parts[0];
        }
    }
    return context;
}

QString plainName(const QString& name)
{
    QString context;
    if (name.contains("::"))
    {
        auto parts=name.split("::");
        if (parts.count()>0)
        {
            context=parts.back();
        }
    }
    return context;
}
}

//--------------------------------------------------------------------------

std::shared_ptr<SvgIcon> SvgIconLocator::icon(const QString& name, const StyleContext& context) const
{
    if (context.object()==nullptr)
    {
        return iconPriv(name,true);
    }

    return iconForContext(name,context,true);
}

//-------------------------------------------------------------------------- 

std::shared_ptr<SvgIcon> SvgIconLocator::iconPriv(const QString& name, bool autocreate) const
{
    // find icon in cache
    auto it=m_icons.find(name);
    if (it!=m_icons.end())
    {
        // qDebug() << "Icon found " << name<< " " <<it->second.get();;
        return it->second;
    }
    // qDebug() << "Icon not found " << name;
    if (!autocreate)
    {
        return std::shared_ptr<SvgIcon>{};
    }

    // find actual name
    auto actualName=nameMapping(name);

    // create new icon if not found in cache

    // find icon path
    auto path=namePath(actualName);
    if (path.isEmpty())
    {
        path=existingFileName(actualName,extension(),m_iconDirs);
        if (path.isEmpty())
        {
            // try to split hierarchical name and find path again
            auto pName=plainName(name);
            if (pName!=name)
            {
                path=namePath(pName);
            }
            if (path.isEmpty())
            {
                path=existingFileName(pName,extension(),m_iconDirs);
            }
            if (path.isEmpty())
            {
                qWarning() << "Failed to find icon file for " << name;
                return m_fallbackIcon;
            }
        }
    }

    const colorMapsT* maps=contextColorMaps(nameContext(name));

    // make icon
    auto icon=std::make_shared<SvgIcon>();
    icon->setName(name);
    auto ok=icon->addFile(path,*maps,m_defaultSizes);
    if (!ok)
    {
        return m_fallbackIcon;
    }

    // keep new icon in cache
    m_icons[name]=icon;
    // qDebug() << "Icon added " << name << " " <<icon.get();

    // done
    return icon;
}

//--------------------------------------------------------------------------

std::shared_ptr<SvgIcon> SvgIconLocator::iconForContext(const QString& name, const StyleContext& context, bool autocreate) const
{
    // try to find icon in cache
    auto pathHash=context.pathHash(name);
    auto it1=m_contextIconCache.find(pathHash.first);
    if (it1!=m_contextIconCache.end())
    {
        if (it1->second.path==pathHash.second)
        {
            return it1->second.icon;
        }
    }

    // find selector context
    SelectorContext* bestSelectorContext=nullptr;
    uint64_t bestMatchedContextMask=0;
    for (auto& selectorContext : m_selectorContexts)
    {
        auto mask=context.matches(selectorContext.m_selector);
        if (mask>bestMatchedContextMask)
        {
            bestMatchedContextMask=mask;
            bestSelectorContext=&selectorContext;
        }
    }

    // check if context found
    if (bestSelectorContext==nullptr)
    {
        if (!autocreate)
        {
            return std::shared_ptr<SvgIcon>{};
        }
        else
        {
            auto icon=iconPriv(name,autocreate);
            m_contextIconCache.emplace(std::piecewise_construct,std::forward_as_tuple(pathHash.first),std::forward_as_tuple(name,pathHash.second,icon,ContextSelector{}));
            return icon;
        }
    }

    // try to find icon in context
    auto it=bestSelectorContext->m_icons.find(name);
    if (it!=bestSelectorContext->m_icons.end())
    {
        // qDebug() << "Icon found " << name;

        return it->second;
    }
    // qDebug() << "Icon not found " << name;
    if (!autocreate)
    {
        return std::shared_ptr<SvgIcon>{};
    }

    // find actual name
    auto actualName=nameMapping(bestSelectorContext,name);

    // create new icon if not found in caches

    // figure out icon path
    auto path=namePath(bestSelectorContext,actualName);
    if (path.isEmpty())
    {
        path=existingFileName(actualName,extension(),m_iconDirs);
        if (path.isEmpty())
        {
            // try to split hierarchical name and find path again
            auto pName=plainName(name);
            if (pName!=name)
            {
                path=namePath(pName);
            }
            if (path.isEmpty())
            {
                path=existingFileName(pName,extension(),m_iconDirs);
            }
            if (path.isEmpty())
            {
                qWarning() << "Failed to find icon file " << path;
                return m_fallbackIcon;
            }
        }
    }

    // figure out color map
    const colorMapsT* maps=contextColorMaps(bestSelectorContext,nameContext(name));

    // make icon
    auto icon=std::make_shared<SvgIcon>();
    icon->setName(name);
    auto ok=icon->addFile(path,*maps,m_defaultSizes);
    if (!ok)
    {
        return m_fallbackIcon;
    }

    // keep new icon in cache
    bestSelectorContext->m_icons[name]=icon;
    m_contextIconCache.emplace(std::piecewise_construct,std::forward_as_tuple(pathHash.first),std::forward_as_tuple(name,pathHash.second,icon,bestSelectorContext->m_selector));

    // done
    return icon;
}

//--------------------------------------------------------------------------

void SvgIconLocator::loadIcons(const std::vector<IconConfig>& iconConfigs, const ContextSelector& selector)
{
    for (const auto& iconConfig: iconConfigs)
    {
        std::shared_ptr<SvgIcon> icon;

        // try to find icon in cache
        auto pathHash=StyleContext::contextPathHash(selector,iconConfig.name);
        bool insert=false;        
        if (selector.empty())
        {
            icon=this->iconPriv(iconConfig.name,false);
        }
        else
        {
            auto it1=m_contextIconCache.find(pathHash.first);
            if (it1!=m_contextIconCache.end())
            {
                if (it1->second.path==pathHash.second)
                {
                    icon=it1->second.icon;
                }
            }
        }
        if (!icon)
        {
            insert=true;
            icon=std::make_shared<SvgIcon>();
            icon->setName(iconConfig.name);
        }

        // prepare sizes
        auto sizes=m_defaultSizes;
        if (!iconConfig.sizes.empty())
        {
            sizes=iconConfig.sizes;
        }

        // adjust if alias modes are empty
        auto aliasModes=iconConfig.aliases;
        if (aliasModes.empty())
        {
            aliasModes.emplace("",std::set<IconVariant>{});
        }

        // prepare color maps
        auto colorMaps=iconConfig.modes;
        if (colorMaps.empty())
        {
            const colorMapsT* maps=contextColorMaps(nameContext(iconConfig.name));
            colorMaps=*maps;
        }

        // add file for each alias
        for (const auto& it0: aliasModes)
        {
            // find actualName
            auto alias=it0.first;
            if (it0.first.isEmpty())
            {
                // use name as alias if alias is empty
                alias=iconConfig.name;
            }
            auto actualName=nameMapping(alias);

            // find file path
            auto path=namePath(actualName);
            if (path.isEmpty())
            {
                path=existingFileName(actualName,extension(),m_iconDirs);
                if (path.isEmpty())
                {
                    qWarning() << "Failed to find icon file " << path << " for icon " << iconConfig.name;
                    continue;
                }
            }

            // add file
            auto ok=icon->addFile(path,colorMaps,sizes);
            if (!ok)
            {
                icon.reset();
            }
        }

        // insert icon if no error and this is a new icon
        if (icon && insert)
        {
            if (selector.empty())
            {
                m_icons[iconConfig.name]=icon;
                // qDebug() << "Icon loaded " << iconConfig.name << " " <<icon.get();
            }
            else
            {
                // find selector context
                SelectorContext* selectorCtx=findSelectorContext(selector);

                // insert icon to selector context
                if (selectorCtx==nullptr)
                {
                    qWarning() << "Selector contexts should be initialized before loading icon configurations";
                    selectorCtx=addSelectorContext(selector);
                }
                selectorCtx->m_icons.emplace(iconConfig.name,icon);
                m_contextIconCache.emplace(std::piecewise_construct,std::forward_as_tuple(pathHash.first),std::forward_as_tuple(iconConfig.name,pathHash.second,icon,selector));
            }
        }
    }
}

//--------------------------------------------------------------------------

void SvgIconLocator::reset()
{
    clearIconDirs();
    clearCache();
    addIconDir(":/icons");
    addIconDirSubstitution("${uise-svg-icons-1}","tabler-icons/outline");
    addIconDirSubstitution("${uise-svg-icons-2}","tabler-icons/filled");
    addIconDirSubstitution("${uise-svg-icons-3}","reshot");
}

//--------------------------------------------------------------------------

template <typename T>
void SvgIconLocator::loadSvgIconContext(T* selectorCtx, const SvgIconContext& iconContext)
{
    for (const auto& alias: iconContext.aliases)
    {
        addNameMapping(selectorCtx,alias.first,alias.second);
    }
    for (const auto& namePath: iconContext.namePaths)
    {
        addNamePath(selectorCtx,namePath.first,namePath.second);
    }
    for (const auto& mode: iconContext.modes)
    {
        addColorMap(selectorCtx,mode.second,iconContext.name,mode.first);
    }
    loadIcons(iconContext.icons,iconContext.selector);
}

//--------------------------------------------------------------------------

void SvgIconLocator::loadIconTheme(const SvgIconTheme& theme)
{
    for (const auto& context: theme.contexts())
    {
        if (context.second.selector.empty())
        {
            loadSvgIconContext(this,context.second);
        }
        else
        {
            auto selectorCtx=findSelectorContext(context.second.selector);
            if (selectorCtx==nullptr)
            {
                selectorCtx=addSelectorContext(context.second.selector);
            }
            loadSvgIconContext(selectorCtx,context.second);
        }
    }
}

//--------------------------------------------------------------------------

void SvgIconLocator::reloadIconThemes(const std::vector<SvgIconTheme>& themes)
{
    // collect existing icons
    auto globalIcons=m_icons;
    auto contextIcons=m_contextIconCache;

    // load icon themes
    clearBeforeReload();
    for (const auto& theme: themes)
    {
        loadIconTheme(theme);
    }

    // reload previous global icons
    for (auto&& existedIcon: globalIcons)
    {
        auto newIcon=iconPriv(existedIcon.first,true);
        if (newIcon)
        {
            newIcon->reloadOtherFromThis(existedIcon.second);
        }
    }

    // reload previous context icons
    for (auto&& existedIcon : contextIcons)
    {
        auto newIcon=recreateContextIcon(existedIcon.first,existedIcon.second);
        if (newIcon)
        {
            newIcon->reloadOtherFromThis(existedIcon.second.icon);
        }
    }
}

//--------------------------------------------------------------------------

std::shared_ptr<SvgIcon> SvgIconLocator::recreateContextIcon(size_t hash, const IconSelectorCacheItem& prevIcon)
{
    // find selector context with the maximum number of matched selector
    SelectorContext* bestSelectorContext=findSelectorContext(prevIcon.selector);

    // check if context found
    if (bestSelectorContext==nullptr)
    {
        auto icon=iconPriv(prevIcon.name,true);
        m_contextIconCache.emplace(std::piecewise_construct,std::forward_as_tuple(hash),std::forward_as_tuple(prevIcon.name,prevIcon.path,icon,ContextSelector{}));
        return icon;
    }

    // find icon in context
    auto it=bestSelectorContext->m_icons.find(prevIcon.name);
    if (it!=bestSelectorContext->m_icons.end())
    {
        // qDebug() << "Icon found " << name;

        return it->second;
    }

    // find actual name
    auto actualName=nameMapping(bestSelectorContext,prevIcon.name);

    // create new icon if not found in cache

    // find icon path
    auto path=namePath(bestSelectorContext,actualName);
    if (path.isEmpty())
    {
        path=existingFileName(actualName,extension(),m_iconDirs);
        if (path.isEmpty())
        {
            // try to split hierarchical name and find path again
            auto pName=plainName(prevIcon.name);
            if (pName!=prevIcon.name)
            {
                path=namePath(pName);
            }
            if (path.isEmpty())
            {
                path=existingFileName(pName,extension(),m_iconDirs);
            }
            if (path.isEmpty())
            {
                qWarning() << "Failed to find icon file " << path;
                return m_fallbackIcon;
            }
        }
    }

    const colorMapsT* maps=contextColorMaps(bestSelectorContext,nameContext(prevIcon.name));

    // make icon
    auto icon=std::make_shared<SvgIcon>();
    icon->setName(prevIcon.name);
    auto ok=icon->addFile(path,*maps,m_defaultSizes);
    if (!ok)
    {
        return m_fallbackIcon;
    }

    // keep new icon in cache
    bestSelectorContext->m_icons[prevIcon.name]=icon;
    m_contextIconCache.emplace(std::piecewise_construct,std::forward_as_tuple(hash),std::forward_as_tuple(prevIcon.name,prevIcon.path,icon,bestSelectorContext->m_selector));

    // done
    return icon;
}


//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
