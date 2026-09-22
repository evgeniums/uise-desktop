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

/** @file uise/desktop/src/waveformbar.cpp
*
*  Defines WaveformBar – a seekable, optionally croppable audio waveform or linear progress bar.
*
*/

/****************************************************************************/

#include <algorithm>
#include <cmath>

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

#include <uise/desktop/waveformbar.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

namespace {

constexpr int VerticalPadding=2;
constexpr int LineTrackHeight=3;
constexpr int LineKnobRadius=5;
//! Extra pixels either side of a crop handle that still grab it, so it is easy to hit.
constexpr int HandleGrabSlack=4;

qreal clampFraction(qreal value) noexcept
{
    return std::min<qreal>(1.0,std::max<qreal>(0.0,value));
}

}

//==========================================================================
// Private data
//==========================================================================

class WaveformBar_p
{
    public:

        enum class Drag
        {
            None,
            Seek,
            CropStart,
            CropEnd
        };

        WaveformBar::Style style=WaveformBar::Style::Bars;
        QByteArray waveform;
        qreal progress=0.0;

        bool seekable=true;
        bool croppable=false;
        qreal cropStart=0.0;
        qreal cropEnd=1.0;
        qreal minimumCropFraction=0.05;

        Drag drag=Drag::None;

        //! Where inside a crop handle it was grabbed, relative to the crop edge it moves, so the
        //! handle does not jump to the pointer on the first move.
        qreal dragOffset=0.0;

        // task-voice-messages-plan.md S4f item 2: mirrors light/waveformbar.qss's barColor, so an
        // unstyled bar isn't stuck on the retired, lower-contrast pale blue.
        QColor barColor{0x7d,0xad,0xe1};
        QColor progressColor{0x1a,0x6b,0xc4};
        QColor cropColor{0x1a,0x6b,0xc4};
        int barWidth=3;
        int barSpacing=2;
        int minBarHeight=3;
        int handleWidth=6;
        qreal outsideOpacity=0.35;

        //! Left and right margin: room for the crop handles, which sit OUTSIDE the range.
        int margin() const noexcept
        {
            return croppable?handleWidth:0;
        }
};

//==========================================================================
// WaveformBar
//==========================================================================

//--------------------------------------------------------------------------
WaveformBar::WaveformBar(QWidget* parent)
    : QWidget(parent),
      pimpl(std::make_unique<WaveformBar_p>())
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover,true);
    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------
WaveformBar::~WaveformBar()=default;

//--------------------------------------------------------------------------
void WaveformBar::setStyle(Style style)
{
    if (pimpl->style!=style)
    {
        pimpl->style=style;
        update();
    }
}

//--------------------------------------------------------------------------
WaveformBar::Style WaveformBar::style() const noexcept
{
    return pimpl->style;
}

//--------------------------------------------------------------------------
void WaveformBar::setWaveform(const QByteArray& waveform)
{
    if (pimpl->waveform!=waveform)
    {
        pimpl->waveform=waveform;
        update();
    }
}

//--------------------------------------------------------------------------
const QByteArray& WaveformBar::waveform() const noexcept
{
    return pimpl->waveform;
}

//--------------------------------------------------------------------------
void WaveformBar::setProgress(qreal fraction)
{
    // While the user is dragging, the bar follows the finger, not the playback position.
    if (pimpl->drag==WaveformBar_p::Drag::Seek)
    {
        return;
    }

    fraction=clampFraction(fraction);
    if (!qFuzzyCompare(1.0+pimpl->progress,1.0+fraction))
    {
        pimpl->progress=fraction;
        update();
    }
}

//--------------------------------------------------------------------------
qreal WaveformBar::progress() const noexcept
{
    return pimpl->progress;
}

//--------------------------------------------------------------------------
void WaveformBar::setSeekable(bool enable)
{
    pimpl->seekable=enable;
    if (!enable && pimpl->drag==WaveformBar_p::Drag::Seek)
    {
        // no release would ever end this gesture now that presses are refused
        pimpl->drag=WaveformBar_p::Drag::None;
    }
    setCursor(enable?Qt::PointingHandCursor:Qt::ArrowCursor);
    update();
}

