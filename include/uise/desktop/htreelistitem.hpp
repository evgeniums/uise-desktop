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

/** @file uise/desktop/htreelistitem.hpp
*
*  Declares list item of horizontal tree branch.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_LIST_ITEM_HPP
#define UISE_DESKTOP_HTREE_LIST_ITEM_HPP

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

#include <uise/desktop/htreelistitemtemplate.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

using HTreeListItem=HTreeListItemT<QFrame>;

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_LIST_ITEM_HPP
