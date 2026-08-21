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
#include <QStyleHints>
#include <QRegularExpression>
#include <QDebug>

#include <iostream>

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

// Diagnostic counter for Style::updateWidgetStyle() call volume -- unpolish()+polish() re-matches
// the whole QSS rule set for the widget, so a hot path calling it unconditionally (e.g. per
// message per batch recompute in the chat view) is expensive. Off by default; enable with
// UISE_STYLE_DEBUG=1 to see the running count and which widgets are being repolished.
bool styleDebugEnabled()
{
    static bool enabled=qEnvironmentVariableIsSet("UISE_STYLE_DEBUG");
    return enabled;
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    m_darkTheme=QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    m_darkTheme=QApplication::palette().color(QPalette::Window).lightness() < 128;
#endif
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

        // List default color theme files for dark or light.
        //
        // Kept as an ordered list, not just a QSet: the concatenation order of these files IS
        // the cascade order of the rules in them, so it decides which of two equally specific
        // rules wins. QSet iterates in hash order, and QHash seeds its hash randomly per
        // process, so appending straight out of the set made that order -- and therefore the
        // resulting style -- differ from one run of the application to the next. The set is
        // kept alongside purely as the O(1) "was this overridden" lookup below.
        //
        // entryInfoList() sorts by name (QDir's default sorting), which is what makes the
        // ordered list deterministic.
        QStringList defaultFileNames;
        QSet<QString> defaultFiles;
        QDir defaultStylesDir(defaultColorThemePath);
        defaultStylesDir.setNameFilters(filters());
        auto items=defaultStylesDir.entryInfoList(QDir::Files);
        for (auto&& item:items)
        {
            defaultFileNames.append(item.fileName());
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
        for (auto&& fileName:defaultFileNames)
        {
            if (defaultFiles.contains(fileName))
            {
                files.append(defaultColorThemePath+"/"+fileName);
            }
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
    if (styleDebugEnabled())
    {
        static size_t count=0;
        ++count;
        std::cerr << "UISE-STYLE-DEBUG repolish #" << count
                   << " " << target->metaObject()->className()
                   << " #" << target->objectName().toStdString()
                   << std::endl;
    }
    auto style=source->style();
    if (style!=nullptr)
    {
        style->unpolish(target);
        style->polish(target);
    }
}

//--------------------------------------------------------------------------

bool Style::setStyleProperty(QWidget* widget, const char* name, const QVariant& value, QWidget* repolishTarget)
{
    if (widget==nullptr)
    {
        return false;
    }
    auto current=widget->property(name);
    if (current.isValid() && current==value)
    {
        return false;
    }
    widget->setProperty(name,value);
    updateWidgetStyle(widget,repolishTarget);
    return true;
}

//--------------------------------------------------------------------------

void Style::repolishRecursive(QWidget* widget)
{
    if (widget==nullptr)
    {
        return;
    }
    updateWidgetStyle(widget);
    const auto children=widget->findChildren<QWidget*>();
    for (QWidget* c : children)
    {
        updateWidgetStyle(c);
    }
}

//--------------------------------------------------------------------------

void Style::enableSystemColorSchemeTracking()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (m_systemColorSchemeTracking)
    {
        return;
    }
    m_systemColorSchemeTracking=true;
    QObject::connect(
        QGuiApplication::styleHints(),
        &QStyleHints::colorSchemeChanged,
        qApp,
        [](Qt::ColorScheme)
        {
            auto& style=Style::instance();
            if (style.styleSheetMode()==StyleSheetMode::Auto)
            {
                style.applyStyleSheet(true);
            }
        });
#endif
}

//--------------------------------------------------------------------------

bool isDefaultStyleToken(const QString& str)
{
    auto s=str.trimmed();
    return s.isEmpty()
           || s.compare(QLatin1String("default"),Qt::CaseInsensitive)==0
           || s.compare(QLatin1String("inherit"),Qt::CaseInsensitive)==0;
}

//--------------------------------------------------------------------------

Qt::Alignment alignmentFromString(const QString& str, bool* ok)
{
    if (ok!=nullptr)
    {
        *ok=true;
    }

    Qt::Alignment alignment;

    static const QRegularExpression separator(QStringLiteral("[\\s,|;]+"));
    const auto tokens=str.split(separator,Qt::SkipEmptyParts);
    for (auto token: tokens)
    {
        token=token.toLower();
        if (token.startsWith(QLatin1String("align")))
        {
            token=token.mid(5);
        }

        if (token==QLatin1String("left"))
        {
            alignment|=Qt::AlignLeft;
        }
        else if (token==QLatin1String("right"))
        {
            alignment|=Qt::AlignRight;
        }
        else if (token==QLatin1String("hcenter"))
        {
            alignment|=Qt::AlignHCenter;
        }
        else if (token==QLatin1String("justify"))
        {
            alignment|=Qt::AlignJustify;
        }
        else if (token==QLatin1String("top"))
        {
            alignment|=Qt::AlignTop;
        }
        else if (token==QLatin1String("bottom"))
        {
            alignment|=Qt::AlignBottom;
        }
        else if (token==QLatin1String("vcenter"))
        {
            alignment|=Qt::AlignVCenter;
        }
        else if (token==QLatin1String("baseline"))
        {
            alignment|=Qt::AlignBaseline;
        }
        else if (token==QLatin1String("center"))
        {
            alignment|=Qt::AlignCenter;
        }
        else if (token==QLatin1String("stretch") || token==QLatin1String("none"))
        {
            // explicit "no flags" -- nothing to OR in
        }
        else
        {
            qWarning() << "uise: unknown alignment token" << token;
            if (ok!=nullptr)
            {
                *ok=false;
            }
        }
    }

    return alignment;
}

//--------------------------------------------------------------------------

QString alignmentToString(Qt::Alignment alignment)
{
    QStringList parts;

    const auto h=alignment & Qt::AlignHorizontal_Mask;
    const auto v=alignment & Qt::AlignVertical_Mask;

    if (h==Qt::AlignHCenter && v==Qt::AlignVCenter)
    {
        return QStringLiteral("center");
    }

    if (h==Qt::AlignLeft)
    {
        parts << QStringLiteral("left");
    }
    else if (h==Qt::AlignRight)
    {
        parts << QStringLiteral("right");
    }
    else if (h==Qt::AlignHCenter)
    {
        parts << QStringLiteral("hcenter");
    }
    else if (h==Qt::AlignJustify)
    {
        parts << QStringLiteral("justify");
    }

    if (v==Qt::AlignTop)
    {
        parts << QStringLiteral("top");
    }
    else if (v==Qt::AlignBottom)
    {
        parts << QStringLiteral("bottom");
    }
    else if (v==Qt::AlignVCenter)
    {
        parts << QStringLiteral("vcenter");
    }
    else if (v==Qt::AlignBaseline)
    {
        parts << QStringLiteral("baseline");
    }

    if (parts.isEmpty())
    {
        return QStringLiteral("stretch");
    }

    return parts.join(QLatin1Char(' '));
}

//--------------------------------------------------------------------------

Qt::Orientation orientationFromString(const QString& str, bool* ok)
{
    auto s=str.trimmed().toLower();
    if (ok!=nullptr)
    {
        *ok=true;
    }

    if (s==QLatin1String("vertical") || s==QLatin1String("v"))
    {
        return Qt::Vertical;
    }
    if (s==QLatin1String("horizontal") || s==QLatin1String("h"))
    {
        return Qt::Horizontal;
    }

    qWarning() << "uise: unknown orientation" << str;
    if (ok!=nullptr)
    {
        *ok=false;
    }
    return Qt::Horizontal;
}

//--------------------------------------------------------------------------

QString orientationToString(Qt::Orientation orientation)
{
    return orientation==Qt::Vertical ? QStringLiteral("vertical") : QStringLiteral("horizontal");
}

//--------------------------------------------------------------------------

Qt::CursorShape cursorShapeFromString(const QString& str, bool* ok)
{
    if (ok!=nullptr)
    {
        *ok=true;
    }

    auto s=str.trimmed().toLower();
    if (s.startsWith(QLatin1String("qt::")))
    {
        s=s.mid(4);
    }
    if (s.endsWith(QLatin1String("cursor")))
    {
        s.chop(6);
    }

    if (s==QLatin1String("arrow"))
    {
        return Qt::ArrowCursor;
    }
    if (s==QLatin1String("pointer") || s==QLatin1String("pointinghand") || s==QLatin1String("pointing-hand"))
    {
        return Qt::PointingHandCursor;
    }
    if (s==QLatin1String("hand") || s==QLatin1String("openhand"))
    {
        return Qt::OpenHandCursor;
    }
    if (s==QLatin1String("closedhand"))
    {
        return Qt::ClosedHandCursor;
    }
    if (s==QLatin1String("ibeam") || s==QLatin1String("text"))
    {
        return Qt::IBeamCursor;
    }
    if (s==QLatin1String("wait") || s==QLatin1String("busy"))
    {
        return Qt::WaitCursor;
    }
    if (s==QLatin1String("cross") || s==QLatin1String("crosshair"))
    {
        return Qt::CrossCursor;
    }
    if (s==QLatin1String("whatsthis"))
    {
        return Qt::WhatsThisCursor;
    }
    if (s==QLatin1String("forbidden") || s==QLatin1String("not-allowed"))
    {
        return Qt::ForbiddenCursor;
    }
    if (s==QLatin1String("sizeall"))
    {
        return Qt::SizeAllCursor;
    }
    if (s==QLatin1String("sizehor"))
    {
        return Qt::SizeHorCursor;
    }
    if (s==QLatin1String("sizever"))
    {
        return Qt::SizeVerCursor;
    }
    if (s==QLatin1String("sizefdiag"))
    {
        return Qt::SizeFDiagCursor;
    }
    if (s==QLatin1String("sizebdiag"))
    {
        return Qt::SizeBDiagCursor;
    }
    if (s==QLatin1String("splith"))
    {
        return Qt::SplitHCursor;
    }
    if (s==QLatin1String("splitv"))
    {
        return Qt::SplitVCursor;
    }
    if (s==QLatin1String("blank") || s==QLatin1String("none"))
    {
        return Qt::BlankCursor;
    }

    qWarning() << "uise: unknown cursor shape" << str;
    if (ok!=nullptr)
    {
        *ok=false;
    }
    return Qt::ArrowCursor;
}

//--------------------------------------------------------------------------

QString cursorShapeToString(Qt::CursorShape shape)
{
    switch (shape)
    {
        case (Qt::ArrowCursor): return QStringLiteral("arrow");
        case (Qt::PointingHandCursor): return QStringLiteral("pointer");
        case (Qt::OpenHandCursor): return QStringLiteral("hand");
        case (Qt::ClosedHandCursor): return QStringLiteral("closedhand");
        case (Qt::IBeamCursor): return QStringLiteral("ibeam");
        case (Qt::WaitCursor): return QStringLiteral("wait");
        case (Qt::CrossCursor): return QStringLiteral("cross");
        case (Qt::WhatsThisCursor): return QStringLiteral("whatsthis");
        case (Qt::ForbiddenCursor): return QStringLiteral("forbidden");
        case (Qt::SizeAllCursor): return QStringLiteral("sizeall");
        case (Qt::SizeHorCursor): return QStringLiteral("sizehor");
        case (Qt::SizeVerCursor): return QStringLiteral("sizever");
        case (Qt::SizeFDiagCursor): return QStringLiteral("sizefdiag");
        case (Qt::SizeBDiagCursor): return QStringLiteral("sizebdiag");
        case (Qt::SplitHCursor): return QStringLiteral("splith");
        case (Qt::SplitVCursor): return QStringLiteral("splitv");
        case (Qt::BlankCursor): return QStringLiteral("blank");
        default: break;
    }
    return QStringLiteral("arrow");
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
