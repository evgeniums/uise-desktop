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

/** @file uise/desktop/utils/enums.hpp
*
*  Defines enums used in library.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ENUMS_HPP
#define UISE_DESKTOP_ENUMS_HPP

#include <cstdint>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

enum class Direction : uint8_t
{
    NONE=0,
    HOME=1,
    END=2
};

enum class Order : uint8_t
{
    ASC=1,
    DESC=2
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ENUMS_HPP
