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

/** @file uise/desktop/icontheme.hpp
*
*  Declares icon theme class.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ICON_THEME_HPP
#define UISE_DESKTOP_ICON_THEME_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/icon.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT IconTheme
{
    public:

        struct IconConfig
        {
            QString name;
            std::set<IconState> states;
            SizeSet sizes;
            bool multistate=false;
            bool monostate=true;

            IconConfig(QString name, std::set<IconState> states={}, SizeSet sizes={})
                : name(std::move(name)),
                  states(std::move(states)),
                  sizes(std::move(sizes))
            {
                if (this->states.empty())
                {
                    this->states.emplace(IconMode::Normal);
                }
            }

            IconConfig(QString name, IconState state, SizeSet sizes={})
                : IconConfig(std::move(name),std::set<IconState>{state},std::move(sizes))
            {}
        };

        std::shared_ptr<Icon> icon(const QString& name) const;
        std::shared_ptr<Icon> multistateIcon(const QString& name) const;

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

        void preloadIcons(const std::vector<IconConfig>& iconConfigs);

        void addColorMap(IconState state,std::map<QString,QString> map)
        {
            m_colorMaps.emplace(state,std::move(map));
        }

        const std::map<QString,QString>* colorMap(IconState state) const
        {
            auto it=m_colorMaps.find(state);
            if (it!=m_colorMaps.end())
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

        void setFallbackIcon(std::shared_ptr<Icon> icon)
        {
            m_fallbackIcon=std::move(icon);
        }

        std::shared_ptr<Icon> fallbackIcon() const
        {
            return m_fallbackIcon;
        }

        QString extension() const
        {
            return "svg";
        }

    private:

        mutable std::map<QString,std::shared_ptr<Icon>> m_icons;
        mutable std::map<QString,std::shared_ptr<Icon>> m_multistateIcons;

        std::vector<QString> m_iconDirs;
        std::map<QString,QString> m_namesMap;
        std::map<QString,QString> m_namePaths;

        std::map<IconState,std::map<QString,QString>> m_colorMaps;
        SizeSet m_defaultSizes;

        std::shared_ptr<Icon> m_fallbackIcon;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ICON_THEME_HPP
