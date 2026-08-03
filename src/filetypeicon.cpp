/**
@copyright Evgeny Sidorov 2026

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/src/filetypeicon.cpp
*
*  Defines fileTypeIcon() and fileTypeIconName().
*
*/

/****************************************************************************/

#include <map>

#include <QFile>
#include <QWidget>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/filetypeicon.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

QString fileTypeIconName(const QString& suffix)
{
    static const std::map<QString,QString> mapping{
        {QStringLiteral("pdf"),QStringLiteral("pdf")},
        {QStringLiteral("doc"),QStringLiteral("doc")},
        {QStringLiteral("docx"),QStringLiteral("docx")},
        {QStringLiteral("xls"),QStringLiteral("xls")},
        {QStringLiteral("xlsx"),QStringLiteral("xls")},
        {QStringLiteral("ppt"),QStringLiteral("ppt")},
        {QStringLiteral("pptx"),QStringLiteral("ppt")},
        {QStringLiteral("txt"),QStringLiteral("txt")},
        {QStringLiteral("csv"),QStringLiteral("csv")},
        {QStringLiteral("zip"),QStringLiteral("zip")},
        {QStringLiteral("rar"),QStringLiteral("zip")},
        {QStringLiteral("7z"),QStringLiteral("zip")},
        {QStringLiteral("html"),QStringLiteral("html")},
        {QStringLiteral("htm"),QStringLiteral("html")},
        {QStringLiteral("css"),QStringLiteral("css")},
        {QStringLiteral("js"),QStringLiteral("js")},
        {QStringLiteral("jsx"),QStringLiteral("jsx")},
        {QStringLiteral("ts"),QStringLiteral("ts")},
        {QStringLiteral("tsx"),QStringLiteral("tsx")},
        {QStringLiteral("xml"),QStringLiteral("xml")},
        {QStringLiteral("svg"),QStringLiteral("svg")},
        {QStringLiteral("php"),QStringLiteral("php")},
        {QStringLiteral("sql"),QStringLiteral("sql")},
        {QStringLiteral("rs"),QStringLiteral("rs")},
        {QStringLiteral("vue"),QStringLiteral("vue")}
    };
    auto it=mapping.find(suffix);
    return (it!=mapping.end()) ? it->second : QString();
}

//--------------------------------------------------------------------------

std::shared_ptr<SvgIcon> fileTypeIcon(const QString& suffix, QWidget* context, const QString& fallbackAlias)
{
    auto name=fileTypeIconName(suffix);
    if (name.isEmpty())
    {
        return Style::instance().svgIconLocator().icon(fallbackAlias,context);
    }

    auto path=QString(":/icons/tabler-icons/outline/file-type-%1.svg").arg(name);
    if (!QFile::exists(path))
    {
        return Style::instance().svgIconLocator().icon(fallbackAlias,context);
    }

    // these are plain tabler outline icons (stroke="currentColor"), loaded by resource path
    // rather than through a named context alias, so the usual JSON-driven per-mode color
    // resolution does not apply -- substitute currentColor by hand instead, for the theme
    // active right now. Unlike alias-resolved icons this bakes the color in at construction
    // time: it will not repaint itself on a later theme toggle until the caller rebuilds it.
    // isDarkTheme(), not checkDarkTheme(): the latter always re-detects the OS/application
    // palette live and ignores an explicitly-set uise style mode, so it would keep reporting
    // the OS theme even after the app is switched to the other one via setStyleSheetMode()
    auto color=Style::instance().isDarkTheme() ? QStringLiteral("#CCCCCC") : QStringLiteral("#444444");
    std::map<QString,QString> substitution{{QStringLiteral("currentColor"),color}};
    SvgIcon::ColorMap colorMap(substitution);
    std::map<IconVariant,SvgIcon::ColorMap> colorMaps{{IconMode::Normal,colorMap}};

    auto icon=std::make_shared<SvgIcon>();
    icon->addFile(path,colorMaps);
    return icon;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
