/**
@copyright Evgeny Sidorov 2026

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/test/graphicsviewzoom/testgraphicsviewzoom.cpp
*
*  Test GraphicsViewZoom.
*
*/

/****************************************************************************/

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QScrollBar>
#include <QMouseEvent>
#include <QApplication>

#include <uise/test/uise-testthread.hpp>
#include <uise/test/uise-testutils.hpp>

#include <uise/desktop/utils/graphicsviewzoom.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

// GraphicsViewZoom itself is exercised directly through its public API (zoomBy()/zoomTo()/
// zoomIn()/zoomOut()/fitToItem()/resetZoom()) plus real QMouseEvents for panning -- deliberately
// NOT via synthetic QWheelEvent/QNativeGestureEvent objects. Both of those have multi-argument,
// Qt-version-sensitive constructors this test cannot verify against a real Qt build (see the
// design notes' own confidence caveat on the native-gesture path); the wheel/gesture ROUTING in
// eventFilter() is exactly the part meant to be exercised manually via the imageviewer/imageeditor
// demos instead, once built. Everything tested here -- scale invariance under flip/rotation,
// clamping, cursor-anchored zoom, fit/reset, and the scrollbar-based pan mechanics -- is the
// harder, more bug-prone math underneath that routing, and is fully covered without synthesizing
// either event type.
using ViewContainer=TestWidgetContainer<QGraphicsView>;
using ViewContainerPtr=std::shared_ptr<ViewContainer>;

BOOST_AUTO_TEST_SUITE(TestGraphicsViewZoom)

