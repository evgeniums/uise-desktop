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

/** @file uise/desktop/test/waveformbar/testwaveformbar.cpp
*
*  Tests of WaveformBar: seeking, cropping, bar merging and painting.
*
*/

/****************************************************************************/

#include <cmath>
#include <memory>
#include <vector>

#include <QApplication>
#include <QImage>
#include <QMouseEvent>
#include <QPalette>

#include <uise/test/uise-testthread.hpp>
#include <uise/test/uise-testutils.hpp>
#include <uise/desktop/waveformbar.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

using BarContainer=TestWidgetContainer<WaveformBar>;
using BarContainerPtr=std::shared_ptr<BarContainer>;
using Step=std::function<void (BarContainerPtr)>;

namespace {

constexpr int BarWidthPx=300;
constexpr int BarHeightPx=32;

//! Everything the widget told the test, kept across steps.
struct Recorder
{
    std::vector<qreal> seeks;
    std::vector<qreal> seekFinishes;
    std::vector<std::pair<qreal,qreal>> crops;

    void attach(WaveformBar* bar)
    {
        QObject::connect(bar,&WaveformBar::seekRequested,bar,[this](qreal f){seeks.push_back(f);});
        QObject::connect(bar,&WaveformBar::seekFinished,bar,[this](qreal f){seekFinishes.push_back(f);});
        QObject::connect(bar,&WaveformBar::cropChanged,bar,[this](qreal s,qreal e){crops.emplace_back(s,e);});
    }

    void clear()
    {
        seeks.clear();
        seekFinishes.clear();
        crops.clear();
    }
};

bool near(qreal a, qreal b, qreal eps=1e-6)
{
    return std::abs(a-b)<eps;
}

//! Send a left-button mouse event at local x (vertical middle) straight to the widget.
bool sendMouse(WaveformBar* bar, QEvent::Type type, qreal x)
{
    const QPointF local(x,BarHeightPx/2.0);
    const auto global=QPointF(bar->mapToGlobal(local.toPoint()));
    const auto button=(type==QEvent::MouseMove)?Qt::NoButton:Qt::LeftButton;
    const auto buttons=(type==QEvent::MouseButtonRelease)?Qt::NoButton:Qt::LeftButton;
    QMouseEvent event(type,local,global,button,buttons,Qt::NoModifier);
    QApplication::sendEvent(bar,&event);
    return event.isAccepted();
}

bool press(WaveformBar* bar, qreal x)
{
    return sendMouse(bar,QEvent::MouseButtonPress,x);
}

void move(WaveformBar* bar, qreal x)
{
    sendMouse(bar,QEvent::MouseMove,x);
}

void release(WaveformBar* bar, qreal x)
{
    sendMouse(bar,QEvent::MouseButtonRelease,x);
}

//! A waveform of `count` values, all `value`.
QByteArray flatWaveform(int count, quint8 value)
{
    return QByteArray(count,static_cast<char>(value));
}

//! Create the bar at a fixed, deterministic size and show it.
Step makeInit(const QString& name, std::shared_ptr<Recorder> recorder)
{
    return [name,recorder](BarContainerPtr container)
    {
        auto* bar=new WaveformBar();
        bar->setFixedSize(BarWidthPx,BarHeightPx);
        recorder->attach(bar);
        BarContainer::beginTestCase(container,bar,name);
    };
}

//! The colour of the pixel at logical (x,y) of a grab of the widget.
QColor pixelAt(const QImage& image, int x, int y)
{
    const auto ratio=image.devicePixelRatio();
    return image.pixelColor(static_cast<int>(x*ratio),static_cast<int>(y*ratio));
}

//! Grab on an opaque black background, so a bar shows as a colour and a gap as black.
QImage grabOnBlack(WaveformBar* bar)
{
    QPalette palette=bar->palette();
    palette.setColor(QPalette::Window,Qt::black);
    bar->setPalette(palette);
    bar->setAutoFillBackground(true);
    return bar->grab().toImage();
}

//! Centre x of bar `index` of `count`, for a bar with `margin` either side.
qreal barCenterX(const WaveformBar* bar, int index, int count, int margin)
{
    const auto inner=static_cast<qreal>(bar->width()-2*margin);
    return margin+(static_cast<qreal>(index)+0.5)*(inner/static_cast<qreal>(count));
}

}