//--------------------------------------------------------------------------
bool WaveformBar::isSeekable() const noexcept
{
    return pimpl->seekable;
}

//--------------------------------------------------------------------------
bool WaveformBar::isSeeking() const noexcept
{
    return pimpl->drag==WaveformBar_p::Drag::Seek;
}

//--------------------------------------------------------------------------
void WaveformBar::setCroppable(bool enable)
{
    if (pimpl->croppable!=enable)
    {
        pimpl->croppable=enable;
        if (!enable && (pimpl->drag==WaveformBar_p::Drag::CropStart || pimpl->drag==WaveformBar_p::Drag::CropEnd))
        {
            pimpl->drag=WaveformBar_p::Drag::None;
        }
        updateGeometry();
        update();
    }
}

//--------------------------------------------------------------------------
bool WaveformBar::isCroppable() const noexcept
{
    return pimpl->croppable;
}

//--------------------------------------------------------------------------
void WaveformBar::setCropRange(qreal start, qreal end)
{
    start=clampFraction(start);
    end=clampFraction(end);

    // keep the two apart, moving whichever end has room
    const auto minimum=pimpl->minimumCropFraction;
    if (end-start<minimum)
    {
        if (start+minimum<=1.0)
        {
            end=start+minimum;
        }
        else
        {
            end=1.0;
            start=1.0-minimum;
        }
    }

    pimpl->cropStart=start;
    pimpl->cropEnd=end;
    update();
}

//--------------------------------------------------------------------------
qreal WaveformBar::cropStart() const noexcept
{
    return pimpl->cropStart;
}

//--------------------------------------------------------------------------
qreal WaveformBar::cropEnd() const noexcept
{
    return pimpl->cropEnd;
}

//--------------------------------------------------------------------------
void WaveformBar::setMinimumCropFraction(qreal fraction)
{
    pimpl->minimumCropFraction=std::min<qreal>(0.5,std::max<qreal>(0.0,fraction));

    // the live range may now be narrower than allowed
    setCropRange(pimpl->cropStart,pimpl->cropEnd);
}

//--------------------------------------------------------------------------
qreal WaveformBar::minimumCropFraction() const noexcept
{
    return pimpl->minimumCropFraction;
}

//--------------------------------------------------------------------------
void WaveformBar::setBarColor(const QColor& color)
{
    pimpl->barColor=color;
    update();
}

//--------------------------------------------------------------------------
QColor WaveformBar::barColor() const noexcept
{
    return pimpl->barColor;
}

//--------------------------------------------------------------------------
void WaveformBar::setProgressColor(const QColor& color)
{
    pimpl->progressColor=color;
    update();
}

//--------------------------------------------------------------------------
QColor WaveformBar::progressColor() const noexcept
{
    return pimpl->progressColor;
}

//--------------------------------------------------------------------------
void WaveformBar::setCropColor(const QColor& color)
{
    pimpl->cropColor=color;
    update();
}

//--------------------------------------------------------------------------
QColor WaveformBar::cropColor() const noexcept
{
    return pimpl->cropColor;
}

//--------------------------------------------------------------------------
void WaveformBar::setBarWidth(int px)
{
    pimpl->barWidth=std::max(1,px);
    update();
}

//--------------------------------------------------------------------------
int WaveformBar::barWidth() const noexcept
{
    return pimpl->barWidth;
}

//--------------------------------------------------------------------------
void WaveformBar::setBarSpacing(int px)
{
    pimpl->barSpacing=std::max(0,px);
    update();
}

//--------------------------------------------------------------------------
int WaveformBar::barSpacing() const noexcept
{
    return pimpl->barSpacing;
}

//--------------------------------------------------------------------------
void WaveformBar::setMinBarHeight(int px)
{
    pimpl->minBarHeight=std::max(1,px);
    update();
}

