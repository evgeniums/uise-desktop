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

/** @file uise/desktop/syntaxtheme.hpp
*
*  Declares SyntaxTheme.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_SYNTAX_THEME_HPP
#define UISE_DESKTOP_SYNTAX_THEME_HPP

#include <map>
#include <optional>

#include <QString>
#include <QColor>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Colour palette for code-block syntax highlighting, loaded from a "kind":"syntax" JSON
 *  document (see Style::reloadStyleSheet()'s dispatch on that discriminator).
 *
 * Deliberately independent of WithModesMap/IconMode: a syntax highlighter has no
 * hover/checked/disabled axis for a bucket colour to vary over, so that machinery buys nothing
 * here. Buckets are looked up by a plain string name rather than a fixed enum specifically so a
 * new bucket (or a future per-language override) never needs a schema change -- see
 * task-message-formatting-plan.md, Stage 1.
 *
 * The brief's sixth semantic bucket, "Primary Text", is deliberately not represented here at all:
 * its own guidance is to leave it as the unmodified fallback text colour, so its absence from a
 * theme already encodes the intended behaviour.
 */
class UISE_DESKTOP_EXPORT SyntaxTheme
{
    public:

        SyntaxTheme()
        {}

        bool loadFromJson(const QString& json, QString* errorMessage=nullptr);

        QString name() const
        {
            return m_name;
        }

        std::optional<QColor> color(const QString& bucket) const
        {
            auto it=m_colors.find(bucket);
            if (it!=m_colors.end())
            {
                return it->second;
            }
            return std::optional<QColor>{};
        }

        const std::map<QString,QColor>& colors() const
        {
            return m_colors;
        }

    private:

        QString m_name;
        std::map<QString,QColor> m_colors;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_SYNTAX_THEME_HPP
