/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** \file uise/desktop/utils/layout.hpp
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

class Layout
{
    public:

        static void clear(QLayout* layout)
        {
            layout->setContentsMargins(0,0,0,0);
            layout->setSpacing(0);
        }

        template <typename LayoutT, typename WidgetT>
        static LayoutT* create(WidgetT* widget, bool reset=true)
        {
            auto layout=new LayoutT(widget);
            if (reset)
            {
                clear(layout);
            }
            return layout;
        }

        template <typename WidgetT>
        static QVBoxLayout* vertical(WidgetT* widget, bool reset=true)
        {
            return create<QVBoxLayout>(widget,reset);
        }

        template <typename WidgetT>
        static QHBoxLayout* horizontal(WidgetT* widget, bool reset=true)
        {
            return create<QHBoxLayout>(widget,reset);
        }

        template <typename WidgetT>
        static QGridLayout* grid(WidgetT* widget, bool reset=true)
        {
            return create<QGridLayout>(widget,reset);
        }

        template <typename WidgetT>
        static QBoxLayout* box(WidgetT* widget, Qt::Orientation orientation, bool reset=true)
        {
            return (orientation==Qt::Horizontal)?
                        static_cast<QBoxLayout*>(horizontal(widget,reset))
                      :
                        static_cast<QBoxLayout*>(vertical(widget,reset));
        }
};

UISE_DESKTOP_NAMESPACE_EMD

#endif // UISE_DESKTOP_LAYOUT_HPP