//--------------------------------------------------------------------------
int WaveformBar::minBarHeight() const noexcept
{
    return pimpl->minBarHeight;
}

//--------------------------------------------------------------------------
void WaveformBar::setHandleWidth(int px)
{
    pimpl->handleWidth=std::max(2,px);
    update();
}

//--------------------------------------------------------------------------
int WaveformBar::handleWidth() const noexcept
{
    return pimpl->handleWidth;
}

//--------------------------------------------------------------------------
void WaveformBar::setOutsideOpacity(qreal opacity)
{
    pimpl->outsideOpacity=clampFraction(opacity);
    update();
}

//--------------------------------------------------------------------------
qreal WaveformBar::outsideOpacity() const noexcept
{
    return pimpl->outsideOpacity;
}

//--------------------------------------------------------------------------
int WaveformBar::visibleBarCount() const
{
    if (pimpl->style!=Style::Bars)
    {
        return 0;
    }

    const auto inner=std::max(0,width()-2*pimpl->margin());
    const auto pitch=pimpl->barWidth+pimpl->barSpacing;
    const auto fit=std::max(1,(inner+pimpl->barSpacing)/pitch);

    if (pimpl->waveform.isEmpty())
    {
        return fit;
    }
    return std::min(fit,static_cast<int>(pimpl->waveform.size()));
}

//--------------------------------------------------------------------------
qreal WaveformBar::fractionAtX(qreal x) const
{
    const auto margin=pimpl->margin();
    const auto inner=static_cast<qreal>(std::max(1,width()-2*margin));
    return clampFraction((x-margin)/inner);
}

//--------------------------------------------------------------------------
QSize WaveformBar::sizeHint() const
{
    return QSize(200,32);
}

//--------------------------------------------------------------------------
QSize WaveformBar::minimumSizeHint() const
{
    return QSize(40+2*pimpl->margin(),16);
}

