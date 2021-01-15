/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** \file uise/desktop/uisedesktop.hpp
*
*  Defines macros for uise-desktop library.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HPP
#define UISE_DESKTOP_HPP

// define export symbols for windows platform
#ifndef UISE_DESKTOP_EXPORT
#  if defined(WIN32)
#        ifdef BUILD_UISE_DESKTOP
#            define UISE_DESKTOP_EXPORT __declspec(dllexport)
#        else
#            define UISE_DESKTOP_EXPORT __declspec(dllimport)
#        endif
#  else
#    define UISE_DESKTOP_EXPORT
#  endif
#endif

#define UISE_DESKTOP_NAMESPACE_BEGIN namespace uise { namespace desktop {
#define UISE_DESKTOP_NAMESPACE_EMD }}

#endif // UISE_DESKTOP_HPP
