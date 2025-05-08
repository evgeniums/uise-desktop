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
*  Defines destroyWidget() method.
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
        widget->setParent(nullptr);
        widget->deleteLater();
    }
}

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_DESTROYWIDGET_HPP
