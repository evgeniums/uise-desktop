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

/** @file uise/desktop/style.hpp
*
*  Declares Style.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_STYLE_HPP
#define UISE_DESKTOP_STYLE_HPP

#include <map>
#include <QString>
#include <QIcon>
#include <QStyle>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/svgiconlocator.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

struct ButtonsStyle
{
    Qt::Alignment alignment=Qt::AlignRight;
    bool showText=true;
    bool showIcon=false;
};

class WidgetFactory;

/**
 * @brief Helper class to work with Qt style sheets and icons.
 */
class UISE_DESKTOP_EXPORT Style : public WithModesMap
{
    public:

        constexpr static const char* UiseStylePath=":/uise/desktop/style";
        constexpr static const char* AnyColorTheme="any";

        constexpr static const char* LightTheme="light";
        constexpr static const char* DarkTheme="dark";

        /**
         * @brief Modes of style sheet theme.
         */
        enum class StyleSheetMode : int
        {
            Auto, //!< Detect theme automatically depending on default application style.
            Light, //!< Light theme.
            Dark //!< Dark theme.
        };

        /**
         * @brief Constructor.
         */
        Style();

        /**
         * @brief Get style singleton object.
         * @return Style singleton object.
         */
        static Style& instance();

        /**
         * @brief Reset style to initial state.
         *
         * QIcon theme settings are not reset.
         */
        void reset();

        /**
         * @brief Query if current style is in dark theme.
         * @return Query result.
         */
        bool isDarkTheme() const noexcept
        {
            if (m_darkStyleSheetMode==StyleSheetMode::Auto)
            {
                return const_cast<Style*>(this)->checkDarkTheme();
            }

            return m_darkTheme;
        }

        /**
         * @brief Check if application style is in dark theme.
         * @return Query result.
         *
         * Dark theme is automatically detected depending on QLabel pallete found in Qapplication.
         */
        bool checkDarkTheme();

        /**
         * @brief Get style sheet mode.
         * @return Query result.
         */
        StyleSheetMode styleSheetMode() const noexcept
        {
            return m_darkStyleSheetMode;
        }

        /**
         * @brief Set style sheet mode.
         * @param val New mode.
         *
         * New mode will be applied to the style only after call to reloadStyleSheet().
         * New style can be applied to widgets or application by calling applyStyleSheet().
         */
        void setStyleSheetMode(StyleSheetMode val) noexcept
        {
            m_darkStyleSheetMode=val;
        }

        /**
         * @brief Get style sheet paths.
         * @return Query result.
         *
         * See also setStyleSheetDirs().
         */
        QStringList styleSheetDirs() const
        {
            return m_styleSheetDirs;
        }

        /**
         * @brief Set style sheet path.
         * @param dirs Directories to folders with files containing style sheets.
         *
         * Path of the folder for dark theme is automatically constructed by adding "/dark" to the style sheet path.
         * Thus, the style sheet path points to folder with style sheet files for light theme and subfolder "/dark" with style sheet files for dark theme.
         * Style sheet files must have *.qss or *.css extensions.
         */
        void setStyleSheetDirs(QStringList dirs)
        {
            m_styleSheetDirs=std::move(dirs);
        }

        /**
         * @brief Append path to list of style sheet paths.
         * @param dir New directory.
         *
         * See also setStyleSheetDirs().
         */
        void appendStyleSheetDir(QString dir)
        {
            m_styleSheetDirs.push_back(std::move(dir));
        }

        /**
         * @brief Reset paths of style sheets.
         *
         * See also setStyleSheetDirs().
         */
        void resetStyleSheetDirs();

        /**
         * @brief Get actual Qt style sheet.
         * @return Query result.
         *
         * Actual style sheet must be either set with setStyleSheet() or automatically constructed with reloadStyleSheet() in advance.
         */
        QString qss() const
        {
            return m_qss;
        }

        /**
         * @brief Explicitly set actual Qt style sheet.
         * @param styleSheet New style sheet.
         *
         * Style sheet will be applied to widgets or application only after calling applyStyleSheet().
         */
        void setQss(const QString& qss)
        {
            m_qss=QString("%1\n%2").arg(m_baseQss,qss);
        }

        /**
         * @brief Get base Qt style sheet.
         * @return Query result.
         *
         * See also setBaseQss().
         */
        QString baseQss() const
        {
            return m_baseQss;
        }

        /**
         * @brief Set base Qt style sheet.
         * @param baseQss New base style sheet.
         *
         * Base style sheet is an immutable part of automatically constructed actual style sheet.
         * Base style sheet is prepended to the automatically constructed style sheet in reloadStyleSheet().
         */
        void setBaseQss(QString baseQss)
        {
            m_baseQss=std::move(baseQss);
        }

        /**
         * @brief Get loaded Qt style sheet.
         * @return Loaded style sheet.
         *
         * Loaded style sheet is constructed automatically in reloadStyleSheet() by joining contents style sheet files read from folder at styleSheetPath().
         */
        QString loadedQss() const
        {
            return m_loadedQss;
        }

        /**
         * @brief Get actual web style sheet.
         * @return Query result.
         *
         * Actual style sheet must be either set with setStyleSheet() or automatically constructed with reloadStyleSheet() in advance.
         */
        QString css() const
        {
            return m_css;
        }

        /**
         * @brief Explicitly set actual wb style sheet.
         * @param styleSheet New style sheet.
         *
         * Style sheet will be applied to widgets or application only after calling applyStyleSheet().
         */
        void setCss(const QString& css)
        {
            m_css=QString("%1\n%2").arg(m_baseCss,css);
        }

