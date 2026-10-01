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

/** @file uise/desktop/colorpalette.hpp
*
*  Declares ColorPaletteTheme and the palette index helpers.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_COLOR_PALETTE_HPP
#define UISE_DESKTOP_COLOR_PALETTE_HPP

#include <map>
#include <vector>
#include <cstddef>

#include <QString>
#include <QColor>
#include <QByteArray>
#include <QByteArrayView>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Names of the palettes the library itself consumes.
 *
 * A palette is a named, ordered list of colours defined in a "kind":"palette" style JSON
 * document and looked up through Style::colorPalette() / Style::paletteColor(). Names are plain
 * strings so an application can define and use palettes of its own; these constants only keep the
 * built-in consumers (and applications that must agree with them, e.g. a notification popup that
 * has to colour a sender like the chat bubble does) from drifting apart.
 */
namespace ColorPaletteNames
{
    //! Text colours for a message sender's title (group chat bubble header, popup notification).
    constexpr const char* ChatSenderTitle="chat-sender-title";

    //! Background colours of generated (initials) avatars.
    constexpr const char* AvatarBackground="avatar-background";
}

/**
 * @brief Map a SHA-1 digest to a palette slot.
 * @param sha1Digest A 20-byte SHA-1 result.
 * @param count Number of palette entries.
 * @return A stable index in [0,count), or 0 if the digest is too short or count is 0.
 *
 * Takes the first sizeof(size_t) bytes in native byte order, modulo count. This is exactly what
 * the avatar code always did, so a palette of unchanged length keeps every avatar's colour.
 */
UISE_DESKTOP_EXPORT size_t paletteIndex(const QByteArray& sha1Digest, size_t count);

/**
 * @brief Map an arbitrary key to a palette slot via SHA-1, see paletteIndex().
 *
 * Deliberately not qHash(): that is seeded per process, so a sender's colour would change on
 * every start.
 */
UISE_DESKTOP_EXPORT size_t paletteIndexForKey(QByteArrayView key, size_t count);

/**
 * @brief Named colour palettes loaded from a "kind":"palette" JSON document (see
 *  Style::reloadStyleSheet()'s dispatch on that discriminator).
 *
 * Schema:
 * @code
 * {
 *   "kind": "palette",
 *   "theme": "light",                 // optional, like every other style JSON; absent == any theme
 *   "palettes": {
 *     "chat-sender-title": ["#C03D33", "#B15C13"],
 *     "avatar-background": []         // an empty list is valid and means "no palette"
 *   }
 * }
 * @endcode
 *
 * Invalid colour strings are skipped with a warning rather than failing the whole document, so a
 * typo costs one palette slot instead of the whole theme.
 */
class UISE_DESKTOP_EXPORT ColorPaletteTheme
{
    public:

        ColorPaletteTheme()
        {}

        bool loadFromJson(const QString& json, QString* errorMessage=nullptr);

        QString name() const
        {
            return m_name;
        }

        //! Whether this document defines the palette at all (an explicitly empty list counts).
        bool hasPalette(const QString& name) const
        {
            return m_palettes.find(name)!=m_palettes.end();
        }

        const std::vector<QColor>& palette(const QString& name) const
        {
            static const std::vector<QColor> empty;
            auto it=m_palettes.find(name);
            if (it!=m_palettes.end())
            {
                return it->second;
            }
            return empty;
        }

        const std::map<QString,std::vector<QColor>>& palettes() const
        {
            return m_palettes;
        }

    private:

        QString m_name;
        std::map<QString,std::vector<QColor>> m_palettes;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_COLOR_PALETTE_HPP
