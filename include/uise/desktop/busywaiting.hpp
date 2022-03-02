/* Original Work Copyright (c) 2012-2014 Alexander Turkin
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

class UISE_DESKTOP_EXPORT BusyWaiting : public QFrame
{
    Q_OBJECT

    public:

        BusyWaiting(QWidget *parent = nullptr,
                    bool centerOnParent = true,
                    bool disableParentWhenSpinning = true);

        BusyWaiting(Qt::WindowModality modality,
                    QWidget *parent = nullptr,
                    bool centerOnParent = true,
                    bool disableParentWhenSpinning = true);

        ~BusyWaiting();

        BusyWaiting(const BusyWaiting&)=delete;
        BusyWaiting(BusyWaiting&&)=delete;
        BusyWaiting& operator=(const BusyWaiting&)=delete;
        BusyWaiting& operator=(BusyWaiting&&)=delete;

    public slots:

        void start();
        void stop();

    public:

        void setStyleSample(QWidget* widget);

        void setOverrideColor(QColor color) noexcept;
        void setRoundness(qreal roundness) noexcept;
        void setMinimumTrailOpacity(qreal minimumTrailOpacity) noexcept;
        void setTrailFadePercentage(qreal trail) noexcept;
        void setRevolutionsPerSecond(qreal revolutionsPerSecond) noexcept;
        void setNumberOfLines(int lines) noexcept;
        void setLineLength(int length) noexcept;
        void setLineWidth(int width) noexcept;
        void setInnerRadius(int radius) noexcept;

        QColor color() const;
        qreal roundness() const noexcept;
        qreal minimumTrailOpacity() const noexcept;
        qreal trailFadePercentage() const noexcept;
        qreal revolutionsPersSecond() const noexcept;
        int numberOfLines() const noexcept;
        int lineLength() const noexcept;
        int lineWidth() const noexcept;
        int innerRadius() const noexcept;

        bool isSpinning() const noexcept;

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
