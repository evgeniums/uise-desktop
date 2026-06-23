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

        virtual bool isCenterAligned() const
        {
            return true;
        }

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

/**
 * @brief Factory selector for operation-wait indicators (e.g. spinners).
 *
 * Register a concrete widget under this type in the WidgetFactory to provide
 * the default busy-waiting indicator used by LoadingFrame / FrameWithModalStatus.
 * Use makeWidget<AbstractOperationLoadingWidget>() to create one.
 */
class UISE_DESKTOP_EXPORT AbstractOperationLoadingWidget : public AbstractLoadingWidget
{
    Q_OBJECT

    public:

        using AbstractLoadingWidget::AbstractLoadingWidget;

        ~AbstractOperationLoadingWidget() override;
};

/**
 * @brief Factory selector for panel-content loading indicators (e.g. skeleton shimmer).
 *
 * Register a concrete widget under this type in the WidgetFactory to provide
 * the default skeleton/placeholder widget used when panel content is loading.
 * Use makeWidget<AbstractPanelLoadingWidget>() to create one.
 */
class UISE_DESKTOP_EXPORT AbstractPanelLoadingWidget : public AbstractLoadingWidget
{
    Q_OBJECT

    public:

        using AbstractLoadingWidget::AbstractLoadingWidget;

        ~AbstractPanelLoadingWidget() override;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACT_LOADING_WIDGET_HPP
