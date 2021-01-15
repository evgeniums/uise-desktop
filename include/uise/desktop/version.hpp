/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** \file uise/desktop/version.hpp
*
*  Defines macros for tracking the version of the library.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_VERSION_HPP
#define UISE_DESKTOP_VERSION_HPP

//! @internal
//! Transforms a (version, revision, patchlevel) triple into a number of the
//! form 0xVVRRPPPP to allow comparing versions in a normalized way.
//!
//! See http://sourceforge.net/p/predef/wiki/VersionNormalization.
#define UISE_DESKTOP_CONFIG_VERSION(version, revision, patch) \
    (((version) << 24) + ((revision) << 16) + (patch))

//! @ingroup group-config
//! Macro expanding to the major version of the library, i.e. the `x` in `x.y.z`.
#define UISE_DESKTOP_MAJOR_VERSION 0

//! @ingroup group-config
//! Macro expanding to the minor version of the library, i.e. the `y` in `x.y.z`.
#define UISE_DESKTOP_MINOR_VERSION 0

//! @ingroup group-config
//! Macro expanding to the patch level of the library, i.e. the `z` in `x.y.z`.
#define UISE_DESKTOP_PATCH_VERSION 1

//! @ingroup group-config
//! Macro expanding to the full version of the library, in hexadecimal
//! representation.
#define UISE_DESKTOP_VERSION                                            \
    UISE_DESKTOP_CONFIG_VERSION(UISE_DESKTOP_MAJOR_VERSION,                     \
                        UISE_DESKTOP_MINOR_VERSION,                     \
                        UISE_DESKTOP_PATCH_VERSION)                     \

#endif // UISE_DESKTOP_VERSION_HPP
