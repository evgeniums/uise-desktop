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

/** @file uise/desktop/utils/makenullwidget.hpp
*
*  Defines makeNullWidget().
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_MAKENULLWIDGET_HPP
#define UISE_DESKTOP_MAKENULLWIDGET_HPP

#include <QWidget>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Functor for creating null QWidget
 */
struct makeNullWidgetT
{
    QWidget* operator()(QWidget* parent=nullptr) const
    {
        return nullptr;
    }
};
/**
 * @brief Creates null QWidget.
 */
constexpr makeNullWidgetT makeNullWidget{};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_MAKENULLWIDGET_HPP
