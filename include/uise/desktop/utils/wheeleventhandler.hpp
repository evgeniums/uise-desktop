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

/** @file uise/desktop/wheeleventhandler.hpp
*
*  Declares WheelEventHandler.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_MOUSEWHEEL_HPP
#define UISE_DESKTOP_MOUSEWHEEL_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Handler of mouse wheel events.
 */
class UISE_DESKTOP_EXPORT WheelEventHandler
{
    public:

        /**
         * @brief Constructor.
         * @param Scroll step to use as scrolling factor.
         */
        WheelEventHandler(int scrollStep=4);

        /**
         * @brief Handle wheel event.
         * @param event Wheel event.
         * @return Scrolling delta.
         */
        int handleWheelEvent(QWheelEvent *event);

        /**
         * @brief Reset accumulated wheel offset.
         */
        void resetWheel() noexcept
        {
            m_offsetAccumulated=0;
        }

        /**
         * @brief Set scrolling step.
         * @param val Value.
         */
        void setWheelScrollStep(int val) noexcept
        {
            m_scrollStep=val;
        }

        /**
         * @brief Get scrolling step.
         * @return Query result.
         */
        int wheelScrollStep() const noexcept
        {
            return m_scrollStep;
        }

    private:

        float m_offsetAccumulated;
        int m_scrollStep;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_MOUSEWHEEL_HPP