//--------------------------------------------------------------------------
void WaveformBar::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing,true);

    const auto margin=pimpl->margin();
    const auto inner=static_cast<qreal>(std::max(1,width()-2*margin));
    const auto crop=pimpl->croppable;

    auto opacityAt=[this,crop](qreal fraction)
    {
        return (crop && (fraction<pimpl->cropStart || fraction>pimpl->cropEnd))?pimpl->outsideOpacity:1.0;
    };

    if (pimpl->style==Style::Bars)
    {
        const auto count=visibleBarCount();
        const auto values=pimpl->waveform.size();
        const auto step=inner/static_cast<qreal>(count);
        const auto full=static_cast<qreal>(std::max(1,height()-2*VerticalPadding));
        const auto minimum=static_cast<qreal>(std::min(pimpl->minBarHeight,height()));

        painter.setPen(Qt::NoPen);
        for (int i=0;i<count;i++)
        {
            // Merge the values this bar stands for by their maximum, so a narrow widget keeps
            // the peaks of the shape. Never an empty range: with fewer values than bars each
            // bar still gets one.
            qreal level=0.0;
            if (values>0)
            {
                const auto begin=static_cast<qsizetype>((static_cast<qint64>(i)*values)/count);
                auto end=static_cast<qsizetype>((static_cast<qint64>(i+1)*values)/count);
                if (end<=begin)
                {
                    end=begin+1;
                }
                int peak=0;
                for (auto j=begin;j<end && j<values;j++)
                {
                    peak=std::max(peak,static_cast<int>(static_cast<quint8>(pimpl->waveform.at(j))));
                }
                level=static_cast<qreal>(peak)/255.0;
            }

            const auto barHeight=minimum+(full-minimum)*level;
            const auto barWidth=static_cast<qreal>(std::min<qreal>(pimpl->barWidth,step));
            const auto x=margin+static_cast<qreal>(i)*step+(step-barWidth)/2.0;
            const auto y=(static_cast<qreal>(height())-barHeight)/2.0;

            const auto center=(static_cast<qreal>(i)+0.5)/static_cast<qreal>(count);
            painter.setOpacity(opacityAt(center));
            painter.setBrush(center<=pimpl->progress?pimpl->progressColor:pimpl->barColor);
            painter.drawRoundedRect(QRectF(x,y,barWidth,barHeight),barWidth/2.0,barWidth/2.0);
        }
    }
    else
    {
        const auto y=(static_cast<qreal>(height())-LineTrackHeight)/2.0;
        const auto startX=static_cast<qreal>(margin);
        const auto progressX=startX+inner*pimpl->progress;

        painter.setPen(Qt::NoPen);
        painter.setOpacity(1.0);
        painter.setBrush(pimpl->barColor);
        painter.drawRoundedRect(QRectF(startX,y,inner,LineTrackHeight),LineTrackHeight/2.0,LineTrackHeight/2.0);

        if (crop)
        {
            // what lies outside the range is drawn over the track as a dimming of the whole
            // track, and the range itself redrawn at full strength
            painter.setBrush(pimpl->progressColor);
            painter.setOpacity(pimpl->outsideOpacity);
            painter.drawRoundedRect(QRectF(startX,y,inner*pimpl->progress,LineTrackHeight),LineTrackHeight/2.0,LineTrackHeight/2.0);
            painter.setOpacity(1.0);
            const auto from=startX+inner*pimpl->cropStart;
            const auto to=std::min(progressX,startX+inner*pimpl->cropEnd);
            if (to>from)
            {
                painter.drawRect(QRectF(from,y,to-from,LineTrackHeight));
            }
        }
        else
        {
            painter.setBrush(pimpl->progressColor);
            painter.drawRoundedRect(QRectF(startX,y,inner*pimpl->progress,LineTrackHeight),LineTrackHeight/2.0,LineTrackHeight/2.0);
        }

        if (pimpl->seekable)
        {
            painter.setOpacity(1.0);
            painter.setBrush(pimpl->progressColor);
            painter.drawEllipse(QPointF(progressX,static_cast<qreal>(height())/2.0),LineKnobRadius,LineKnobRadius);
        }
    }

    // A thin playhead over the waveform, so the exact position reads even between bars.
    if (pimpl->style==Style::Bars && pimpl->seekable && pimpl->progress>0.0 && pimpl->progress<1.0)
    {
        painter.setOpacity(1.0);
        painter.setBrush(pimpl->progressColor);
        painter.drawRect(QRectF(margin+inner*pimpl->progress-0.5,0.0,1.0,static_cast<qreal>(height())));
    }

    if (crop)
    {
        const auto handleHeight=static_cast<qreal>(height());
        const auto handleWidth=static_cast<qreal>(pimpl->handleWidth);
        const auto startX=margin+inner*pimpl->cropStart;
        const auto endX=margin+inner*pimpl->cropEnd;

        painter.setOpacity(1.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(pimpl->cropColor);
        painter.drawRoundedRect(QRectF(startX-handleWidth,0.0,handleWidth,handleHeight),handleWidth/2.0,handleWidth/2.0);
        painter.drawRoundedRect(QRectF(endX,0.0,handleWidth,handleHeight),handleWidth/2.0,handleWidth/2.0);
    }
}

//--------------------------------------------------------------------------
void WaveformBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button()!=Qt::LeftButton)
    {
        event->ignore();
        return;
    }

    const auto x=event->position().x();

    if (pimpl->croppable)
    {
        const auto margin=pimpl->margin();
        const auto inner=static_cast<qreal>(std::max(1,width()-2*margin));
        const auto startX=margin+inner*pimpl->cropStart;
        const auto endX=margin+inner*pimpl->cropEnd;
        const auto handleWidth=static_cast<qreal>(pimpl->handleWidth);

        const auto onStart=x>=startX-handleWidth-HandleGrabSlack && x<=startX+HandleGrabSlack;
        const auto onEnd=x>=endX-HandleGrabSlack && x<=endX+handleWidth+HandleGrabSlack;
        if (onStart || onEnd)
        {
            // when the handles are close, pick the nearer one
            if (onStart && onEnd)
            {
                pimpl->drag=(std::abs(x-startX)<=std::abs(x-endX))?WaveformBar_p::Drag::CropStart:WaveformBar_p::Drag::CropEnd;
            }
            else
            {
                pimpl->drag=onStart?WaveformBar_p::Drag::CropStart:WaveformBar_p::Drag::CropEnd;
            }
            // remember where in the handle it was grabbed, measured from the edge it moves
            pimpl->dragOffset=x-(pimpl->drag==WaveformBar_p::Drag::CropStart?startX:endX);
            event->accept();
            return;
        }
    }

    if (pimpl->seekable)
    {
        pimpl->drag=WaveformBar_p::Drag::Seek;
        mouseMoveEvent(event);
        event->accept();
        return;
    }

    event->ignore();
}

