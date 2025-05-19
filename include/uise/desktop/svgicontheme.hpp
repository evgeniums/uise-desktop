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
#include <uise/desktop/stylecontext.hpp>
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

        std::shared_ptr<SvgIcon> icon(const QString& name, bool autocreate) const;

        std::shared_ptr<SvgIcon> icon(const QString& name, const StyleContext& ={}) const;

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

        void addIconDir(QString path)
        {
            m_iconDirs.emplace_back(std::move(path));
        }

        const std::vector<QString>& iconDirss() const
        {
            return m_iconDirs;
        }        

        void addColorMap(SvgIcon::ColorMap map, const QString& context={}, IconVariant mode=IconMode::Normal)
        {
            if (context.isEmpty())
            {
                m_defaultColorMaps.emplace(mode,std::move(map));
            }
            else
            {
                colorMapsT* maps=nullptr;
                auto it=m_contextColorMaps.find(context);
                if (it!=m_contextColorMaps.end())
                {
                    maps=&it->second;
                }
                else
                {
                    auto it1=m_contextColorMaps.emplace(context,colorMapsT{});
                    maps=&it1.first->second;
                }
                maps->emplace(mode,std::move(map));
            }
        }

        const SvgIcon::ColorMap* colorMaps(IconVariant mode=IconMode::Normal, const QString& context={}) const
        {
            auto maps=contextColorMaps(context);
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

        const colorMapsT* contextColorMaps(const QString& context={}) const
        {
            if (context.isNull())
            {
                return &m_defaultColorMaps;
            }
            else
            {
                auto it=m_contextColorMaps.find(context);
                if (it!=m_contextColorMaps.end())
                {
                    return &it->second;
                }
            }
            return contextColorMaps();
        }

        colorMapsT* contextColorMaps(const QString& context={})
        {
            if (context.isNull())
            {
                return &m_defaultColorMaps;
            }
            else
            {
                auto it=m_contextColorMaps.find(context);
                if (it!=m_contextColorMaps.end())
                {
                    return &it->second;
                }
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
            m_fallbackIcon=icon(name,true);
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
        std::map<QString,colorMapsT> m_contextColorMaps;
        SizeSet m_defaultSizes;

        std::shared_ptr<SvgIcon> m_fallbackIcon;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_SVG_ICON_THEME_HPP
