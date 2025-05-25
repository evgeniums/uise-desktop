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

size_t nameAndTagsHash(const QString& name, const QSet<QString>& tags)
{
    QString acc=name;
    for (const auto& tag: tags)
    {
        acc+=tag;
    }
    auto hash=qHash(acc);
    return hash;
}

}

//--------------------------------------------------------------------------

std::shared_ptr<SvgIcon> SvgIconLocator::icon(const QString& name, const StyleContext& context) const
{
    if (context.tags().empty())
    {
        return iconPriv(name,true);
    }
    return iconForTags(name,context.tags(),true);
}

//-------------------------------------------------------------------------- 

std::shared_ptr<SvgIcon> SvgIconLocator::iconPriv(const QString& name, bool autocreate) const
{
    // find icon in cache
    auto it=m_icons.find(name);
    if (it!=m_icons.end())
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
                qWarning() << "Failed to find icon file " << path;
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

    // done
    return icon;
}

//--------------------------------------------------------------------------

std::shared_ptr<SvgIcon> SvgIconLocator::iconForTags(const QString& name, const QSet<QString>& tags, bool autocreate) const
{
    // try to find icon in cache
    auto hash=nameAndTagsHash(name,tags);
    auto it1=m_tagsIconCache.find(hash);
    if (it1!=m_tagsIconCache.end())
    {
        if (it1->second.name==name && it1->second.tags==tags)
        {
            return it1->second.icon;
        }
    }

    // find tags context with the maximum number of matched tags
    TagsContext* bestTagsContext=nullptr;
    size_t bestMatchedTags=0;
    for (auto& tagContext : m_tagContexts)
    {
        size_t matchedTags=0;
        for (const auto& tag: tags)
        {
            if (tagContext.m_tags.contains(tag))
            {
                matchedTags++;
            }
        }
        if (matchedTags>bestMatchedTags)
        {
            bestMatchedTags=matchedTags;
            bestTagsContext=&tagContext;
        }
    }
    if (!autocreate)
    {
        if (bestMatchedTags!=tags.count())
        {
            return std::shared_ptr<SvgIcon>{};
        }
    }
    if (bestTagsContext==nullptr)
    {
        if (!autocreate)
        {
            return std::shared_ptr<SvgIcon>{};
        }
        else
        {
            auto icon=iconPriv(name,autocreate);
            m_tagsIconCache.emplace(std::piecewise_construct,std::forward_as_tuple(hash),std::forward_as_tuple(name,tags,icon));
            return icon;
        }
    }

    // find icon in tags contexts
    auto it=bestTagsContext->m_icons.find(name);
    if (it!=bestTagsContext->m_icons.end())
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
    auto actualName=nameMapping(bestTagsContext,name);

    // create new icon if not found in cache

    // find icon path
    auto path=namePath(bestTagsContext,actualName);
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

    const colorMapsT* maps=contextColorMaps(bestTagsContext,nameContext(name));

    // make icon
    auto icon=std::make_shared<SvgIcon>();
    icon->setName(name);
    auto ok=icon->addFile(path,*maps,m_defaultSizes);
    if (!ok)
    {
        return m_fallbackIcon;
    }

    // keep new icon in cache
    bestTagsContext->m_icons[name]=icon;
    m_tagsIconCache.emplace(std::piecewise_construct,std::forward_as_tuple(hash),std::forward_as_tuple(name,tags,icon));

    // done
    return icon;
}

//--------------------------------------------------------------------------

void SvgIconLocator::loadIcons(const std::vector<IconConfig>& iconConfigs, const QSet<QString> tags)
{
    for (const auto& iconConfig: iconConfigs)
    {
        bool insert=false;
        std::shared_ptr<SvgIcon> icon;
        if (tags.isEmpty())
        {
            icon=this->iconPriv(iconConfig.name,false);
        }
        else
        {
            icon=this->iconForTags(iconConfig.name,tags,false);
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
            if (tags.isEmpty())
            {
                m_icons[iconConfig.name]=icon;
            }
            else
            {
                // find tags context with all tags
                TagsContext* tagCtx=findTagContext(tags);

                // insert icon to tag context
                if (tagCtx==nullptr)
                {
                    qWarning() << "Tags contexts should be initialized befor loading icon configurations";
                    tagCtx=addTagContext(tags);
                }
                tagCtx->m_icons.emplace(iconConfig.name,icon);
                auto hash=nameAndTagsHash(iconConfig.name,tags);
                m_tagsIconCache.emplace(std::piecewise_construct,std::forward_as_tuple(hash),std::forward_as_tuple(iconConfig.name,tags,icon));
            }
        }
    }
}

//--------------------------------------------------------------------------

void SvgIconLocator::reset()
{
    clearIconDirs();
    clearCache();
    addIconDir(":/uise-icons");
}

//--------------------------------------------------------------------------

template <typename T>
void SvgIconLocator::loadSvgIconContext(T* tagCtx, const SvgIconContext& iconContext)
{
    for (const auto& alias: iconContext.aliases)
    {
        addNameMapping(tagCtx,alias.first,alias.second);
    }
    for (const auto& namePath: iconContext.namePaths)
    {
        addNamePath(tagCtx,namePath.first,namePath.second);
    }
    for (const auto& mode: iconContext.modes)
    {
        addColorMap(tagCtx,mode.second,iconContext.name,mode.first);
    }
    loadIcons(iconContext.icons,iconContext.tags);
}

//--------------------------------------------------------------------------

void SvgIconLocator::loadIconTheme(const SvgIconTheme& theme)
{
    for (const auto& context: theme.contexts())
    {
        if (context.second.tags.empty())
        {
            loadSvgIconContext(this,context.second);
        }
        else
        {
            auto tagCtx=findTagContext(context.second.tags);
            if (tagCtx==nullptr)
            {
                tagCtx=addTagContext(context.second.tags);
            }
            loadSvgIconContext(tagCtx,context.second);
        }
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
