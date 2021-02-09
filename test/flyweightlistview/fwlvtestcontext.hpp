/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/test/flyweightlistview/fwlvtestcontext.hpp
*
*  Helpers for testing FlyweightListView.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_TEST_FWLVTESTCONTEXT_HPP
#define UISE_DESKTOP_TEST_FWLVTESTCONTEXT_HPP

#include <boost/test/unit_test.hpp>

#include <uise/test/uise-testthread.hpp>

#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/utils/destroywidget.hpp>

#include "fwlvtestwidget.hpp"

using namespace UISE_DESKTOP_NAMESPACE;

UISE_TEST_NAMESPACE_BEGIN

struct FwlvTestContext
{
    constexpr static const int PlayStepPeriod=100;

    FwlvTestWidget* testWidget=nullptr;
    FlwListType* view=nullptr;
    QMainWindow* mainWindow=nullptr;

    Qt::Orientation orientation=Qt::Vertical;
    Direction stickMode=Direction::END;
    bool flyweightMode=true;

    size_t initialWidth=1000;
    size_t initialHeight=830;

    inline bool isHorizontal() const noexcept
    {
        return orientation==Qt::Horizontal;
    }

    inline void endTestCase()
    {
        destroyWidget(mainWindow);
        delete this;
        TestThread::instance()->continueTest();
    }

    inline void initWidgets(std::function<void ()> handler)
    {
        mainWindow=new QMainWindow();
        testWidget=new FwlvTestWidget();
        view=testWidget->pimpl->view;
        mainWindow->setCentralWidget(testWidget);
        mainWindow->show();
        mainWindow->resize(initialWidth,initialHeight);

        testWidget->pimpl->stickMode->setCurrentIndex(testWidget->pimpl->stickMode->findData(static_cast<int>(stickMode)));
        testWidget->pimpl->orientationButton->setChecked(isHorizontal());
        testWidget->pimpl->flyweightButton->setChecked(!flyweightMode);

        QTimer::singleShot(PlayStepPeriod,mainWindow,handler);
    }

    void runTest(std::function<void (FwlvTestContext* ctx)> handler)
    {
        TestThread::instance()->postGuiThread(
            [this,handler]()
            {
                initWidgets([this,handler](){handler(this);});
            }
        );
        auto ret=TestThread::instance()->execTest(15000);
        UISE_TEST_CHECK(ret);
    }

    static inline void execSingleMode(std::function<void (FwlvTestContext* ctx)> handler,Qt::Orientation orientation, Direction stickMode, bool flyweightMode)
    {
        auto ctx=new FwlvTestContext();
        ctx->orientation=orientation;
        ctx->stickMode=stickMode;
        ctx->flyweightMode=flyweightMode;
        ctx->runTest(handler);
    }

    static inline void execAllModes(std::function<void (FwlvTestContext* ctx)> handler)
    {
        BOOST_TEST_CONTEXT("VerticalStickEndFlyweight") {execSingleMode(handler,Qt::Vertical,Direction::END,true);}
        BOOST_TEST_CONTEXT("HorizontalStickEndFlyweight") {execSingleMode(handler,Qt::Horizontal,Direction::END,true);}
        BOOST_TEST_CONTEXT("VerticalStickHomeFlyweight") {execSingleMode(handler,Qt::Vertical,Direction::HOME,true);}
        BOOST_TEST_CONTEXT("HorizontalStickHomeFlyweight") {execSingleMode(handler,Qt::Horizontal,Direction::HOME,true);}
        BOOST_TEST_CONTEXT("VerticalStickNoneFlyweight") {execSingleMode(handler,Qt::Vertical,Direction::NONE,true);}
        BOOST_TEST_CONTEXT("HorizontalStickNoneFlyweight") {execSingleMode(handler,Qt::Horizontal,Direction::NONE,true);}

        BOOST_TEST_CONTEXT("VerticalStickEndNoFlyweight") {execSingleMode(handler,Qt::Vertical,Direction::END,false);}
        BOOST_TEST_CONTEXT("HorizontalStickEndNoFlyweight") {execSingleMode(handler,Qt::Horizontal,Direction::END,false);}
        BOOST_TEST_CONTEXT("VerticalStickHomeNoFlyweight") {execSingleMode(handler,Qt::Vertical,Direction::HOME,false);}
        BOOST_TEST_CONTEXT("HorizontalStickHomeNoFlyweight") {execSingleMode(handler,Qt::Horizontal,Direction::HOME,false);}
        BOOST_TEST_CONTEXT("VerticalStickNoneNoFlyweight") {execSingleMode(handler,Qt::Vertical,Direction::NONE,false);}
        BOOST_TEST_CONTEXT("HorizontalStickNoneNoFlyweight") {execSingleMode(handler,Qt::Horizontal,Direction::NONE,false);}
    }
};

UISE_TEST_NAMESPACE_END

#endif // UISE_DESKTOP_TEST_FWLVTESTCONTEXT_HPP
