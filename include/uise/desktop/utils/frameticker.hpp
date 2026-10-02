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

/** @file uise/desktop/utils/frameticker.hpp
*
*  Defines FrameTicker.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FRAMETICKER_HPP
#define UISE_DESKTOP_FRAMETICKER_HPP

#include <functional>

#include <QAbstractAnimation>

#include <uise/desktop/uisedesktop.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro -- see the note in
// singleshottimer.hpp.
namespace uise {

/**
 * @brief Open-ended per-frame callback driven by Qt's shared animation timer.
 *
 * Runs the handler once per animation frame (the same timer that drives QVariantAnimation, so it
 * is frame-aligned with other animations) until stopTicking() is called. The handler receives the
 * number of milliseconds elapsed since the previous frame. The first frame after startTicking()
 * only records the start time and does not invoke the handler, so starting from inside some other
 * operation never runs a frame synchronously.
 *
 * Stopping from inside the handler is safe.
 */
class UISE_DESKTOP_EXPORT FrameTicker : public QAbstractAnimation
{
    public:

        using HandlerT=std::function<void (int)>;

        /**
         * @brief Constructor.
         * @param parent Parent QObject.
         */
        FrameTicker(QObject* parent=nullptr);

        /**
         * @brief Set handler invoked on each frame.
         * @param handler Handler taking milliseconds elapsed since the previous frame.
         */
        void setHandler(HandlerT handler);

        /**
         * @brief Start ticking. Does nothing if already ticking.
         */
        void startTicking();

        /**
         * @brief Stop ticking. Does nothing if not ticking.
         */
        void stopTicking();

        /**
         * @brief Check if ticking.
         */
        bool isTicking() const
        {
            return state()==QAbstractAnimation::Running;
        }

        /**
         * @brief Duration of the animation, always infinite.
         */
        int duration() const override
        {
            return -1;
        }

    protected:

        void updateCurrentTime(int currentTime) override;

    private:

        HandlerT m_handler;
        int m_lastTime=-1;
};

}

#endif // UISE_DESKTOP_FRAMETICKER_HPP
