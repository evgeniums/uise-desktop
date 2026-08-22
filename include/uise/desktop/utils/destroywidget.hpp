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

/** @file uise/desktop/utils/destroywidget.hpp
*
*  Defines destroyWidget() and destroyWidgetFast() methods.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DESTROYWIDGET_HPP
#define UISE_DESKTOP_DESTROYWIDGET_HPP

#include <QWidget>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Safely destroy widget.
 * @param widget Widget to destroy.
 *
 * The widget is hidden first and the destroyed with deletelater() in the next step of processing loop.
 */
inline void destroyWidget(QWidget* widget)
{
    if (widget)
    {
        widget->setVisible(false);
/**
 * Reparting causes cascade restyling whch is redundant when detroyng widget
 * **/
#if 0
        widget->setParent(nullptr);
#endif
        widget->deleteLater();
    }
}

/**
 * @brief Safely destroy widget without reparenting it first.
 * @param widget Widget to destroy.
 *
 * Same as destroyWidget() but skips setParent(nullptr). Reparenting a widget with an app-wide
 * stylesheet in effect triggers QWidgetPrivate::inheritStyle(), which re-polishes the whole
 * descendant subtree against the QSS -- work that is entirely wasted for a widget that is about
 * to be deleted anyway. Use this for widgets torn down in bulk (e.g. list views clearing their
 * items) where the reparent step buys nothing observable before deleteLater() runs.
 */
inline void destroyWidgetFast(QWidget* widget)
{
    if (widget)
    {
        widget->setVisible(false);
        widget->deleteLater();
    }
}

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_DESTROYWIDGET_HPP
