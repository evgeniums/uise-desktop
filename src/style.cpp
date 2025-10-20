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
#include <uise/desktop/svgiconcontext.hpp>
#include <uise/desktop/defaultwidgetfactory.hpp>
#include <uise/desktop/style.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

QStringList filters()
{
    return QStringList{"*.qss","*.css","*.json"};
}

}

//--------------------------------------------------------------------------
Style::Style(
    ) : m_darkTheme(false),
        m_darkStyleSheetMode(StyleSheetMode::Auto)
{
    resetStyleSheetDirs();
    resetSvgIconLocator();
    m_widgetFactory=defaultWidgetFactory();
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
    m_loadedQss.clear();
    m_loadedCss.clear();
    m_iconThemes.clear();

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
    for (auto&& folderPath:m_styleSheetDirs)
    {
        QDir stylesDir(folderPath);
        stylesDir.setNameFilters(filters());
        auto items=stylesDir.entryInfoList(QDir::Files);
        for (auto&& item:items)
        {
            files.append(item.canonicalFilePath());
        }
    }

    // setup color theme name
    QString defaultColorTheme;
    auto colorTheme=m_colorThemeName;
    if (darkTheme)
    {
        defaultColorTheme=DarkTheme;
    }
    else
    {
        defaultColorTheme=LightTheme;
    }
    if (colorTheme.isEmpty())
    {
        colorTheme=defaultColorTheme;
    }

    // list color style files    
    for (auto&& folderPath:m_styleSheetDirs)
    {        
        auto defaultColorThemePath=QString("%1/%2").arg(folderPath,defaultColorTheme);
        auto themePath=QString("%1/%2").arg(folderPath,colorTheme);

        // list deafult color theme files for dark or light
        QSet<QString> defaultFiles;
        QDir defaultStylesDir(defaultColorThemePath);
        defaultStylesDir.setNameFilters(filters());
        auto items=defaultStylesDir.entryInfoList(QDir::Files);
        for (auto&& item:items)
        {
            defaultFiles.insert(item.fileName());
        }

        // list current theme files
        if (colorTheme!=defaultColorTheme)
        {
            QDir themeStylesDir(themePath);
            themeStylesDir.setNameFilters(filters());
            items=themeStylesDir.entryInfoList(QDir::Files);
            for (auto&& item:items)
            {
                auto file=item.canonicalFilePath();
                if (defaultFiles.contains(item.fileName()))
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
        QFileInfo finf{fileName};

        QFile file(fileName);
        if (file.open(QFile::ReadOnly))
        {
            auto data=file.readAll();
            if (!data.isEmpty())
            {
                QString src=QString::fromUtf8(data);
                if (finf.suffix()=="qss")
                {
                    m_loadedQss+=QString("%1\n").arg(src);
                }
                else if (finf.suffix()=="css")
                {
                    m_loadedCss+=QString("%1\n").arg(src);
                }
                else if (finf.suffix()=="json")
                {
                    SvgIconTheme iconTheme;
                    QString errorMessage;
                    auto ok=iconTheme.loadFromJson(src,&errorMessage);
                    if (ok)
                    {
                        auto name=iconTheme.name();
                        if (name==defaultColorTheme || name==colorTheme || name==AnyColorTheme)
                        {
                            auto& inserted=m_iconThemes.emplace_back(std::move(iconTheme));
                            inserted.setModesMap(modeMap());
                        }
                        else
                        {
                            qWarning() << "Invalid SVG icon theme \"" << name << "\" in " << fileName;
                        }
                    }
                    else
                    {
                        qWarning() << "Failed to load SVG icon theme from " << fileName << ": " << errorMessage;
                    }
                }
            }
        }
    }

    //! @todo Apply color substitutions
    setQss(m_loadedQss);
    setCss(m_loadedCss);
}

//--------------------------------------------------------------------------
void Style::applyQss(QWidget *widget)
{
    if (widget==nullptr)
    {
        qApp->setStyleSheet(m_qss);
    }
    else
    {
        widget->setStyleSheet(m_qss);
        checkDarkTheme();
    }    
}

//--------------------------------------------------------------------------
void Style::applySvgIconTheme()
{
    for (const auto& iconTheme: m_iconThemes)
    {
        m_svgIconLocator.loadIconTheme(iconTheme);
    }
}

//--------------------------------------------------------------------------
void Style::reloadSvgIconTheme()
{
    m_svgIconLocator.reloadIconThemes(m_iconThemes);
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
    m_qss.clear();
    m_baseQss.clear();
    m_loadedQss.clear();

    m_css.clear();
    m_baseCss.clear();
    m_loadedCss.clear();

    m_darkTheme=false;
    m_darkStyleSheetMode=StyleSheetMode::Auto;

    m_colorMap.clear();
    m_iconThemes.clear();

    resetStyleSheetDirs();
    resetSvgIconLocator();
}

//--------------------------------------------------------------------------
void Style::resetStyleSheetDirs()
{
    m_styleSheetDirs.clear();
    m_styleSheetDirs.push_back(UiseStylePath);
}

//--------------------------------------------------------------------------
void Style::resetSvgIconLocator()
{
    m_svgIconLocator.reset();
}

//--------------------------------------------------------------------------

void Style::mergeWidgetFactory(std::shared_ptr<WidgetFactory> factory)
{
    if (!m_widgetFactory)
    {
        m_widgetFactory=std::move(factory);
    }
    else
    {
        m_widgetFactory->merge(*factory);
    }
}

//--------------------------------------------------------------------------

void Style::updateWidgetStyle(QWidget* source, QWidget* target)
{
    if (source==nullptr)
    {
        return;
    }
    if (target==nullptr)
    {
        target=source;
    }
    auto style=source->style();
    if (style!=nullptr)
    {
        style->unpolish(target);
        style->polish(target);
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
