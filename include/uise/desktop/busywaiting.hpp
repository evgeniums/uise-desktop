/**

Original Work Copyright (c) 2012-2014 Alexander Turkin
   Modified 2014 by William Hallatt
   Modified 2015 by Jacob Dawid
   Modified 2022 by Evgeny Sidorov
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

/****************************************************************************/

/** @file uise/desktop/busywaiting.hpp
*
*  Declares BusyWaiting.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_BUSYWAITING_HPP
#define UISE_DESKTOP_BUSYWAITING_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class BusyWaiting_p;

/**
 * @brief Animated spinner showing busy/loading/wait state.
 */
class UISE_DESKTOP_EXPORT BusyWaiting : public QFrame
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         * @param centerOnParent Center on parent widget.
         * @param disableParentWhenSpinning DIsable parent widget when busy is enabled.
         */
        BusyWaiting(QWidget *parent = nullptr,
                    bool centerOnParent = true,
                    bool disableParentWhenSpinning = true);

        /**
         * @brief Constructor with modality.
         * @param modality Modality mode.
         * @param parent Parent widget.
         * @param centerOnParent Center on parent widget.
         * @param disableParentWhenSpinning DIsable parent widget when busy is enabled.
         */
        BusyWaiting(Qt::WindowModality modality,
                    QWidget *parent = nullptr,
                    bool centerOnParent = true,
                    bool disableParentWhenSpinning = true);

        /**
         * @brief DEstructor.
        */
        ~BusyWaiting();

        BusyWaiting(const BusyWaiting&)=delete;
        BusyWaiting(BusyWaiting&&)=delete;
        BusyWaiting& operator=(const BusyWaiting&)=delete;
        BusyWaiting& operator=(BusyWaiting&&)=delete;

    public slots:

        /**
         * @brief Run widget.
         */
        void start();

        /**
         * @brief Stop widget.
         */
        void stop();

    public:

        /**
         * @brief Set sample widget to be used as style reference.
         * @param widget Reference widget.
         *
         * This object will own sample widget.
         */
        void setStyleSample(QWidget* widget);

        /**
         * @brief Set lines color overriding color derived from current style.
         * @param color Lines color.
         */
        void setOverrideColor(QColor color) noexcept;

        /**
         * @brief Get override color.
         * @return Query result.
         */
        QColor overrideColor() const noexcept;

        /**
         * @brief Get actual color.
         * @return Query result.
         */
        QColor color() const;

        /**
         * @brief Set lines roudness.
         * @param roundness Lines roudness.
         */
        void setRoundness(qreal roundness) noexcept;

        /**
         * @brief Get roudness.
         * @return Query result.
         */
        qreal roundness() const noexcept;

        /**
         * @brief Set minimul opacity of lines in spinner trail.
         * @param minimumTrailOpacity Opacity.
         */
        void setMinimumTrailOpacity(qreal minimumTrailOpacity) noexcept;

        /**
         * @brief Get minimul opacity of lines in spinner trail.
         * @return Query result.
         */
        qreal minimumTrailOpacity() const noexcept;

        /**
         * @brief Set percentage of lines in trail that must be faded.
         * @param trail Percentage.
         */
        void setTrailFadePercentage(qreal trail) noexcept;

        /**
         * @brief Get percentage of lines in trail that must be faded.
         * @return Query result.
         */
        qreal trailFadePercentage() const noexcept;

        /**
         * @brief Set spinner speed in revolutions per second.
         * @param revolutionsPerSecond speed.
         */
        void setRevolutionsPerSecond(qreal revolutionsPerSecond) noexcept;

        /**
         * @brief Get spinner speed in revolutions per second.
         * @return Query result.
         */
        qreal revolutionsPersSecond() const noexcept;

        /**
         * @brief Set number of lines.
         * @param lines Number of lines.
         */
        void setNumberOfLines(int lines) noexcept;

        /**
         * @brief Get number of lines.
         * @return Query result.
         */
        int numberOfLines() const noexcept;

        /**
         * @brief Set length of lines.
         * @param length Length of lines.
         */
        void setLineLength(int length) noexcept;

        /**
         * @brief Get length of lines.
         * @return Query result.
         */
        int lineLength() const noexcept;

        /**
         * @brief Set width of lines.
         * @param width Width of lines.
         */
        void setLineWidth(int width) noexcept;

        /**
         * @brief Get width of lines.
         * @return Query result.
         */
        int lineWidth() const noexcept;

        /**
         * @brief Set inner radius.
         * @param radius Inner radius.
         */
        void setInnerRadius(int radius) noexcept;

        /**
         * @brief Get inner radius.
         * @return Query result.
         */
        int innerRadius() const noexcept;

        /**
         * @brief Check if spinner is running.
         * @return Query result.
         */
        bool isRunning() const noexcept;

    private slots:

        void rotate();

    protected:

        void paintEvent(QPaintEvent *paintEvent);

    private:

        void initialize();
        void updateSize();
        void updateTimer();
        void updatePosition();

        std::unique_ptr<BusyWaiting_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_BUSYWAITING_HPP
