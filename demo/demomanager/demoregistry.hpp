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

/** @file demo/demomanager/demoregistry.hpp
*
*  Declares the compiled-in registry of demo applications.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DEMO_MANAGER_REGISTRY_HPP
#define UISE_DESKTOP_DEMO_MANAGER_REGISTRY_HPP

#include <vector>

//--------------------------------------------------------------------------

/**
 * @brief Description of a single demo application.
 *
 * The registry is generated at configure time from the TITLE/DESCRIPTION
 * arguments passed to uise_demo() for every demo (see demo/CMakeLists.txt
 * and cmake/uisedemo.cmake), so it always matches the demos actually built.
 */
struct DemoInfo
{
    const char* executable;
    const char* title;
    const char* description;
};

//! Registry of all demo applications, sorted by title.
const std::vector<DemoInfo>& demoRegistry();

//--------------------------------------------------------------------------

#endif // UISE_DESKTOP_DEMO_MANAGER_REGISTRY_HPP
