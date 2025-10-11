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

/** @file uise/desktop/uisedesktop.hpp
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
#        ifdef UISE_DESKTOP_BUILD
#            define UISE_DESKTOP_EXPORT __declspec(dllexport)
#        else
#            define UISE_DESKTOP_EXPORT __declspec(dllimport)
#        endif
#  else
#    define UISE_DESKTOP_EXPORT
#  endif
#endif

#define UISE_DESKTOP_NAMESPACE_BEGIN namespace uise {
#define UISE_DESKTOP_NAMESPACE_END }

UISE_DESKTOP_NAMESPACE_BEGIN
UISE_DESKTOP_NAMESPACE_END

#define UISE_DESKTOP_NAMESPACE uise

#define UISE_THIRDPARTY_NAMESPACE_BEGIN namespace uise { namespace thirdparty {
#define UISE_THIRDPARTY_NAMESPACE_END }}

#define UISE_THIRDPARTY_NAMESPACE uise::thirdparty

#define UISE_DESKTOP_USING using namespace UISE_DESKTOP_NAMESPACE;

#ifdef WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif

#endif

#ifndef UNCOMMENTED_QDEBUG
#define UNCOMMENTED_QDEBUG qDebug()
#define UNCOMMENTED_QDEBUG_NL
#endif

#endif // UISE_DESKTOP_HPP