//--------------------------------------------------------------------------
void WaveformBar::mouseMoveEvent(QMouseEvent* event)
{
    const auto x=event->position().x();
    const auto fraction=fractionAtX(x);

    switch (pimpl->drag)
    {
        case WaveformBar_p::Drag::Seek:
        {
            auto target=fraction;
            if (pimpl->croppable)
            {
                target=std::min(pimpl->cropEnd,std::max(pimpl->cropStart,target));
            }
            pimpl->progress=target;
            update();
            emit seekRequested(target);
            event->accept();
            return;
        }

        case WaveformBar_p::Drag::CropStart:
        {
            const auto edge=fractionAtX(x-pimpl->dragOffset);
            const auto start=std::min(edge,pimpl->cropEnd-pimpl->minimumCropFraction);
            pimpl->cropStart=std::max<qreal>(0.0,start);
            update();
            emit cropChanged(pimpl->cropStart,pimpl->cropEnd);
            event->accept();
            return;
        }

        case WaveformBar_p::Drag::CropEnd:
        {
            const auto edge=fractionAtX(x-pimpl->dragOffset);
            const auto end=std::max(edge,pimpl->cropStart+pimpl->minimumCropFraction);
            pimpl->cropEnd=std::min<qreal>(1.0,end);
            update();
            emit cropChanged(pimpl->cropStart,pimpl->cropEnd);
            event->accept();
            return;
        }

        case WaveformBar_p::Drag::None:
            break;
    }

    // hovering: show the resize cursor over a crop handle
    if (pimpl->croppable)
    {
        const auto margin=pimpl->margin();
        const auto inner=static_cast<qreal>(std::max(1,width()-2*margin));
        const auto startX=margin+inner*pimpl->cropStart;
        const auto endX=margin+inner*pimpl->cropEnd;
        const auto handleWidth=static_cast<qreal>(pimpl->handleWidth);
        const auto overHandle=(x>=startX-handleWidth-HandleGrabSlack && x<=startX+HandleGrabSlack)
                              || (x>=endX-HandleGrabSlack && x<=endX+handleWidth+HandleGrabSlack);
        setCursor(overHandle?Qt::SizeHorCursor:(pimpl->seekable?Qt::PointingHandCursor:Qt::ArrowCursor));
    }
    QWidget::mouseMoveEvent(event);
}

//--------------------------------------------------------------------------
void WaveformBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button()!=Qt::LeftButton || pimpl->drag==WaveformBar_p::Drag::None)
    {
        event->ignore();
        return;
    }

    const auto wasSeeking=pimpl->drag==WaveformBar_p::Drag::Seek;
    pimpl->drag=WaveformBar_p::Drag::None;
    event->accept();

    if (wasSeeking)
    {
        emit seekFinished(pimpl->progress);
    }
}

//--------------------------------------------------------------------------
void WaveformBar::leaveEvent(QEvent* event)
{
    if (pimpl->drag==WaveformBar_p::Drag::None)
    {
        setCursor(pimpl->seekable?Qt::PointingHandCursor:Qt::ArrowCursor);
    }
    QWidget::leaveEvent(event);
}

//--------------------------------------------------------------------------

}
