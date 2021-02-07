/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/desktop/utils/singleshottimer.hpp
*
*  Defines SingleShotTimer.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_SINGLESHOTTIMER_HPP
#define UISE_DESKTOP_SINGLESHOTTIMER_HPP

#include <functional>

#include <QTimer>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Single shot timer that invokes a handler at most once.
 *
 * Sucessive calls of shot() will result in invokation of the handler from the last call only.
 */
class UISE_DESKTOP_EXPORT SingleShotTimer : public QObject
{
    Q_OBJECT

    public:

        using HandlerT=std::function<void ()>;

        /**
         * @brief SingleShotTimer
         * @param parent Parent QObject.
         */
        SingleShotTimer(QObject* parent=nullptr);

        /**
         * @brief Run timer.
         * @param milliseconds Timer interval.
         * @param handler Handler to invoke after timeout.
         *
         * Successive calls to shot() method before timer is invoked will result in invoking the handler of the last call.
         */
        void shot(
            size_t milliseconds,
            HandlerT handler
        );

        /**
         * @brief Stop timer and clear handler.
         */
        void clear();

    private slots:

        void onTimeout();

    private:

        QTimer m_timer;
        HandlerT m_handler;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_SINGLESHOTTIMER_HPP
