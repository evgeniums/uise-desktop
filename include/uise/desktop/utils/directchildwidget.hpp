/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** \file uise/desktop/utils/directchildwidget.hpp
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
 * @brief Get direct child widget of a parent widget where .
 * @param parent Parent widget to look for the direct child of.
 * @param nestedChild Child widget of a parent that can be nested.
 * @return Direct child widget of a parent that exists in the parents chain of nestedChild.
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

UISE_DESKTOP_NAMESPACE_EMD

#endif // UISE_DESKTOP_DIRECTCHILDWIDGET_HPP
