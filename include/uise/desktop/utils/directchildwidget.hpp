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

/** @file uise/desktop/utils/directchildwidget.hpp
*
*  Declares directChildWidget() and directChildWidget() methods.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DIRECTCHILDWIDGET_HPP
#define UISE_DESKTOP_DIRECTCHILDWIDGET_HPP

#include <QDebug>
#include <QWidget>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Get direct child widget of a parent widget.
 * @param parent Parent widget to look for the direct child of.
 * @param nestedChild Child widget of a parent that can be nested.
 * @return Direct child widget of a parent that exists in the chain of parents of the nestedChild.
 */
UISE_DESKTOP_EXPORT QWidget* directChildWidget(QWidget* parent, QWidget* nestedChild);

/**
 * @brief Get direct child widget at certain position.
 * @param parent Parent widget to look for the direct child of.
 * @param pos Position where to find the child.
 * @return Found child widget.
 */
inline QWidget* directChildWidgetAt(QWidget* parent, const QPoint& pos)
{
    return directChildWidget(parent,parent->childAt(pos));
}

template <typename T>
T* findWidgetInParentChain(QWidget* widget)
{
    if (widget==nullptr)
    {
        return nullptr;
    }

    auto w=qobject_cast<T*>(widget);
    if (w!=nullptr)
    {
        return w;
    }

    return findWidgetInParentChain<T>(widget->parentWidget());
}

template <typename T>
inline T* childWidgetAt(QWidget* parent, const QPoint& pos)
{
    auto w=parent->childAt(pos);
    return findWidgetInParentChain<T>(w);
}

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_DIRECTCHILDWIDGET_HPP
