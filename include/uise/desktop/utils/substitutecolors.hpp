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

/** @file uise/desktop/utils/substitutecolor.hpp
*
*  Defines substituteColors() method.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_SUBSTITUTECOLORS_HPP
#define UISE_DESKTOP_SUBSTITUTECOLORS_HPP

#include <string>
#include <map>
#include <sstream>

#include <uise/desktop/uisedesktop.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

/**
 * @brief Substitite colors in CSS string.
 * @param in Input string.
 * @param subst Color map.
 * @return Result CSS string.
 */
inline std::string substituteColors(
        const std::string& in,
        const std::map<std::string, std::string>& subst
    )
{
    std::ostringstream out;
    size_t pos = 0;
    for (;;)
    {
        auto substPos = in.find("#", pos);
        if (substPos == std::string::npos) break;
        auto endPos = substPos+9;
        if (endPos >= in.size()) break;

        out.write(&* in.begin() + pos, substPos - pos);

        auto color = in.substr(substPos, endPos - substPos);
        auto it = subst.find(color);
        if (it == subst.end())
        {
            out << color;
        }
        else
        {
            out << it->second;
        }
        pos = endPos;
    }
    out << in.substr(pos,std::string::npos);
    return out.str();
}

}

#endif // UISE_DESKTOP_SUBSTITUTECOLORS_HPP
