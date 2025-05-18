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
*  Defines IconTheme.
*
*/

/****************************************************************************/

#include <filesystem>

#include <QDebug>
#include <QFile>

#include <uise/desktop/icontheme.hpp>

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
        if (std::filesystem::exists(p))
        {
            return QString::fromStdString(p.string());
        }
    }

    return QString{};
}

}

//-------------------------------------------------------------------------- 

std::shared_ptr<Icon> IconTheme::icon(const QString& name) const
{
    auto actualName=nameMapping(name);
    auto it=m_icons.find(actualName);
    if (it!=m_icons.end())
    {
        return it->second;
    }

    auto path=namePath(name);
    if (path.isEmpty())
    {
        path=existingFileName(name,extension(),m_iconDirs);
        if (path.isEmpty())
        {
            qWarning() << "Failed to find icon file " << path;
            return m_fallbackIcon;
        }
    }

    auto icon=std::make_shared<Icon>();
    auto ok=icon->loadSvg(path,m_defaultSizes,m_colorMaps);
    if (!ok)
    {
        qWarning() << "Failed to load icon file " << path;
        return m_fallbackIcon;
    }

    m_icons[name]=icon;

    return icon;
}

//--------------------------------------------------------------------------

std::shared_ptr<Icon> IconTheme::multistateIcon(const QString& name) const
{
    auto actualName=nameMapping(name);
    auto it=m_multistateIcons.find(actualName);
    if (it!=m_multistateIcons.end())
    {
        return it->second;
    }

    auto path=namePath(name);
    if (path.isEmpty())
    {
        path=existingFileName(name,extension(),m_iconDirs);
        if (path.isEmpty())
        {
            qWarning() << "Failed to find multistate icon file " << path;
            return m_fallbackIcon;
        }
    }

    auto icon=std::make_shared<Icon>();
    auto ok=icon->loadMultistateSvg(path,m_colorMaps,m_defaultSizes);
    if (!ok)
    {
        qWarning() << "Failed to load multistate icon file " << path;
        return m_fallbackIcon;
    }

    m_multistateIcons[name]=icon;

    return icon;
}

//--------------------------------------------------------------------------

void IconTheme::preloadIcons(const std::vector<IconConfig>& iconConfigs)
{
    for (const auto& iconConfig: iconConfigs)
    {
        // find file path
        auto path=namePath(iconConfig.name);
        if (path.isEmpty())
        {
            path=existingFileName(iconConfig.name,extension(),m_iconDirs);
            if (path.isEmpty())
            {
                qWarning() << "Failed to find icon file " << path;
                continue;
            }
        }

        // prepare sizes
        auto sizes=m_defaultSizes;
        if (!iconConfig.sizes.empty())
        {
            sizes=iconConfig.sizes;
        }

        // prepare color maps
        std::map<IconState,std::map<QString,QString>> colorMaps;
        for (const auto& it1: iconConfig.states)
        {
            auto it2=m_colorMaps.find(it1);
            if (it2!=m_colorMaps.end())
            {
                colorMaps.emplace(it1,it2->second);
            }
        }

        auto icon=std::make_shared<Icon>();

        // load monostate icon
        if (iconConfig.monostate)
        {
            auto ok=icon->loadSvg(path,sizes,colorMaps);
            if (!ok)
            {
                qWarning() << "Failed to load icon file " << path;
            }
            else
            {
                m_icons[iconConfig.name]=icon;
            }
        }

        // load multistate icon
        if (iconConfig.multistate)
        {
            auto ok=icon->loadMultistateSvg(path,colorMaps,sizes);
            if (!ok)
            {
                qWarning() << "Failed to load multistate icon file " << path;
            }
            else
            {
                m_multistateIcons[iconConfig.name]=icon;
            }
        }
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
