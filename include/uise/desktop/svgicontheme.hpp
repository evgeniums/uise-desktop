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

/** @file uise/desktop/svgicontheme.hpp
*
*  Declares SVG icon theme class.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_SVG_ICON_THEME_HPP
#define UISE_DESKTOP_SVG_ICON_THEME_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/svgicon.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT SvgIconTheme
{
    using colorMapsT=std::map<IconVariant,SvgIcon::ColorMap>;

    public:

        struct IconConfig
        {
            using aliasModesT=std::map<QString,std::set<IconVariant>>;

            QString name;
            aliasModesT aliasModes;
            colorMapsT colorMaps;
            SizeSet sizes;

            IconConfig(QString name, colorMapsT colorMaps=colorMapsT{}, aliasModesT aliasModes={}, SizeSet sizes={})
                : name(std::move(name)),
                  colorMaps(std::move(colorMaps)),
                  aliasModes(std::move(aliasModes)),
                  sizes(std::move(sizes))
            {}
        };

        std::shared_ptr<SvgIcon> icon(const QString& name, bool autocreate=true) const;

        void loadIcons(const std::vector<IconConfig>& iconConfigs);

        void addNameMapping(
            QString alias,
            QString name
        )
        {
            m_namesMap.emplace(std::move(alias),std::move(name));
        }

        QString nameMapping(const QString& alias) const
        {
            auto it=m_namesMap.find(alias);
            if (it!=m_namesMap.end())
            {
                return it->second;
            }
            return alias;
        }

        void addNamePath(
            QString name,
            QString path
            )
        {
            m_namePaths.emplace(std::move(name),std::move(path));
        }

        QString namePath(const QString& name) const
        {
            auto it=m_namePaths.find(name);
            if (it!=m_namePaths.end())
            {
                return it->second;
            }
            return QString{};
        }

        void addIconPath(QString path)
        {
            m_iconDirs.emplace_back(std::move(path));
        }

        const std::vector<QString>& iconPaths() const
        {
            return m_iconDirs;
        }        

        void addColorMap(SvgIcon::ColorMap map, IconVariant mode=IconMode::Normal)
        {
            m_defaultColorMaps.emplace(mode,std::move(map));
        }

        const SvgIcon::ColorMap* colorMaps(IconVariant mode=IconMode::Normal) const
        {
            auto it=m_defaultColorMaps.find(mode);
            if (it!=m_defaultColorMaps.end())
            {
                return &it->second;
            }
            return nullptr;
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
            m_fallbackIcon=icon(name);
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
            for (auto& it:m_icons)
            {
                it.second->reload(m_defaultColorMaps);
            }
        }

    private:

        mutable std::map<QString,std::shared_ptr<SvgIcon>> m_icons;

        std::vector<QString> m_iconDirs;
        std::map<QString,QString> m_namesMap;
        std::map<QString,QString> m_namePaths;

        std::map<QString,colorMapsT> m_colorMaps;

        colorMapsT m_defaultColorMaps;
        SizeSet m_defaultSizes;

        std::shared_ptr<SvgIcon> m_fallbackIcon;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_SVG_ICON_THEME_HPP
