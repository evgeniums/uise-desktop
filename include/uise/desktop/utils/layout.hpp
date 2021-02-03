/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/desktop/utils/layout.hpp
*
*  Defines hepler class to work with Qt layouts.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_LAYOUT_HPP
#define UISE_DESKTOP_LAYOUT_HPP

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Hepler class to work with Qt layouts.
 */
class Layout
{
    public:

        /**
         * @brief Clear layout.
         * @param layout Layout to clear.
         */
        static void clear(QLayout* layout)
        {
            layout->setContentsMargins(0,0,0,0);
            layout->setSpacing(0);
        }

        /**
         * @brief Create layout for widget.
         * @param widget Widget to create layout for.
         * @param reset If set (default) then reset created layout.
         * @return Created layout.
         */
        template <typename LayoutT, typename WidgetT>
        static LayoutT* create(WidgetT* widget, bool reset=true)
        {
            delete widget->layout();

            auto layout=new LayoutT(widget);
            if (reset)
            {
                clear(layout);
            }
            return layout;
        }

        /**
         * @brief Create QVBoxLayout for widget.
         * @param widget Widget to create layout for.
         * @param reset If set (default) then reset created layout.
         * @return Created layout.
         */
        template <typename WidgetT>
        static QVBoxLayout* vertical(WidgetT* widget, bool reset=true)
        {
            return create<QVBoxLayout>(widget,reset);
        }

        /**
         * @brief Create QHBoxLayout for widget.
         * @param widget Widget to create layout for.
         * @param reset If set (default) then reset created layout.
         * @return Created layout.
         */
        template <typename WidgetT>
        static QHBoxLayout* horizontal(WidgetT* widget, bool reset=true)
        {
            return create<QHBoxLayout>(widget,reset);
        }

        /**
         * @brief Create QGridLayout for widget.
         * @param widget Widget to create layout for.
         * @param reset If set (default) then reset created layout.
         * @return Created layout.
         */
        template <typename WidgetT>
        static QGridLayout* grid(WidgetT* widget, bool reset=true)
        {
            return create<QGridLayout>(widget,reset);
        }

        /**
         * @brief Create QBoxLayout for widget.
         * @param widget Widget to create layout for.
         * @param orientation Orintation of the layout.
         * @param reset If set (default) then reset created layout.
         * @return Created layout.
         */
        template <typename WidgetT>
        static QBoxLayout* box(WidgetT* widget, Qt::Orientation orientation, bool reset=true)
        {
            return (orientation==Qt::Horizontal)?
                        static_cast<QBoxLayout*>(horizontal(widget,reset))
                      :
                        static_cast<QBoxLayout*>(vertical(widget,reset));
        }

        static Qt::Orientation orthOrientation(Qt::Orientation orientation) noexcept
        {
            return orientation==Qt::Horizontal?Qt::Vertical:Qt::Horizontal;
        }
};

UISE_DESKTOP_NAMESPACE_EMD

#endif // UISE_DESKTOP_LAYOUT_HPP