BOOST_AUTO_TEST_SUITE(TestWaveformBar)

//--------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestSeekGesture)
{
    auto rec=std::make_shared<Recorder>();

    auto pressAndDrag=[rec](BarContainerPtr container)
    {
        auto* bar=container->testWidget;
        UISE_TEST_CHECK_EQUAL(bar->width(),BarWidthPx);
        UISE_TEST_CHECK(!bar->isSeeking());

        UISE_TEST_CHECK(press(bar,150));
        UISE_TEST_CHECK(bar->isSeeking());
        UISE_TEST_REQUIRE_EQUAL(rec->seeks.size(),static_cast<size_t>(1));
        UISE_TEST_CHECK(near(rec->seeks.back(),bar->fractionAtX(150)));
        UISE_TEST_CHECK(near(rec->seeks.back(),0.5));

        move(bar,225);
        UISE_TEST_REQUIRE_EQUAL(rec->seeks.size(),static_cast<size_t>(2));
        UISE_TEST_CHECK(near(rec->seeks.back(),0.75));
        UISE_TEST_CHECK(near(bar->progress(),0.75));

        // the finger drives the bar, not the playback position the host keeps pushing in
        bar->setProgress(0.1);
        UISE_TEST_CHECK(near(bar->progress(),0.75));

        release(bar,225);
        UISE_TEST_CHECK(!bar->isSeeking());
        UISE_TEST_REQUIRE_EQUAL(rec->seekFinishes.size(),static_cast<size_t>(1));
        UISE_TEST_CHECK(near(rec->seekFinishes.back(),0.75));

        // ... and afterwards playback drives it again
        bar->setProgress(0.1);
        UISE_TEST_CHECK(near(bar->progress(),0.1));
    };

    auto clamping=[rec](BarContainerPtr container)
    {
        auto* bar=container->testWidget;
        rec->clear();

        press(bar,-20);
        UISE_TEST_REQUIRE_EQUAL(rec->seeks.size(),static_cast<size_t>(1));
        UISE_TEST_CHECK(near(rec->seeks.back(),0.0));
        move(bar,5000);
        UISE_TEST_CHECK(near(rec->seeks.back(),1.0));
        release(bar,5000);
        UISE_TEST_CHECK(near(rec->seekFinishes.back(),1.0));

        // a move with no button held is only a hover: it never seeks
        rec->clear();
        move(bar,100);
        UISE_TEST_CHECK(rec->seeks.empty());
    };

    auto notSeekable=[rec](BarContainerPtr container)
    {
        auto* bar=container->testWidget;
        rec->clear();

        bar->setSeekable(false);
        UISE_TEST_CHECK(!press(bar,100));
        UISE_TEST_CHECK(!bar->isSeeking());
        UISE_TEST_CHECK(rec->seeks.empty());

        // turning seeking off in the middle of a gesture must not leave it latched
        bar->setSeekable(true);
        press(bar,100);
        UISE_TEST_CHECK(bar->isSeeking());
        bar->setSeekable(false);
        UISE_TEST_CHECK(!bar->isSeeking());
        bar->setSeekable(true);
        release(bar,100);
    };

    Step init=makeInit("WaveformBar seek gesture",rec);
    BarContainer::runTestCase({init,pressAndDrag,clamping,notSeekable});
}

