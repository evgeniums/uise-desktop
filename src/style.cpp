/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/desktop/src/style.cpp
*
*  Defines Style.
*
*/

/****************************************************************************/

#include <QWidget>
#include <QApplication>
#include <QPalette>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include <uise/desktop/style.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

const char* UiseIconPath=":/uise/desktop/icons";

}

//--------------------------------------------------------------------------
Style::Style(
    ) : m_fallbackIconPath(UiseIconPath),
        m_darkTheme(false),
        m_darkStyleSheetMode(StyleSheetMode::Auto)
{}

//--------------------------------------------------------------------------
bool Style::checkDarkTheme()
{
    auto palette = qApp->palette("QLabel");
    auto textColor = palette.color(QPalette::WindowText).value();
    auto bgColor = palette.color(QPalette::Window).value();
    m_darkTheme = textColor > bgColor;

    return m_darkTheme;
}

//--------------------------------------------------------------------------
Style& Style::instance()
{
    static Style inst;
    return inst;
}

//--------------------------------------------------------------------------
void Style::reloadStyleSheet()
{
    // check dark theme
    auto darkTheme=false;
    if (m_darkStyleSheetMode==StyleSheetMode::Auto)
    {
        darkTheme=checkDarkTheme();
    }
    else if (m_darkStyleSheetMode==StyleSheetMode::Dark)
    {
        darkTheme=true;
    }

    // check if folder exists for dark theme
    auto folderPath=m_styleSheetPath;
    if (darkTheme)
    {
        auto tryDarkFolderPath=QString("%1/dark").arg(folderPath);
        if (QFileInfo::exists(tryDarkFolderPath))
        {
            folderPath=tryDarkFolderPath;
        }
    }

    // collect style sheets from files in folder
    m_loadedStyleSheet.clear();
    QDir stylesDir(folderPath);
    QStringList filters;
    filters << "*.css" << "*.qss";
    stylesDir.setNameFilters(filters);
    auto items=stylesDir.entryInfoList(QDir::Files);
    for (auto&& item:items)
    {
        QFile file(item.canonicalFilePath());
        if (file.open(QFile::ReadOnly))
        {
            auto data=file.readAll();
            if (!data.isEmpty())
            {
                m_loadedStyleSheet+=QString("%1\n").arg(QString::fromUtf8(data));
            }
        }
    }

    // apply color map
    auto styleSheet=m_loadedStyleSheet;
    for (auto&& color: m_colorMap)
    {
        styleSheet.replace(color.first,color.second,Qt::CaseInsensitive);
    }
    setStyleSheet(styleSheet);
}

//--------------------------------------------------------------------------
QIcon Style::icon(const QString &name, const QString &ext) const
{
    QIcon fallback;
    if (!m_fallbackIconPath.isEmpty())
    {
        auto fallbackPath=QString("%1/%2.%3").arg(m_fallbackIconPath,name,ext);
        if (m_darkTheme)
        {
            auto darkFallbackPath=QString("%1/dark").arg(m_fallbackIconPath);
            auto darkIconPath=QString("%1/%2.%3").arg(darkFallbackPath,name,ext);
            if (QFile::exists(darkIconPath))
            {
                fallbackPath=darkIconPath;
            }
        }
        fallback=QIcon(fallbackPath);
    }
    return QIcon::fromTheme(name,fallback);
}

//--------------------------------------------------------------------------
void Style::applyStyleSheet(QWidget *widget)
{
    if (widget==nullptr)
    {
        qApp->setStyleSheet(m_styleSheet);
    }
    else
    {
        widget->setStyleSheet(m_styleSheet);
        checkDarkTheme();
    }
}

//--------------------------------------------------------------------------
void Style::prependIconThemeSearchPath(const QString &path)
{
    auto paths =QIcon::themeSearchPaths();
    paths.prepend(path);
    QIcon::setThemeSearchPaths(paths);
}

//--------------------------------------------------------------------------
QString Style::firstIconThemeSearchPath()
{
    auto paths=QIcon::themeSearchPaths();
    if (!paths.isEmpty())
    {
        return paths.first();
    }
    return QString();
}

//--------------------------------------------------------------------------
void Style::setIconThemeName(const QString &name)
{
    QIcon::setThemeName(name);
}

//--------------------------------------------------------------------------
QString Style::iconThemeName()
{
    return QIcon::themeName();
}

//--------------------------------------------------------------------------
void Style::prependIconThemeFallbackPath(const QString &path)
{
    auto paths =QIcon::fallbackSearchPaths();
    paths.prepend(path);
    QIcon::setFallbackSearchPaths(paths);
}

//--------------------------------------------------------------------------
QString Style::firstIconThemeFallbackPath()
{
    auto paths=QIcon::fallbackSearchPaths();
    if (!paths.isEmpty())
    {
        return paths.first();
    }
    return QString();
}

//--------------------------------------------------------------------------
void Style::setIconThemeFallbackName(const QString &name)
{
    QIcon::setFallbackThemeName(name);
}

//--------------------------------------------------------------------------
QString Style::iconThemeFallbackName()
{
    return QIcon::fallbackThemeName();
}

//--------------------------------------------------------------------------
bool Style::loadColorMap(const QString &fileName, QString* errMsg)
{
    QString error;
    try
    {
        // check if file exists
        if (!QFileInfo::exists(fileName))
        {
            error="file does not exists";
            throw std::exception();
        }

        // read file
        QFile file(fileName);
        if (!file.open(QFile::ReadOnly))
        {
            error=file.errorString();
            throw std::exception();
        }
        auto data=file.readAll();

        // parse document
        auto doc=QJsonDocument::fromJson(data);
        if (doc.isNull())
        {
            error="file is empty";
            throw std::exception();
        }
        if (!doc.isObject())
        {
            error="not a JSON object";
            throw std::exception();
        }
        auto obj=doc.object();

        // find colors section
        auto colorsIt=obj.find("colors");
        if (colorsIt==obj.end())
        {
            error="no colors section in JSON document";
            throw std::exception();
        }
        auto colorsVal=colorsIt.value();
        if (!colorsVal.isObject())
        {
            error="colors section is not a JSON object";
            throw std::exception();
        }
        auto colors=colorsVal.toObject();

        // read all colors
        std::map<QString,QString> colorMap;
        for (auto it=colors.begin();it!=colors.end();++it)
        {
            auto key=it.key();
            key=key.trimmed();
            if (key.isEmpty())
            {
                error=QString("color name can not be empty");
                throw std::exception();
            }

            auto val=it.value();
            if (!val.isString())
            {
                error=QString("color %1 value is not a string");
                throw std::exception();
            }
            colorMap[it.key()]=val.toString();
        }

        // set color map
        setColorMap(colorMap);
    }
    catch (...)
    {
        if (errMsg!=nullptr)
        {
            *errMsg=error;
        }
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------
void Style::reset()
{
    m_styleSheet.clear();
    m_baseStyleSheet.clear();
    m_loadedStyleSheet.clear();
    m_styleSheetPath.clear();
    m_fallbackIconPath=UiseIconPath;

    m_darkTheme=false;
    m_darkStyleSheetMode=StyleSheetMode::Auto;

    m_colorMap.clear();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
