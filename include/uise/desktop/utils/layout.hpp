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
#include <QWidget>

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

        /**
         * @brief Force every ancestor's layout to re-activate synchronously, right now.
         *
         * QWidget::updateGeometry() -- which is what a hidden/resized layout item uses to tell
         * its containing layout that its contribution to sizeHint() changed -- does not
         * recompute anything synchronously: it posts a QEvent::LayoutRequest, processed on the
         * NEXT event loop iteration. Until then, the widget's actual on-screen geometry (and
         * everything a containing layout places relative to it) stays stale, which reads as a
         * brief, visible size/position glitch -- or, if a widget in the same tree paints with a
         * transparent QSS background, a region vacated by a shrinking sibling can composite the
         * new transparent pixels directly over stale ones still sitting in the backing store.
         * Call this to force the correction now instead of waiting for that deferred event --
         * e.g. right after building/reconfiguring a subtree that has never been laid out yet, or
         * right before the widget's own current geometry is read for further placement.
         *
         * Was duplicated verbatim across several call sites (FastSwitchButton, ForwardDialog,
         * ReplyDialog, FileUploadWidget) before being promoted here.
         *
         * @param widget Widget to start from; its own layout (if any) is activated first, then
         * every ancestor's, walking up via parentWidget().
         */
        static void activateUpward(QWidget* widget)
        {
            for (auto* w=widget; w!=nullptr; w=w->parentWidget())
            {
                if (w->layout()!=nullptr)
                {
                    w->layout()->invalidate();
                    w->layout()->activate();
                }

                // layout()->activate() moves/resizes children synchronously via setGeometry(),
                // which schedules a repaint of the affected regions -- but only once this event
                // loop turn is processed. Force it explicitly instead of trusting that to happen
                // before the next paint (see the class comment above for why that matters).
                w->update();
            }
        }
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_LAYOUT_HPP