//--------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestCropHandles)
{
    auto rec=std::make_shared<Recorder>();

    // Croppable: a handle's width is reserved either side, so the range maps onto
    // [handleWidth, width-handleWidth] = [6, 294], 288 px.
    auto grabStartHandle=[rec](BarContainerPtr container)
    {
        auto* bar=container->testWidget;
        bar->setCroppable(true);
        bar->setCropRange(0.2,0.8);
        UISE_TEST_CHECK(near(bar->cropStart(),0.2));
        UISE_TEST_CHECK(near(bar->cropEnd(),0.8));

        // the start edge is at 6+288*0.2 = 63.6 and its handle occupies [57.6, 63.6]

        // grabbing the handle where it is and not moving must not move it
        UISE_TEST_CHECK(press(bar,60));
        UISE_TEST_CHECK(!bar->isSeeking());   // a handle grab is not a seek
        move(bar,60);
        UISE_TEST_CHECK(near(bar->cropStart(),0.2));

        // the edge follows the pointer by the same distance, offset included
        move(bar,60+(288*0.2));
        UISE_TEST_CHECK(near(bar->cropStart(),0.4));
        UISE_TEST_CHECK(near(bar->cropEnd(),0.8));
        UISE_TEST_REQUIRE_GE(rec->crops.size(),static_cast<size_t>(1));
        UISE_TEST_CHECK(near(rec->crops.back().first,0.4));
        UISE_TEST_CHECK(near(rec->crops.back().second,0.8));
        UISE_TEST_CHECK(rec->seeks.empty());

        // the handles cannot cross: dragged far right, the start stops a minimum gap short of the end
        move(bar,5000);
        UISE_TEST_CHECK(near(bar->cropStart(),0.8-bar->minimumCropFraction()));
        UISE_TEST_CHECK(near(bar->cropEnd(),0.8));
        release(bar,5000);

        // and far left it stops at the start of the audio
        press(bar,6+288*bar->cropStart()-3);
        move(bar,-5000);
        UISE_TEST_CHECK(near(bar->cropStart(),0.0));
        release(bar,-5000);
    };

    auto grabEndHandle=[rec](BarContainerPtr container)
    {
        auto* bar=container->testWidget;
        rec->clear();
        bar->setCropRange(0.2,0.8);

        const qreal endEdge=6+288*0.8;   // 236.4, the handle occupies [236.4, 242.4]
        UISE_TEST_CHECK(press(bar,endEdge+2));
        move(bar,-5000);
        UISE_TEST_CHECK(near(bar->cropEnd(),0.2+bar->minimumCropFraction()));
        UISE_TEST_CHECK(near(bar->cropStart(),0.2));
        move(bar,5000);
        UISE_TEST_CHECK(near(bar->cropEnd(),1.0));
        release(bar,5000);
        UISE_TEST_CHECK(rec->seekFinishes.empty());
    };

    auto seekInsideRange=[rec](BarContainerPtr container)
    {
        auto* bar=container->testWidget;
        rec->clear();
        bar->setCropRange(0.25,0.75);

        // a press away from the handles still seeks, and is confined to the range
        press(bar,6+288*0.5);
        UISE_TEST_REQUIRE_EQUAL(rec->seeks.size(),static_cast<size_t>(1));
        UISE_TEST_CHECK(near(rec->seeks.back(),0.5));
        move(bar,20);
        UISE_TEST_CHECK(near(rec->seeks.back(),0.25));
        move(bar,290);
        UISE_TEST_CHECK(near(rec->seeks.back(),0.75));
        release(bar,290);
        UISE_TEST_CHECK(near(rec->seekFinishes.back(),0.75));
    };

    auto nearerHandleWins=[rec](BarContainerPtr container)
    {
        auto* bar=container->testWidget;
        rec->clear();
        bar->setMinimumCropFraction(0.0);
        bar->setCropRange(0.5,0.51);   // edges at 150.0 and 152.88: the grab zones overlap

        // 151 is nearer the start edge
        press(bar,151);
        move(bar,151-30);
        UISE_TEST_CHECK(bar->cropStart()<0.5);
        UISE_TEST_CHECK(near(bar->cropEnd(),0.51));
        release(bar,121);

        bar->setCropRange(0.5,0.51);
        rec->clear();

        // 152.5 is nearer the end edge
        press(bar,152.5);
        move(bar,152.5+30);
        UISE_TEST_CHECK(bar->cropEnd()>0.51);
        UISE_TEST_CHECK(near(bar->cropStart(),0.5));
        release(bar,182.5);
    };

    auto minimumFractionReclamps=[](BarContainerPtr container)
    {
        auto* bar=container->testWidget;
        bar->setMinimumCropFraction(0.05);
        bar->setCropRange(0.4,0.42);
        UISE_TEST_CHECK(near(bar->cropEnd()-bar->cropStart(),0.05));

        // widening the minimum widens a range that is now too narrow
        bar->setMinimumCropFraction(0.3);
        UISE_TEST_CHECK(bar->cropEnd()-bar->cropStart()>=0.3-1e-9);

        // a range set at the very end stays inside 0..1
        bar->setCropRange(0.99,1.0);
        UISE_TEST_CHECK(bar->cropEnd()<=1.0);
        UISE_TEST_CHECK(bar->cropStart()>=0.0);
        UISE_TEST_CHECK(bar->cropEnd()-bar->cropStart()>=0.3-1e-9);
    };

    auto uncroppingMidDrag=[rec](BarContainerPtr container)
    {
        auto* bar=container->testWidget;
        rec->clear();
        bar->setMinimumCropFraction(0.05);
        bar->setCropRange(0.2,0.8);
        press(bar,60);
        bar->setCroppable(false);
        // the drag must be gone: moving now does not touch the range or send anything
        move(bar,200);
        UISE_TEST_CHECK(rec->crops.empty());
        release(bar,200);
    };

    Step init=makeInit("WaveformBar crop handles",rec);
    BarContainer::runTestCase({init,grabStartHandle,grabEndHandle,seekInsideRange,nearerHandleWins,
                               minimumFractionReclamps,uncroppingMidDrag});
}

