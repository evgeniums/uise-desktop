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

/** @file uise/desktop/icon.hpp
*
*  Defines Icon.
*
*/

/****************************************************************************/

#include <QFile>
#include <QPixmap>

#include <uise/desktop/icon.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

bool Icon::loadSvg(
        const QString& filename,
        const SizeSet& sizes,
        const std::map<IconState,std::map<QString,QString>>& colorMaps
    )
{
    QFile file(filename);
    if (file.open(QIODevice::ReadOnly))
    {
        QByteArray buf = file.readAll();

        for (const auto& state: colorMaps)
        {
            auto stateBuf=buf;
            for (const auto& colorMap: state.second)
            {
                stateBuf.replace(colorMap.first.toLocal8Bit().data(),colorMap.second.toLocal8Bit().data());
            }
            for (const auto& size: sizes)
            {
                QPixmap px{size};
                if (px.loadFromData(stateBuf,"svg"))
                {
                    add(px,state.first);
                }
                else
                {
                    reset();
                    return false;
                }
            }
        }
    }
    else
    {
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------

bool Icon::loadMultistateSvg(
        const QString& filename,
        const std::map<IconState,std::map<QString,QString>>& colorMaps,
        const SizeSet& sizes
    )
{
    QFile file(filename);
    if (file.open(QIODevice::ReadOnly))
    {
        QByteArray buf = file.readAll();

        for (const auto& state: colorMaps)
        {
            auto stateBuf=buf;
            for (const auto& colorMap: state.second)
            {
                stateBuf.replace(colorMap.first.toLocal8Bit().data(),colorMap.second.toLocal8Bit().data());
            }
            for (const auto& size: sizes)
            {
                QPixmap px{size};
                if (px.loadFromData(stateBuf,"svg"))
                {
                    addMultistate(px,state.first);
                }
                else
                {
                    reset();
                    return false;
                }
            }
        }
    }
    else
    {
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
