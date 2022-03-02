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

/** @file uise/desktop/src/busywaiting.cpp
*
*  Defines BusyWaiting.
*
*/

/****************************************************************************/

#include <cmath>
#include <algorithm>

#include <QPainter>
#include <QTimer>
#include <QColor>
#include <QLineEdit>

#include <uise/desktop/busywaiting.hpp>

namespace
{
//--------------------------------------------------------------------------
int lineCountDistanceFromPrimary(int current, int primary, int totalNrOfLines)
{
    int distance = primary - current;
    if (distance < 0)
    {
        distance += totalNrOfLines;
    }
    return distance;
}

//--------------------------------------------------------------------------
QColor currentLineColor(int countDistance, int totalNrOfLines,
                        qreal trailFadePerc, qreal minOpacity,
                        QColor color)
{
    if (countDistance == 0)
    {
        return color;
    }
    const qreal minAlphaF = minOpacity / 100.0;
    int distanceThreshold = static_cast<int>(ceil((totalNrOfLines - 1) * trailFadePerc / 100.0));
    if (countDistance > distanceThreshold)
    {
        color.setAlphaF(minAlphaF);
    }
    else
    {
        qreal alphaDiff = color.alphaF() - minAlphaF;
        qreal gradient = alphaDiff / static_cast<qreal>(distanceThreshold + 1);
        qreal resultAlpha = color.alphaF() - gradient * countDistance;

        // If alpha is out of bounds, clip it.
        resultAlpha = std::min(1.0, std::max(0.0, resultAlpha));
        color.setAlphaF(resultAlpha);
    }
    return color;
}

//--------------------------------------------------------------------------
}

UISE_DESKTOP_NAMESPACE_BEGIN

class BusyWaiting_p
{
    public:

        QColor  overrideColor;
        qreal   roundness=0;
        qreal   minimumTrailOpacity=0;
        qreal   trailFadePercentage=0;
        qreal   revolutionsPerSecond=0;
        int     numberOfLines=0;
        int     lineLength=0;
        int     lineWidth=0;
        int     innerRadius=0;

        QTimer *timer=nullptr;
        bool    centerOnParent=false;
        bool    disableParentWhenSpinning=false;
        int     currentCounter=0;
        bool    isSpinning=false;

        QWidget *styleSample=nullptr;
};

//--------------------------------------------------------------------------
BusyWaiting::BusyWaiting(
       QWidget *parent,
       bool centerOnParent,
       bool disableParentWhenSpinning
    ) : QFrame(parent),
        pimpl(std::make_unique<BusyWaiting_p>())
{
    pimpl->centerOnParent=centerOnParent;
    pimpl->disableParentWhenSpinning=disableParentWhenSpinning;
    initialize();
}

//--------------------------------------------------------------------------
BusyWaiting::BusyWaiting(
        Qt::WindowModality modality,
        QWidget *parent,
        bool centerOnParent,
        bool disableParentWhenSpinning
    ) : QFrame(parent, Qt::Dialog | Qt::FramelessWindowHint),
        pimpl(std::make_unique<BusyWaiting_p>())
{
    pimpl->centerOnParent=centerOnParent;
    pimpl->disableParentWhenSpinning=disableParentWhenSpinning;
    initialize();

    setWindowModality(modality);
    setAttribute(Qt::WA_TranslucentBackground);
}

//--------------------------------------------------------------------------
BusyWaiting::~BusyWaiting()
{}

//--------------------------------------------------------------------------
void BusyWaiting::initialize()
{
    pimpl->roundness = 100.0;
    pimpl->minimumTrailOpacity = 3.14159265358979323846;
    pimpl->trailFadePercentage = 80.0;
    pimpl->revolutionsPerSecond = 1.57079632679489661923;
    pimpl->numberOfLines = 20;
    pimpl->lineLength = 10;
    pimpl->lineWidth = 2;
    pimpl->innerRadius = 10;
    pimpl->currentCounter = 0;
    pimpl->isSpinning = false;

    pimpl->timer = new QTimer(this);
    connect(pimpl->timer, SIGNAL(timeout()), this, SLOT(rotate()));
    updateSize();
    updateTimer();
    hide();

    pimpl->styleSample=new QLineEdit(this);
    pimpl->styleSample->hide();
}

//--------------------------------------------------------------------------
void BusyWaiting::paintEvent(QPaintEvent *)
{
    updatePosition();
    QPainter painter(this);
    painter.fillRect(this->rect(), Qt::transparent);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (pimpl->currentCounter >= pimpl->numberOfLines)
    {
        pimpl->currentCounter = 0;
    }

    painter.setPen(Qt::NoPen);
    for (int i = 0; i < pimpl->numberOfLines; ++i)
    {
        painter.save();
        painter.translate(pimpl->innerRadius + pimpl->lineLength,
                          pimpl->innerRadius + pimpl->lineLength);
        qreal rotateAngle = static_cast<qreal>(360 * i) / static_cast<qreal>(pimpl->numberOfLines);
        painter.rotate(rotateAngle);
        painter.translate(pimpl->innerRadius, 0);
        int distance = lineCountDistanceFromPrimary(i, pimpl->currentCounter, pimpl->numberOfLines);

        QColor colour = currentLineColor(distance, pimpl->numberOfLines, pimpl->trailFadePercentage,
                                 pimpl->minimumTrailOpacity, color());
        painter.setBrush(colour);

        painter.drawRoundedRect( QRect(0, -pimpl->lineWidth / 2, pimpl->lineLength, pimpl->lineWidth), pimpl->roundness,
                    pimpl->roundness, Qt::RelativeSize);
        painter.restore();
    }
}

