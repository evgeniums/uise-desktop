/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

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

UISE_DESKTOP_NAMESPACE_BEGIN

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

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_SUBSTITUTECOLORS_HPP