//--------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestBarCount)
{
    auto rec=std::make_shared<Recorder>();

    auto counts=[](BarContainerPtr container)
    {
        auto* bar=container->testWidget;

        // 3 px bars, 2 px apart: a 300 px bar fits (300+2)/5 = 60
        bar->setWaveform(flatWaveform(100,128));
        UISE_TEST_CHECK_EQUAL(bar->visibleBarCount(),60);

        // never more than one bar per value
        bar->setWaveform(flatWaveform(30,128));
        UISE_TEST_CHECK_EQUAL(bar->visibleBarCount(),30);

        // no waveform yet: placeholder bars fill the width
        bar->setWaveform(QByteArray());
        UISE_TEST_CHECK_EQUAL(bar->visibleBarCount(),60);

        // a line has no bars
        bar->setWaveform(flatWaveform(100,128));
        bar->setStyle(WaveformBar::Style::Line);
        UISE_TEST_CHECK_EQUAL(bar->visibleBarCount(),0);
        bar->setStyle(WaveformBar::Style::Bars);

        // handles take room out of the width
        bar->setCroppable(true);
        UISE_TEST_CHECK_EQUAL(bar->visibleBarCount(),(288+2)/5);
        bar->setCroppable(false);

        // wide enough for all 100 values: exactly 100, not 160
        bar->setFixedWidth(800);
        UISE_TEST_CHECK_EQUAL(bar->visibleBarCount(),100);
        bar->setFixedWidth(BarWidthPx);
    };

    Step init=makeInit("WaveformBar bar count",rec);
    BarContainer::runTestCase({init,counts});
}

