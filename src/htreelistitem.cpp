/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/htreelistitem.cpp
*
*  Defines HTreeListItem.
*
*/

/****************************************************************************/

#include <uise/desktop/htreelistitem.hpp>

#include <uise/desktop/ipp/htreelistitemtemplate.ipp>
#include <uise/desktop/ipp/htreeflyweightlistitem.ipp>

#include <uise/desktop/htreelistitemexport.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

#ifndef _MSC_VER
template class HTreeListItemT<QFrame>;
template class HTreeFlyweightListItem<QFrame>;
#endif

UISE_DESKTOP_NAMESPACE_END
