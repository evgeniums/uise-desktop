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

/** @file uise/desktop/svgiconcontext.hpp
*
*  Declares SVG icon context classes.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_SVG_ICON_CONTEXT_HPP
#define UISE_DESKTOP_SVG_ICON_CONTEXT_HPP

#include <optional>

#include <QObject>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/svgicon.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

using SvgIconColorMaps=std::map<IconVariant,SvgIcon::ColorMap>;

struct SvgIconConfig
{
    using aliasModesT=std::map<QString,std::set<IconVariant>>;

    QString name;
    aliasModesT aliases;
    SvgIconColorMaps modes;
    SizeSet sizes;

    SvgIconConfig(QString name, SvgIconColorMaps modes=SvgIconColorMaps{}, aliasModesT aliases={}, SizeSet sizes={})
        : name(std::move(name)),
        modes(std::move(modes)),
        aliases(std::move(aliases)),
        sizes(std::move(sizes))
    {}
};

struct UISE_DESKTOP_EXPORT SvgIconContext
{
    QString name;

    QSet<QString> tags;
    std::map<QString,QString> aliases;
    std::map<QString,QString> namePaths;
    SvgIconColorMaps modes;
    std::vector<SvgIconConfig> icons;
};

class WithModesMap
{
    public:

        WithModesMap():m_modeMap(defaultModeMap())
        {}

        void setModesMap(std::map<QString,IconMode> modeMap)
        {
            m_modeMap=std::move(modeMap);
        }

        const std::map<QString,IconMode>& modeMap() const
        {
            return m_modeMap;
        }

        std::optional<IconVariant> mode(const QString& modeName) const
        {
            auto it=m_modeMap.find(modeName);
            if (it!=m_modeMap.end())
            {
                return it->second;
            }
            return std::optional<IconVariant>{};
        }

    private:

        std::map<QString,IconMode> m_modeMap;
};

class SvgIconLocator;

class SvgIconTheme : public WithModesMap
{
    public:

        SvgIconTheme()
        {}

        bool loadFromJson(const QString& json, QString* errorMessage=nullptr);

        QString name() const
        {
            return m_name;
        }

        const auto& contexts() const
        {
            return m_contexts;
        }

    private:

        QString m_name;
        std::map<QString,SvgIconContext> m_contexts;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_SVG_ICON_CONTEXT_HPP
