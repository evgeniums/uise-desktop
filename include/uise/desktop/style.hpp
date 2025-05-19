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
#include <uise/desktop/svgicontheme.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Helper class to work with Qt style sheets and icons.
 */
class UISE_DESKTOP_EXPORT Style final
{
    public:

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
         * See also setStyleSheetPaths().
         */
        QStringList styleSheetPaths() const
        {
            return m_styleSheetPaths;
        }

        /**
         * @brief Set style sheet path.
         * @param paths Paths to folders with files containing style sheets.
         *
         * Path of the folder for dark theme is automatically constructed by adding "/dark" to the style sheet path.
         * Thus, the style sheet path points to folder with style sheet files for light theme and subfolder "/dark" with style sheet files for dark theme.
         * Style sheet files must have *.qss or *.css extensions.
         */
        void setStyleSheetPaths(QStringList paths)
        {
            m_styleSheetPaths=std::move(paths);
        }

        /**
         * @brief Append path to list of style sheet paths.
         * @param path New path.
         *
         * See also setStyleSheetPaths().
         */
        void appendStyleSheetPath(QString path)
        {
            m_styleSheetPaths.push_back(std::move(path));
        }

        /**
         * @brief Reset paths of style sheets.
         *
         * See also setStyleSheetPaths().
         */
        void resetStyleSheetPaths();

        /**
         * @brief Get actual style sheet.
         * @return Query result.
         *
         * Actual style sheet must be either set with setStyleSheet() or automatically constructed with reloadStyleSheet() in advance.
         */
        QString styleSheet() const
        {
            return m_styleSheet;
        }

        /**
         * @brief Explicitly set actual style sheet.
         * @param styleSheet New style sheet.
         *
         * Style sheet will be applied to widgets or application only after calling applyStyleSheet().
         */
        void setStyleSheet(const QString& styleSheet)
        {
            m_styleSheet=QString("%1\n%2").arg(m_baseStyleSheet,styleSheet);
        }

        /**
         * @brief Get base style sheet.
         * @return Query result.
         *
         * See also setBaseStyleSheet().
         */
        QString baseStyleSheet() const
        {
            return m_baseStyleSheet;
        }

        /**
         * @brief Set base style sheet.
         * @param baseStyleSheet New base style sheet.
         *
         * Base style sheet is an immutable part of automatically constructed actual style sheet.
         * Base style sheet is prepended to the automatically constructed style sheet in reloadStyleSheet().
         */
        void setBaseStyleSheet(QString baseStyleSheet)
        {
            m_baseStyleSheet=std::move(baseStyleSheet);
        }

        /**
         * @brief Get loaded style sheet.
         * @return Loaded style sheet.
         *
         * Loaded style sheet is constructed automatically in reloadStyleSheet() by joining contents style sheet files read from folder at styleSheetPath().
         */
        QString loadedStyleSheet() const
        {
            return m_loadedStyleSheet;
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
         * @brief Apply style sheet to widget or application.
         * @param widget Widget to apply style sheet to, if nullptr then the style will be applied to the entire application.
         */
        void applyStyleSheet(QWidget* widget=nullptr);

        /**
         * @brief Create icon.
         * @param name Name of the icon.
         * @param ext Extension to use for construction of filename of fallback icon.
         * @return Requested icon if file for the icon is found.
         *
         * Search for the icon is performed in the following order:
         * <pre>
         * 1. Look up at QIcon::themeName() of QIcon::themeSearchPaths().
         * 2. Look up at QIcon::fallbackThemeName() of QIcon::fallbackThemePaths().
         * 3. Look up at fallbackIconPaths() or fallbackIconPaths()/dark one by one depending on the dark/light state of current theme.
         * </pre>
         */
        QIcon icon(const QString& name, const QString& ext="svg") const;

        /**
         * @brief Get paths of fallback icons.
         * @return Query result.
         *
         * See also icon().
         */
        QStringList fallbacktIconPaths() const
        {
            return m_fallbackIconPaths;
        }

        /**
         * @brief Set paths of fallback icons.
         * @param path New list of paths.
         *
         * See also icon().
         */
        void setFallbackIconPaths(QStringList paths)
        {
            m_fallbackIconPaths=std::move(paths);
        }

        /**
         * @brief Prepend path to list of paths of fallback icons.
         * @param path New default path.
         *
         * See also icon().
         */
        void prependFallbackIconPath(QString path)
        {
            m_fallbackIconPaths.push_front(std::move(path));
        }

        /**
         * @brief Reset paths of fallback icons.
         * @param path New list of paths.
         *
         * See also icon().
         */
        void resetFallbackIconPaths();

        SvgIconTheme& svgIconTheme()
        {
            return m_svgIconTheme;
        }

    private:

        QString m_styleSheet;
        QString m_baseStyleSheet;
        QString m_loadedStyleSheet;
        QStringList m_styleSheetPaths;
        QStringList m_fallbackIconPaths;

        bool m_darkTheme;
        StyleSheetMode m_darkStyleSheetMode;

        std::map<QString,QString> m_colorMap;

        SvgIconTheme m_svgIconTheme;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_STYLE_HPP
