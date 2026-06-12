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

/** @file uise/desktop/htreelistitemexport.hpp
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_LIST_ITEM_EXPORT_HPP
#define UISE_DESKTOP_HTREE_LIST_ITEM_EXPORT_HPP

#include <uise/desktop/htreelistitem.hpp>
#include <uise/desktop/htreeflyweightlistitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

// Build side only: explicit instantiation definitions with dllexport.
// On the consumer side the suppression is done by the extern template
// declarations at the bottom of htreelistitemtemplate.hpp and
// htreeflyweightlistitem.hpp, which every TU that can instantiate sees.
// (The previous dllimport explicit instantiation here was an MSVC-only
// idiom: clang-cl still emitted local definitions in TUs that did not
// include this header, causing LNK2005 against the import library.)
#if defined(_MSC_VER) && defined(UISE_DESKTOP_BUILD)
template class UISE_DESKTOP_EXPORT HTreeListItemT<QFrame>;
template class UISE_DESKTOP_EXPORT HTreeFlyweightListItem<QFrame>;
#endif

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_LIST_ITEM_EXPORT_HPP
