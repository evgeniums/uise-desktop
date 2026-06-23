/**
@copyright Evgeny Sidorov 2022-2025

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/abstractloadingwidget.hpp
*
*  Declares AbstractLoadingWidget.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACT_LOADING_WIDGET_HPP
#define UISE_DESKTOP_ABSTRACT_LOADING_WIDGET_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Abstract base for animated loading/busy widgets.
 */
class UISE_DESKTOP_EXPORT AbstractLoadingWidget : public WidgetQFrame
{
    Q_OBJECT

    public:

        using WidgetQFrame::WidgetQFrame;

        ~AbstractLoadingWidget() override;

        /**
         * @brief Check if spinner is running.
         * @return Query result.
         */
        virtual bool isRunning() const noexcept =0;

    signals:

        void cancelled();

    public slots:

        /**
         * @brief Run widget.
         */
        virtual void start() =0;

        /**
         * @brief Stop widget.
         */
        virtual void stop() =0;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACT_LOADING_WIDGET_HPP
