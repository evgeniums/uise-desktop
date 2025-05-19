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
*  Defines SvgIconTheme.
*
*/

/****************************************************************************/

#include <filesystem>

#include <QDebug>
#include <QFile>

#include <uise/desktop/svgicontheme.hpp>

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

}

//-------------------------------------------------------------------------- 

std::shared_ptr<SvgIcon> SvgIconTheme::icon(const QString& name, bool autocreate) const
{
    // find actual name
    auto actualName=nameMapping(name);

    // find icon in cache
    auto it=m_icons.find(actualName);
    if (it!=m_icons.end())
    {
        qDebug() << "Icon found " << name;

        return it->second;
    }
    qDebug() << "Icon not found " << name;

    if (!autocreate)
    {
        return std::shared_ptr<SvgIcon>{};
    }

    // create new icon if not found in cache

    // find icon path
    auto path=namePath(actualName);
    if (path.isEmpty())
    {
        path=existingFileName(actualName,extension(),m_iconDirs);
        if (path.isEmpty())
        {
            qWarning() << "Failed to find icon file " << path;
            return m_fallbackIcon;
        }
    }

    // make icon
    auto icon=std::make_shared<SvgIcon>();
    auto ok=icon->addFile(path,m_defaultColorMaps,m_defaultSizes);
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

void SvgIconTheme::loadIcons(const std::vector<IconConfig>& iconConfigs)
{
    for (const auto& iconConfig: iconConfigs)
    {
        bool insert=false;
        auto icon=this->icon(iconConfig.name,false);
        if (!icon)
        {
            insert=true;
            icon=std::make_shared<SvgIcon>();
        }

        // prepare sizes
        auto sizes=m_defaultSizes;
        if (!iconConfig.sizes.empty())
        {
            sizes=iconConfig.sizes;
        }

        // adjust if alias modes are empty
        auto aliasModes=iconConfig.aliasModes;
        if (aliasModes.empty())
        {
            aliasModes.emplace("",std::set<IconVariant>{});
        }

        // prepare color maps
        auto colorMaps=iconConfig.colorMaps;
        if (colorMaps.empty())
        {
            colorMaps=m_defaultColorMaps;
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
            m_icons[iconConfig.name]=icon;
        }
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