        /**
         * @brief Get base Qt style sheet.
         * @return Query result.
         *
         * See also setBaseQss().
         */
        QString baseCss() const
        {
            return m_baseCss;
        }

        /**
         * @brief Set base Qt style sheet.
         * @param baseQss New base style sheet.
         *
         * Base style sheet is an immutable part of automatically constructed actual style sheet.
         * Base style sheet is prepended to the automatically constructed style sheet in reloadStyleSheet().
         */
        void setBaseCss(QString baseCss)
        {
            m_baseCss=std::move(baseCss);
        }

        /**
         * @brief Get loaded web style sheet.
         * @return Loaded style sheet.
         *
         * Loaded style sheet is constructed automatically in reloadStyleSheet() by joining contents style sheet files read from folder at styleSheetPath().
         */
        QString loadedCss() const
        {
            return m_loadedCss;
        }

        QString colorTheme() const
        {
            return m_colorThemeName;
        }

        void setColorTheme(QString value)
        {
            m_colorThemeName=std::move(value);
        }

        /**
         * @brief Get color map.
         * @return Query result.
         *
         * See also setColorMap().
         */
        std::map<QString,QString> colorMap() const
        {
            return m_colorMap;
        }

        /**
         * @brief Set color map.
         * @param colorMap New color map.
         *
         * Color map is used to substitute colors in style sheets read into loadedStyleSheet().
         * New color map will be applied to the style only after call to reloadStyleSheet().
         * New style can be applied to widgets or application by calling applyStyleSheet().
         */
        void setColorMap(std::map<QString,QString> colorMap)
        {
            m_colorMap=std::move(colorMap);
        }

        /**
         * @brief Set substitution for a color in style sheets.
         * @param keyColor Initial color to substitute.
         * @param targetColor New color.
         *
         * See also setColorMap().
         */
        void setColorSubstitution(QString keyColor, QString targetColor)
        {
            m_colorMap.emplace(std::move(keyColor),std::move(targetColor));
        }

        /**
         * @brief Load color map from JSON file.
         * @param fileName Name of JSON file.
         * @param errMsg Error description in case of error.
         * @return Operation status.
         *
         * JSON document must contain "colors" object containing key-value pairs for colors and their substitutions.
         * Note that only hex color values starting with # are supported.
         * For example:
         * <pre>
         * {
         *      "colors":
         *          {
         *              "#00000000": "#FFFFFFFF",
         *              "#FFFFFFFF": "#00000000",
         *              "#aabbccdd": "#ddccbbaa"
         *          }
         * }
         * </pre>
         *
         * See alco setColorMap().
         */
        bool loadColorMap(const QString& fileName, QString* errMsg=nullptr);

        /**
         * @brief Reload style sheet with actual settings.
         *
         * Constructs new actual styleSheet() depending on the styleSheetPath(), styleSheetMode() and colorMap().
         * New style can be applied to widgets or application by calling applyStyleSheet().
         */
        void reloadStyleSheet();

        /**
         * @brief Apply Qt style sheet to widget or application.
         * @param widget Widget to apply style sheet to, if nullptr then the style will be applied to the entire application.
         */
        void applyQss(QWidget* widget=nullptr);

        void applySvgIconTheme();

        void reloadSvgIconTheme();

        SvgIconLocator& svgIconLocator()
        {
            return m_svgIconLocator;
        }

        void resetSvgIconLocator();

        void applyStyleSheet(bool reload=false)
        {
            reloadStyleSheet();
            applyQss();
            if (reload)
            {
                reloadSvgIconTheme();
            }
            else
            {
                applySvgIconTheme();
            }
        }

        const ButtonsStyle& buttonsStyle(const QString& contextName, const StyleContext& ={}) const
        {
            //! @todo Implement configurable style depending on context

            auto it=m_buttonsStyle.find(contextName);
            if (it==m_buttonsStyle.end())
            {
                return m_defaultButtonsStyle;
            }
            return it->second;
        }

        void setDefaultButtonsStyle(ButtonsStyle val, const QString& contextName={})
        {
            if (contextName.isEmpty())
            {
                m_defaultButtonsStyle=val;
            }
            else
            {
                m_buttonsStyle[contextName]=val;
            }
        }

        const WidgetFactory* widgetFactory() const noexcept
        {
            return m_widgetFactory.get();
        }

        void setWidgetFactory(std::shared_ptr<WidgetFactory> factory)
        {
            m_widgetFactory=std::move(factory);
        }

        void mergeWidgetFactory(std::shared_ptr<WidgetFactory> factory);

        static void updateWidgetStyle(QWidget* source, QWidget* target=nullptr);

    private:

        QString m_qss;
        QString m_baseQss;
        QString m_loadedQss;

        QString m_css;
        QString m_baseCss;
        QString m_loadedCss;

        QStringList m_styleSheetDirs;
        QStringList m_fallbackIconPaths;

        bool m_darkTheme;
        StyleSheetMode m_darkStyleSheetMode;

        std::map<QString,QString> m_colorMap;

        SvgIconLocator m_svgIconLocator;
        QString m_colorThemeName;

        std::vector<SvgIconTheme> m_iconThemes;

        ButtonsStyle m_defaultButtonsStyle;
        std::map<QString,ButtonsStyle> m_buttonsStyle;

        std::shared_ptr<WidgetFactory> m_widgetFactory;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_STYLE_HPP
