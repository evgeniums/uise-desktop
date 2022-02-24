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

#include <uise/desktop/utils/substitutecolors.hpp>

#include <uise/desktop/style.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

const char* UiseIconPath=":/uise/desktop/icons";

}

//--------------------------------------------------------------------------
Style::Style(
    ) : m_darkTheme(false),
        m_darkStyleSheetMode(StyleSheetMode::Auto)
{
    resetFallbackIconPaths();
    resetStyleSheetPaths();
}

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

    // load style sheets from files at paths
    for (auto&& folderPath:m_styleSheetPaths)
    {
        // check if folder exists for dark theme
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
    }

    // apply color map
    if (m_colorMap.empty())
    {
        setStyleSheet(m_loadedStyleSheet);
    }
    else
    {
        auto loadedStyleSheet=m_loadedStyleSheet;
        loadedStyleSheet=loadedStyleSheet.toLower();
        std::map<std::string,std::string> colorMap;
        for (auto&& it:m_colorMap)
        {
            colorMap[it.first.toLower().toStdString()]=it.second.toLower().toStdString();
        }
        auto styleSheet=substituteColors(loadedStyleSheet.toStdString(),colorMap);
        setStyleSheet(QString::fromStdString(styleSheet));
    }
}

//--------------------------------------------------------------------------
QIcon Style::icon(const QString &name, const QString &ext) const
{
    auto findPath=[this,&name,&ext](const QString withDark)
    {
        for (auto&& path:m_fallbackIconPaths)
        {
            auto iconPath=QString("%1%2/%3.%4").arg(path,withDark,name,ext);
            if (QFile::exists(iconPath))
            {
                return iconPath;
            }
        }
        return QString();
    };

    QString fallbackPath;
    if (m_darkTheme)
    {
        fallbackPath=findPath("/dark");
    }
    if (fallbackPath.isEmpty())
    {
        fallbackPath=findPath("");
    }

    if (!fallbackPath.isEmpty())
    {
        return QIcon::fromTheme(name,QIcon(fallbackPath));
    }

    return QIcon::fromTheme(name);
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

    m_darkTheme=false;
    m_darkStyleSheetMode=StyleSheetMode::Auto;

    m_colorMap.clear();
    resetFallbackIconPaths();
    resetStyleSheetPaths();
}

//--------------------------------------------------------------------------
void Style::resetFallbackIconPaths()
{
    m_fallbackIconPaths.clear();
    m_fallbackIconPaths.push_back(UiseIconPath);
}

//--------------------------------------------------------------------------
void Style::resetStyleSheetPaths()
{
    m_styleSheetPaths.clear();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