//--------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestPainting)
{
    auto rec=std::make_shared<Recorder>();

    const QColor barColor(0x33,0x99,0xcc);
    const QColor progressColor(0xcc,0x66,0x00);

    auto setup=[barColor,progressColor](BarContainerPtr container)
    {
        auto* bar=container->testWidget;
        bar->setBarColor(barColor);
        bar->setProgressColor(progressColor);
        bar->setCropColor(progressColor);
    };

    // One loud value in silence: after merging 100 values into 60 bars by MAXIMUM the spike
    // survives as a tall bar; averaging would have smeared it into nothing.
    auto peakSurvivesMerging=[](BarContainerPtr container)
    {
        auto* bar=container->testWidget;
        auto wave=flatWaveform(100,0);
        wave[50]=static_cast<char>(255);
        bar->setWaveform(wave);
        bar->setProgress(0.0);

        const auto image=grabOnBlack(bar);
        const auto count=bar->visibleBarCount();
        UISE_TEST_REQUIRE_EQUAL(count,60);

        // values [50,51) belong to bar 30 (30*100/60 = 50)
        const auto tallX=static_cast<int>(barCenterX(bar,30,count,0));
        const auto quietX=static_cast<int>(barCenterX(bar,10,count,0));

        // full height is 32-2*2 = 28 px, centred: it reaches y=3, a silent bar (3 px) does not
        UISE_TEST_CHECK(pixelAt(image,tallX,4)!=QColor(Qt::black));
        UISE_TEST_CHECK(pixelAt(image,quietX,4)==QColor(Qt::black));
        // both are drawn at the middle
        UISE_TEST_CHECK(pixelAt(image,tallX,BarHeightPx/2)!=QColor(Qt::black));
        UISE_TEST_CHECK(pixelAt(image,quietX,BarHeightPx/2)!=QColor(Qt::black));
    };

    auto progressColouring=[barColor,progressColor](BarContainerPtr container)
    {
        auto* bar=container->testWidget;
        bar->setWaveform(flatWaveform(100,255));
        bar->setProgress(0.5);

        const auto image=grabOnBlack(bar);
        const auto count=bar->visibleBarCount();
        const auto played=static_cast<int>(barCenterX(bar,10,count,0));
        const auto unplayed=static_cast<int>(barCenterX(bar,45,count,0));

        UISE_TEST_CHECK(pixelAt(image,played,BarHeightPx/2)==progressColor);
        UISE_TEST_CHECK(pixelAt(image,unplayed,BarHeightPx/2)==barColor);
    };

    auto outsideCropIsDimmed=[barColor](BarContainerPtr container)
    {
        auto* bar=container->testWidget;
        bar->setWaveform(flatWaveform(100,255));
        bar->setProgress(0.0);
        bar->setCroppable(true);
        bar->setCropRange(0.5,1.0);

        const auto image=grabOnBlack(bar);
        const auto count=bar->visibleBarCount();
        const auto outside=pixelAt(image,static_cast<int>(barCenterX(bar,10,count,6)),BarHeightPx/2);
        const auto inside=pixelAt(image,static_cast<int>(barCenterX(bar,45,count,6)),BarHeightPx/2);

        UISE_TEST_CHECK(inside==barColor);
        UISE_TEST_CHECK(outside!=barColor);
        UISE_TEST_CHECK(outside.red()<barColor.red() || outside.green()<barColor.green() || outside.blue()<barColor.blue());
        UISE_TEST_CHECK(outside!=QColor(Qt::black));   // dimmed, not gone

        // the two handles are drawn in the margins
        UISE_TEST_CHECK(pixelAt(image,3,BarHeightPx/2)!=QColor(Qt::black));
        UISE_TEST_CHECK(pixelAt(image,BarWidthPx-3,BarHeightPx/2)!=QColor(Qt::black));
    };

    auto lineStyle=[progressColor,barColor](BarContainerPtr container)
    {
        auto* bar=container->testWidget;
        bar->setCroppable(false);
        bar->setStyle(WaveformBar::Style::Line);
        bar->setProgress(0.5);
        bar->setSeekable(false);

        const auto image=grabOnBlack(bar);
        // the track is 3 px tall in the middle: played left of the position, unplayed right of it
        UISE_TEST_CHECK(pixelAt(image,60,BarHeightPx/2)==progressColor);
        UISE_TEST_CHECK(pixelAt(image,240,BarHeightPx/2)==barColor);
        // and nothing above or below it
        UISE_TEST_CHECK(pixelAt(image,60,4)==QColor(Qt::black));
    };

    Step init=makeInit("WaveformBar painting",rec);
    BarContainer::runTestCase({init,setup,peakSurvivesMerging,progressColouring,outsideCropIsDimmed,lineStyle});
}

//--------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE_END()