BOOST_AUTO_TEST_CASE(TestScaleInvariance)
{
    auto init=[](ViewContainerPtr container){
        auto view=new QGraphicsView();
        view->setScene(new QGraphicsScene(view));
        ViewContainer::beginTestCase(container,view,"Test GraphicsViewZoom scale invariance");
    };

    auto checkInvariance=[](ViewContainerPtr container){
        auto view=container->testWidget;
        GraphicsViewZoom zoom(view);

        auto t=view->transform();
        t.scale(2.5,2.5);
        view->setTransform(t);
        UISE_TEST_CHECK(qAbs(zoom.currentScale()-2.5)<1e-9);

        // Flip horizontally: m11 goes negative -- the magnitude must be unaffected.
        t=view->transform();
        t.scale(-1,1);
        view->setTransform(t);
        UISE_TEST_CHECK(qAbs(zoom.currentScale()-2.5)<1e-9);

        // 90-degree rotation: m11 becomes (near) 0, m12 carries the magnitude instead.
        view->resetTransform();
        t=view->transform();
        t.scale(2.5,2.5);
        t.rotate(90);
        view->setTransform(t);
        UISE_TEST_CHECK(qAbs(zoom.currentScale()-2.5)<1e-9);
    };

    std::vector<std::function<void (ViewContainerPtr container)>> steps={init,checkInvariance};
    ViewContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestClamping)
{
    auto init=[](ViewContainerPtr container){
        auto view=new QGraphicsView();
        auto scene=new QGraphicsScene(view);
        view->setScene(scene);
        scene->addRect(0,0,100,80);
        ViewContainer::beginTestCase(container,view,"Test GraphicsViewZoom clamping");
    };

    auto checkClamping=[](ViewContainerPtr container){
        auto view=container->testWidget;
        auto items=view->scene()->items();
        UISE_TEST_REQUIRE(!items.isEmpty());

        GraphicsViewZoom zoom(view);
        zoom.setFitItem(items.first());

        for (int i=0;i<50;i++)
        {
            zoom.zoomIn();
        }
        UISE_TEST_CHECK(qAbs(zoom.zoomFactor()-GraphicsViewZoom::DefaultMaxZoomFactor)<0.01);

        for (int i=0;i<80;i++)
        {
            zoom.zoomOut();
        }
        UISE_TEST_CHECK(qAbs(zoom.zoomFactor()-GraphicsViewZoom::DefaultMinZoomFactor)<0.01);
    };

    std::vector<std::function<void (ViewContainerPtr container)>> steps={init,checkClamping};
    ViewContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestMinDisplayPixels)
{
    // Test container is 800x600 (TestWidgetContainer's default) -- large enough that a
    // 2000x1600 item's fit scale is comfortably below 1.0, giving zoomOut() real room to descend
    // to the pixel floor before hitting DefaultMinZoomFactor.
    auto init=[](ViewContainerPtr container){
        auto view=new QGraphicsView();
        auto scene=new QGraphicsScene(view);
        view->setScene(scene);
        scene->addRect(0,0,2000,1600);
        ViewContainer::beginTestCase(container,view,"Test GraphicsViewZoom minDisplayPixels floor");
    };

    auto checkFloor=[](ViewContainerPtr container){
        auto view=container->testWidget;
        auto items=view->scene()->items();
        UISE_TEST_REQUIRE(!items.isEmpty());

        GraphicsViewZoom zoom(view);
        zoom.setFitItem(items.first());
        zoom.setMinDisplayPixels(200);
        zoom.fitToItem();

        for (int i=0;i<40;i++)
        {
            zoom.zoomOut();
        }

        // 2000x1600 mapped at fit, then shrunk further to floorScale=200/1600 -- comfortably below
        // baselineScale() (fit), so isUserZoomed() must be true even though isZoomed() (which only
        // ever looks ABOVE 1.0) stays false.
        auto expected=200.0/1600.0;
        UISE_TEST_CHECK(qAbs(zoom.currentScale()-expected)<0.01);
        UISE_TEST_CHECK(zoom.zoomFactor()<1.0);
        UISE_TEST_CHECK(zoom.isUserZoomed());
        UISE_TEST_CHECK(!zoom.isZoomed());
    };

    std::vector<std::function<void (ViewContainerPtr container)>> steps={init,checkFloor};
    ViewContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestMinDisplayPixelsDisabledByDefault)
{
    auto init=[](ViewContainerPtr container){
        auto view=new QGraphicsView();
        auto scene=new QGraphicsScene(view);
        view->setScene(scene);
        scene->addRect(0,0,2000,1600);
        ViewContainer::beginTestCase(container,view,"Test GraphicsViewZoom minDisplayPixels opt-in");
    };

    auto checkDisabled=[](ViewContainerPtr container){
        auto view=container->testWidget;
        auto items=view->scene()->items();
        UISE_TEST_REQUIRE(!items.isEmpty());

        // minDisplayPixels() defaults to 0 (disabled) -- zoomOut() must still bottom out at fit,
        // exactly as TestClamping already verifies, proving the floor above is really opt-in.
        GraphicsViewZoom zoom(view);
        zoom.setFitItem(items.first());
        zoom.fitToItem();

        for (int i=0;i<40;i++)
        {
            zoom.zoomOut();
        }

        UISE_TEST_CHECK(qAbs(zoom.zoomFactor()-1.0)<0.01);
        UISE_TEST_CHECK(!zoom.isUserZoomed());
    };

    std::vector<std::function<void (ViewContainerPtr container)>> steps={init,checkDisabled};
    ViewContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestMinDisplayPixelsCappedAtFit)
{
    // A 100x80 item is already smaller than minDisplayPixels(100) at fit (fit scale is likely >1
    // capped to 1.0 by fitOnlyIfLarger, giving a natural on-screen size of exactly 100x80) -- the
    // floor must never force a zoom IN to reach 100px, so zoomOut() should be unable to go below fit.
    auto init=[](ViewContainerPtr container){
        auto view=new QGraphicsView();
        auto scene=new QGraphicsScene(view);
        view->setScene(scene);
        scene->addRect(0,0,100,80);
        ViewContainer::beginTestCase(container,view,"Test GraphicsViewZoom minDisplayPixels fit cap");
    };

    auto checkCap=[](ViewContainerPtr container){
        auto view=container->testWidget;
        auto items=view->scene()->items();
        UISE_TEST_REQUIRE(!items.isEmpty());

        GraphicsViewZoom zoom(view);
        zoom.setFitItem(items.first());
        zoom.setMinDisplayPixels(100);
        zoom.fitToItem();

        for (int i=0;i<40;i++)
        {
            zoom.zoomOut();
        }

        UISE_TEST_CHECK(qAbs(zoom.zoomFactor()-1.0)<0.01);
    };

    std::vector<std::function<void (ViewContainerPtr container)>> steps={init,checkCap};
    ViewContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestAnchoring)
{
    auto init=[](ViewContainerPtr container){
        auto view=new QGraphicsView();
        auto scene=new QGraphicsScene(view);
        view->setScene(scene);
        scene->addRect(0,0,400,300);
        ViewContainer::beginTestCase(container,view,"Test GraphicsViewZoom anchoring");
    };

    auto checkAnchoring=[](ViewContainerPtr container){
        auto view=container->testWidget;
        GraphicsViewZoom zoom(view);

        // A point away from the viewport centre -- if zoomTo() anchored at the centre instead
        // (which is what a plain view->scale() call, with no anchoring logic at all, would do),
        // this point would visibly drift, and the check below would fail.
        auto anchor=view->viewport()->rect().center()+QPoint(40,-25);
        auto sceneAnchorBefore=view->mapToScene(anchor);

        zoom.zoomTo(zoom.currentScale()*3.0,anchor);

        auto sceneAnchorAfter=view->mapToScene(anchor);
        // Integer scrollbar steps mean this cannot be bit-exact -- a couple of scene units of
        // slop (well under a screen pixel at 3x zoom) is the right tolerance here.
        UISE_TEST_CHECK(qAbs(sceneAnchorAfter.x()-sceneAnchorBefore.x())<2.0);
        UISE_TEST_CHECK(qAbs(sceneAnchorAfter.y()-sceneAnchorBefore.y())<2.0);
    };

    std::vector<std::function<void (ViewContainerPtr container)>> steps={init,checkAnchoring};
    ViewContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestResetAndFit)
{
    auto init=[](ViewContainerPtr container){
        auto view=new QGraphicsView();
        auto scene=new QGraphicsScene(view);
        view->setScene(scene);
        // Deliberately larger than the test window so fitToItem() must actually shrink it,
        // exercising the fitInView() branch rather than the unit-magnitude-normalize one.
        scene->addRect(0,0,2000,1500);
        ViewContainer::beginTestCase(container,view,"Test GraphicsViewZoom reset/fit");
    };

    auto checkResetAndFit=[](ViewContainerPtr container){
        auto view=container->testWidget;
        auto items=view->scene()->items();
        UISE_TEST_REQUIRE(!items.isEmpty());

        GraphicsViewZoom zoom(view);
        zoom.setFitItem(items.first());
        zoom.fitToItem();

        UISE_TEST_CHECK(!zoom.isZoomed());
        auto fitScale=zoom.currentScale();
        UISE_TEST_CHECK(fitScale>0.0 && fitScale<1.0);

        zoom.zoomIn();
        zoom.zoomIn();
        UISE_TEST_CHECK(zoom.isZoomed());

        zoom.resetZoom();
        UISE_TEST_CHECK(!zoom.isZoomed());
        UISE_TEST_CHECK(qAbs(zoom.currentScale()-fitScale)<0.01);
    };

    std::vector<std::function<void (ViewContainerPtr container)>> steps={init,checkResetAndFit};
    ViewContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestPanning)
{
    auto init=[](ViewContainerPtr container){
        auto view=new QGraphicsView();
        auto scene=new QGraphicsScene(view);
        view->setScene(scene);
        scene->addRect(0,0,2000,1500);
        ViewContainer::beginTestCase(container,view,"Test GraphicsViewZoom panning");
    };

    auto checkPanning=[](ViewContainerPtr container){
        auto view=container->testWidget;
        auto items=view->scene()->items();
        UISE_TEST_REQUIRE(!items.isEmpty());

        GraphicsViewZoom zoom(view);
        zoom.setFitItem(items.first());
        zoom.setZoomFactor(4.0,view->viewport()->rect().center());
        UISE_TEST_REQUIRE(zoom.isPannable());

        auto hBefore=view->horizontalScrollBar()->value();
        auto vBefore=view->verticalScrollBar()->value();

        auto start=view->viewport()->rect().center();
        auto end=start-QPoint(30,20);

        // eventFilter() never consumes mouse events (see GraphicsViewZoom's own class doc) -- it
        // only observes them via QApplication::sendEvent(), same as a real drag would arrive.
        // The (type,localPos,globalPos,button,buttons,modifiers) QMouseEvent constructor used
        // below (device defaulted) is a long-standing Qt5/6 compatibility overload; if a future
        // Qt version has dropped it, this is the one place in the suite that would need updating.
        QMouseEvent press(QEvent::MouseButtonPress,QPointF(start),QPointF(view->viewport()->mapToGlobal(start)),
                           Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(view->viewport(),&press);
        UISE_TEST_CHECK(zoom.isPanning());

        QMouseEvent move(QEvent::MouseMove,QPointF(end),QPointF(view->viewport()->mapToGlobal(end)),
                          Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(view->viewport(),&move);

        QMouseEvent release(QEvent::MouseButtonRelease,QPointF(end),QPointF(view->viewport()->mapToGlobal(end)),
                             Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(view->viewport(),&release);
        UISE_TEST_CHECK(!zoom.isPanning());

        // A 30x20 drag must have moved at least one scrollbar -- direction/exact amount depend
        // on the fitted scale, only that panning had an effect is asserted here.
        auto hAfter=view->horizontalScrollBar()->value();
        auto vAfter=view->verticalScrollBar()->value();
        UISE_TEST_CHECK(hAfter!=hBefore || vAfter!=vBefore);
    };

    std::vector<std::function<void (ViewContainerPtr container)>> steps={init,checkPanning};
    ViewContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_CASE(TestUserZoomIntent)
{
    auto init=[](ViewContainerPtr container){
        auto view=new QGraphicsView();
        auto scene=new QGraphicsScene(view);
        view->setScene(scene);
        // Larger than the test window, same as TestResetAndFit -- isZoomed() being true on a
        // never-yet-fitted identity transform is exactly the regression isUserZoomed() exists to
        // be distinguishable from (see ImageViewer::fitImage()'s own gate).
        scene->addRect(0,0,2000,1500);
        scene->addRect(0,0,1000,800);
        ViewContainer::beginTestCase(container,view,"Test GraphicsViewZoom user-zoom intent");
    };

    auto checkUserZoomIntent=[](ViewContainerPtr container){
        auto view=container->testWidget;
        auto items=view->scene()->items();
        UISE_TEST_REQUIRE(items.size()==2);

        GraphicsViewZoom zoom(view);
        zoom.setFitItem(items.at(1));

        // Never-yet-fitted, identity transform, content larger than the viewport: isZoomed() is
        // true (currentScale() 1.0 sits above a sub-1.0 baseline) but isUserZoomed() must be
        // false -- this is the regression itself.
        UISE_TEST_CHECK(zoom.isZoomed());
        UISE_TEST_CHECK(!zoom.isUserZoomed());

        zoom.fitToItem();
        UISE_TEST_CHECK(!zoom.isUserZoomed());

        zoom.zoomIn();
        UISE_TEST_CHECK(zoom.isUserZoomed());

        // Step back down to the baseline clamp -- the flag must clear again, not just isZoomed().
        for(int i=0;i<20;i++)
        {
            zoom.zoomOut();
        }
        UISE_TEST_CHECK(!zoom.isZoomed());
        UISE_TEST_CHECK(!zoom.isUserZoomed());

        // Re-fitting a genuine zoom, then upgrading to a different item (navigating to another
        // image) must drop the flag.
        zoom.zoomIn();
        zoom.zoomIn();
        UISE_TEST_CHECK(zoom.isUserZoomed());
        zoom.setFitItem(items.at(0));
        UISE_TEST_CHECK(!zoom.isUserZoomed());

        // But re-calling setFitItem() with the SAME item (a producer-driven pixmap update /
        // version-ladder upgrade landing on the currently displayed image) must NOT disturb a
        // deliberate zoom.
        zoom.zoomIn();
        zoom.zoomIn();
        UISE_TEST_CHECK(zoom.isUserZoomed());
        zoom.setFitItem(items.at(0));
        UISE_TEST_CHECK(zoom.isUserZoomed());
    };

    std::vector<std::function<void (ViewContainerPtr container)>> steps={init,checkUserZoomIntent};
    ViewContainer::runTestCase(steps);
}

BOOST_AUTO_TEST_SUITE_END()
