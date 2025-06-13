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

/** @file uise/desktop/defaultwidgetfactory.hpp
*
*  Declares widget factory.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DEFAULT_WIDGET_FACTORY_HPP
#define UISE_DESKTOP_DEFAULT_WIDGET_FACTORY_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/widgetfactory.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

std::shared_ptr<WidgetFactory> UISE_DESKTOP_EXPORT defaultWidgetFactory();

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_DEFAULT_WIDGET_FACTORY_HPP