//--------------------------------------------------------------------------
void BusyWaiting::start()
{
    updatePosition();
    pimpl->isSpinning = true;
    show();

    if(parentWidget() && pimpl->disableParentWhenSpinning)
    {
        parentWidget()->setEnabled(false);
    }

    if (!pimpl->timer->isActive())
    {
        pimpl->timer->start();
        pimpl->currentCounter = 0;
    }
}

//--------------------------------------------------------------------------
void BusyWaiting::stop()
{
    pimpl->isSpinning = false;
    hide();

    if(parentWidget() && pimpl->disableParentWhenSpinning)
    {
        parentWidget()->setEnabled(true);
    }

    if (pimpl->timer->isActive())
    {
        pimpl->timer->stop();
        pimpl->currentCounter = 0;
    }
}

//--------------------------------------------------------------------------
void BusyWaiting::setNumberOfLines(int lines) noexcept
{
    pimpl->numberOfLines = lines;
    pimpl->currentCounter = 0;
    updateTimer();
}

//--------------------------------------------------------------------------
void BusyWaiting::setLineLength(int length) noexcept
{
    pimpl->lineLength = length;
    updateSize();
}

//--------------------------------------------------------------------------
void BusyWaiting::setLineWidth(int width) noexcept
{
    pimpl->lineWidth = width;
    updateSize();
}

//--------------------------------------------------------------------------
void BusyWaiting::setInnerRadius(int radius) noexcept
{
    pimpl->innerRadius = radius;
    updateSize();
}

//--------------------------------------------------------------------------
QColor BusyWaiting::color() const
{
    QColor baseColor;
    if (pimpl->overrideColor.isValid())
    {
        baseColor=pimpl->overrideColor;
    }
    else if (pimpl->styleSample)
    {
        baseColor=pimpl->styleSample->palette().color(QPalette::WindowText);
    }
    else
    {
        baseColor=Qt::black;
    }
    return baseColor;
}

//--------------------------------------------------------------------------
qreal BusyWaiting::roundness() const noexcept
{
    return pimpl->roundness;
}

//--------------------------------------------------------------------------
qreal BusyWaiting::minimumTrailOpacity() const noexcept
{
    return pimpl->minimumTrailOpacity;
}

//--------------------------------------------------------------------------
qreal BusyWaiting::trailFadePercentage() const noexcept
{
    return pimpl->trailFadePercentage;
}

//--------------------------------------------------------------------------
qreal BusyWaiting::revolutionsPersSecond() const noexcept
{
    return pimpl->revolutionsPerSecond;
}

//--------------------------------------------------------------------------
int BusyWaiting::numberOfLines() const noexcept
{
    return pimpl->numberOfLines;
}

//--------------------------------------------------------------------------
int BusyWaiting::lineLength() const noexcept
{
    return pimpl->lineLength;
}

//--------------------------------------------------------------------------
int BusyWaiting::lineWidth() const noexcept
{
    return pimpl->lineWidth;
}

//--------------------------------------------------------------------------
int BusyWaiting::innerRadius() const noexcept
{
    return pimpl->innerRadius;
}

//--------------------------------------------------------------------------
bool BusyWaiting::isSpinning() const noexcept
{
    return pimpl->isSpinning;
}

//--------------------------------------------------------------------------
void BusyWaiting::setRoundness(qreal roundness) noexcept
{
    pimpl->roundness = std::max(0.0, std::min(100.0, roundness));
}

//--------------------------------------------------------------------------
void BusyWaiting::setOverrideColor(QColor color) noexcept
{
    pimpl->overrideColor = std::move(color);
}

//--------------------------------------------------------------------------
void BusyWaiting::setRevolutionsPerSecond(qreal revolutionsPerSecond) noexcept
{
    pimpl->revolutionsPerSecond = revolutionsPerSecond;
    updateTimer();
}

//--------------------------------------------------------------------------
void BusyWaiting::setTrailFadePercentage(qreal trail) noexcept
{
    pimpl->trailFadePercentage = trail;
}

//--------------------------------------------------------------------------
void BusyWaiting::setMinimumTrailOpacity(qreal minimumTrailOpacity) noexcept
{
    pimpl->minimumTrailOpacity = minimumTrailOpacity;
}

//--------------------------------------------------------------------------
void BusyWaiting::rotate()
{
    ++pimpl->currentCounter;
    if (pimpl->currentCounter >= pimpl->numberOfLines)
    {
        pimpl->currentCounter = 0;
    }
    update();
}

//--------------------------------------------------------------------------
void BusyWaiting::updateSize()
{
    int size = (pimpl->innerRadius + pimpl->lineLength) * 2;
    setFixedSize(size, size);
}

//--------------------------------------------------------------------------
void BusyWaiting::updateTimer()
{
    pimpl->timer->setInterval(1000 / (pimpl->numberOfLines * pimpl->revolutionsPerSecond));
}

//--------------------------------------------------------------------------
void BusyWaiting::updatePosition()
{
    if (parentWidget() && pimpl->centerOnParent)
    {
        move(parentWidget()->width() / 2 - width() / 2,
             parentWidget()->height() / 2 - height() / 2);
    }
}

//--------------------------------------------------------------------------
void BusyWaiting::setStyleSample(
        QWidget *widget
    )
{
    pimpl->styleSample=widget;
    if (pimpl->styleSample!=nullptr)
    {
        pimpl->styleSample->setParent(this);
        pimpl->styleSample->setVisible(false);
        pimpl->styleSample->setProperty("style-sample",true);
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
