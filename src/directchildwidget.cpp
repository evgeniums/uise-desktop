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

/** @file uise/desktop/src/directchildwidget.cpp
*
*  Defines directChildWidget() method.
*
*/

/****************************************************************************/

#include <uise/desktop/utils/directchildwidget.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

UISE_DESKTOP_EXPORT QWidget* directChildWidget(QWidget* parent, QWidget* nestedChild)
{
    if (nestedChild==nullptr)
    {
        return nullptr;
    }
    if (nestedChild->parent() == parent)
    {
        return nestedChild;
    }
    return directChildWidget(parent,qobject_cast<QWidget*>(nestedChild->parent()));
}

UISE_DESKTOP_NAMESPACE_END
