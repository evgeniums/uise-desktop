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

#include <uise/desktop/htree.hpp>

#include <uise/desktop/utils/substitutecolors.hpp>
#include <uise/desktop/style.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

const char* UiseIconPath=":/uise/desktop/icons";
const char* UiseQssPath=":/uise/desktop/qss";

}

//--------------------------------------------------------------------------
Style::Style(
    ) : m_darkTheme(false),
        m_darkStyleSheetMode(StyleSheetMode::Auto)
{
    resetFallbackIconPaths();
    resetStyleSheetPaths();
    resetSvgIconTheme();
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
    m_loadedStyleSheet.clear();

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

    // list non-color style files
    QStringList files;
    for (auto&& folderPath:m_styleSheetPaths)
    {
        QDir stylesDir(folderPath);
        QStringList filters;
        filters << "*.css" << "*.qss";
        stylesDir.setNameFilters(filters);
        auto items=stylesDir.entryInfoList(QDir::Files);
        for (auto&& item:items)
        {
            files.append(item.canonicalFilePath());
        }
    }

    // list color style files
    for (auto&& folderPath:m_styleSheetPaths)
    {
        QString defaultColorTheme;
        if (darkTheme)
        {
            defaultColorTheme="dark";
        }
        else
        {
            defaultColorTheme="light";
        }
        auto colorTheme=m_colorThemeName;
        if (colorTheme.isEmpty())
        {
            colorTheme=defaultColorTheme;
        }

        auto defaultColorThemePath=QString("%1/%2").arg(folderPath,defaultColorTheme);
        auto themePath=QString("%1/%2").arg(folderPath,colorTheme);

        QStringList filters;
        filters << "*.css" << "*.qss";

        // list deafult color theme files for dark or light
        QSet<QString> defaultFiles;
        QDir defaultStylesDir(defaultColorThemePath);
        defaultStylesDir.setNameFilters(filters);
        auto items=defaultStylesDir.entryInfoList(QDir::Files);
        for (auto&& item:items)
        {
            defaultFiles.insert(item.fileName());
        }

        // list current theme files
        if (colorTheme!=defaultColorTheme)
        {
            QDir themeStylesDir(themePath);
            themeStylesDir.setNameFilters(filters);
            items=themeStylesDir.entryInfoList(QDir::Files);
            for (auto&& item:items)
            {
                auto file=item.canonicalFilePath();
                if (defaultFiles.contains(file))
                {
                    // override default file with file in current color theme
                    auto fileName=item.fileName();
                    defaultFiles.remove(fileName);
                }
                files.append(file);
            }
        }

        // append default color theme files that were not overriden in current color theme
        for (auto&& file:defaultFiles)
        {
            files.append(defaultColorThemePath+"/"+file);
        }
    }

    // load style sheets
    for (auto&& fileName:files)
    {
        QFile file(fileName);
        if (file.open(QFile::ReadOnly))
        {
            auto data=file.readAll();
            if (!data.isEmpty())
            {
                m_loadedStyleSheet+=QString("%1\n").arg(QString::fromUtf8(data));
            }
        }
    }

    setStyleSheet(m_loadedStyleSheet);
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
    resetSvgIconTheme();
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
    m_styleSheetPaths.push_back(UiseQssPath);    
}

//--------------------------------------------------------------------------
void Style::resetSvgIconTheme()
{
    m_svgIconTheme.addIconDir(":/uise/tabler-icons/outline");

    HTree::resetSvgIconTheme(*this);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
