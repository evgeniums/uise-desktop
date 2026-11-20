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
        {QStringLiteral("pdf"),QStringLiteral("file-type-pdf")},
        {QStringLiteral("doc"),QStringLiteral("file-type-doc")},
        {QStringLiteral("docx"),QStringLiteral("file-type-docx")},
        {QStringLiteral("xls"),QStringLiteral("file-type-xls")},
        {QStringLiteral("xlsx"),QStringLiteral("file-type-xls")},
        {QStringLiteral("ppt"),QStringLiteral("file-type-ppt")},
        {QStringLiteral("pptx"),QStringLiteral("file-type-ppt")},
        {QStringLiteral("txt"),QStringLiteral("file-type-txt")},
        {QStringLiteral("csv"),QStringLiteral("file-type-csv")},
        // zip has its own "ZIP" glyph; every other archive format gets the format-neutral
        // "file-zip" so it is not mislabelled as a zip (tabler has no rar/7z/tar/... glyphs)
        {QStringLiteral("zip"),QStringLiteral("file-type-zip")},
        {QStringLiteral("rar"),QStringLiteral("file-zip")},
        {QStringLiteral("7z"),QStringLiteral("file-zip")},
        {QStringLiteral("tar"),QStringLiteral("file-zip")},
        {QStringLiteral("gz"),QStringLiteral("file-zip")},
        {QStringLiteral("tgz"),QStringLiteral("file-zip")},
        {QStringLiteral("bz2"),QStringLiteral("file-zip")},
        {QStringLiteral("tbz"),QStringLiteral("file-zip")},
        {QStringLiteral("tbz2"),QStringLiteral("file-zip")},
        {QStringLiteral("xz"),QStringLiteral("file-zip")},
        {QStringLiteral("txz"),QStringLiteral("file-zip")},
        {QStringLiteral("zst"),QStringLiteral("file-zip")},
        {QStringLiteral("lz"),QStringLiteral("file-zip")},
        {QStringLiteral("lzma"),QStringLiteral("file-zip")},
        {QStringLiteral("cab"),QStringLiteral("file-zip")},
        {QStringLiteral("html"),QStringLiteral("file-type-html")},
        {QStringLiteral("htm"),QStringLiteral("file-type-html")},
        {QStringLiteral("css"),QStringLiteral("file-type-css")},
        {QStringLiteral("js"),QStringLiteral("file-type-js")},
        {QStringLiteral("jsx"),QStringLiteral("file-type-jsx")},
        {QStringLiteral("ts"),QStringLiteral("file-type-ts")},
        {QStringLiteral("tsx"),QStringLiteral("file-type-tsx")},
        {QStringLiteral("xml"),QStringLiteral("file-type-xml")},
        {QStringLiteral("svg"),QStringLiteral("file-type-svg")},
        {QStringLiteral("php"),QStringLiteral("file-type-php")},
        {QStringLiteral("sql"),QStringLiteral("file-type-sql")},
        {QStringLiteral("rs"),QStringLiteral("file-type-rs")},
        {QStringLiteral("vue"),QStringLiteral("file-type-vue")}
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

    auto path=QString(":/icons/tabler-icons/outline/%1.svg").arg(name);
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
