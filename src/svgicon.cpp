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

/** @file uise/desktop/svgicon.hpp
*
*  Defines Icon.
*
*/

/****************************************************************************/

#include <QDebug>
#include <QFile>
#include <QPixmap>
#include <QPainter>

#include <QtSvg/QSvgRenderer>

#include <uise/desktop/svgicon.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

void SvgIcon::paint(QPainter *painter, const QRect &rect, IconVariant mode,  QIcon::State state, bool cache)
{
    // qDebug() << "SvgIcon::paint mode=" << mode << " state=" << state << " name="<<name();

    if (cache)
    {
        // try to find pixmap of requested size
        auto px=pixmap(rect.size(),mode,state);
        if (!px.isNull() && px.size()==rect.size())
        {
            painter->drawPixmap(rect,px);
            return;
        }
    }

    // render from content
    QByteArray content;
    if (state==QIcon::Off)
    {
        content=offContent(mode);
    }
    else
    {
        content=onContent(mode);
    }
    QSvgRenderer renderer(content);
    renderer.render(painter, rect);
}

//--------------------------------------------------------------------------

QPixmap SvgIcon::pixmap(const QSize &size, IconVariant mode,  QIcon::State state)
{
    // qDebug() << "SvgIcon::pixmap state="<<state << " mode="<<mode<< " name="<<m_name << " size=" << size;

    auto* set=pixmapSet(mode);
    if (set==nullptr || set->isNull())
    {
        // qDebug() << "SvgIcon::pixmap set not found="<<state<< " mode="<<mode<< " name="<<m_name;

        return makePixmap(size,mode,state);
    }

    auto px=set->pixmap(size,state);
    if (px.isNull() || px.size()!=size)
    {
        return makePixmap(size,mode,state);
    }
    return px;
}

//--------------------------------------------------------------------------

QPixmap SvgIcon::makePixmap(const QSize &size, IconVariant mode,  QIcon::State state, bool cache)
{
    // find content
    QByteArray content;
    if (state==QIcon::Off)
    {
        // qDebug() << "makePixmap for off mode="<<mode<< " name="<<m_name << " cache="<<cache;

        content=offContent(mode);
    }
    else
    {
        // qDebug() << "makePixmap for on mode="<<mode<< " name="<<m_name<< " cache="<<cache;

        content=onContent(mode);
    }

    // paint pixmap
    QImage img(size, QImage::Format_ARGB32);
    img.fill(qRgba(0, 0, 0, 0));
    auto px = QPixmap::fromImage(img, Qt::NoFormatConversion);
    {
        QPainter painter(&px);
        QRect r(QPoint(0.0, 0.0), size);
        paint(&painter, r, mode, state, false);
    }

    // qDebug() << "SvgIcon::makePixmap 1";

    // put pixmap to cache
    if (cache)
    {
        // qDebug() << "SvgIcon::makePixmap 2";

        auto set=pixmapSet(mode);
        if (set==nullptr)
        {
            // qDebug() << "makePixmap set not found mode="<<mode<< " name="<<m_name;
            // qDebug() << "makePixmap set added mode=" << mode << " name="<<m_name;

            auto it=m_pixmapSets.emplace(mode,IconPixmapSet{});
            set=&it.first->second;
        }
        // qDebug() << "makePixmap adding pixmap to set="<<set;
        set->addPixmap(px,state);
    }

    // done
    return px;
}

//--------------------------------------------------------------------------

bool SvgIcon::addFile(
        const QString& filename,
        const std::map<IconVariant,ColorMap>& colorMaps,
        const SizeSet& sizes
    )
{
    // qDebug() << "SvgIcon::addFile name=" << m_name;

    // load content from file
    QFile file(filename);
    if (!file.open(QFile::ReadOnly))
    {
        qWarning() << "Failed to open SVG file " << filename << ": " << file.errorString();
        return false;
    }
    auto content=file.readAll();
    QSvgRenderer renderer(content);
    if (!renderer.isValid())
    {
        qWarning() << "Invalid format of SVG file " << filename;
        return false;
    }

    // prepare content
    auto exec=[this,&sizes,&content](const std::map<IconVariant,ColorMap>& colorMaps)
    {
        for (const auto& colorMap : colorMaps)
        {
            m_initialContent.emplace(colorMap.first,content);

            if (!colorMap.second.on.empty())
            {
                // qDebug() << "SvgIcon::addFile color map on not empty";

                auto on=content;
                for (const auto& colorMapping : colorMap.second.on)
                {
                    on.replace(colorMapping.first.toLocal8Bit().data(),colorMapping.second.toLocal8Bit().data());
                }
                m_onContent.emplace(colorMap.first,on);

                // qDebug() << "m_onContent " << QString::fromUtf8(on) << " for mode " << colorMap.first;
            }
            else
            {
                // qDebug() << "SvgIcon::addFile color map on empty";
            }

            if (!colorMap.second.off.empty())
            {
                // qDebug() << "SvgIcon::addFile color map off not empty";

                auto off=content;
                for (const auto& colorMapping : colorMap.second.off)
                {
                    off.replace(colorMapping.first.toLocal8Bit().data(),colorMapping.second.toLocal8Bit().data());
                }
                m_offContent.emplace(colorMap.first,off);

                // qDebug() << "m_offContent " << QString::fromUtf8(off) << " for mode " << colorMap.first;
            }
            else
            {
                // qDebug() << "SvgIcon::addFile color map off empty";
            }

            // fill cache of pixmaps for all sizes
            for (const auto& size: sizes)
            {
                if (!colorMap.second.on.empty())
                {
                    makePixmap(size,colorMap.first,QIcon::On);
                }
                if (!colorMap.second.off.empty())
                {
                    makePixmap(size,colorMap.first,QIcon::Off);
                }
            }
        }
    };

    // qDebug() << "m_initialContent " << QString::fromUtf8(content);

    if (colorMaps.empty())
    {
        // qDebug() << "SvgIcon::addFile color maps empty";
        std::map<IconVariant,ColorMap> colorMaps{
            {IconMode::Normal,{}}
        };
        exec(colorMaps);
    }
    else
    {
        // qDebug() << "SvgIcon::addFile color maps not empty";
        exec(colorMaps);
    }

    // done
    return true;
}

//--------------------------------------------------------------------------

QIcon SvgIcon::icon()
{
    return QIcon(new SvgIconEngine(shared_from_this()));
}

//--------------------------------------------------------------------------

QIcon SvgIcon::hoverIcon()
{
    return QIcon(new SvgIconEngine(shared_from_this(),true));
}

//--------------------------------------------------------------------------

void SvgIcon::reload(
        const std::map<IconVariant,ColorMap>& colorMaps
    )
{
    m_pixmapSets.clear();

    for (const auto& modeColorMap : colorMaps)
    {
        auto content=initialContent(modeColorMap.first);

        if (!modeColorMap.second.on.empty())
        {
            auto on=content;
            for (const auto& colorMapping : modeColorMap.second.on)
            {
                on.replace(colorMapping.first.toLocal8Bit().data(),colorMapping.second.toLocal8Bit().data());
            }
            m_onContent.emplace(modeColorMap.first,on);
        }

        if (!modeColorMap.second.off.empty())
        {
            auto off=content;
            for (const auto& colorMapping : modeColorMap.second.off)
            {
                off.replace(colorMapping.first.toLocal8Bit().data(),colorMapping.second.toLocal8Bit().data());
            }
            m_offContent.emplace(modeColorMap.first,off);
        }
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
