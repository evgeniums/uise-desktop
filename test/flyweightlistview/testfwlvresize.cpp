/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/test/flyweightlistview/testfwlvresize.cpp
*
*  Test resizing FlyweightListView.
*
*/

/****************************************************************************/

#include <boost/test/unit_test.hpp>

#include <uise/test/uise-testthread.hpp>

#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/utils/destroywidget.hpp>

#include "fwlvtestwidget.hpp"
#include "fwlvtestcontext.hpp"

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

BOOST_AUTO_TEST_SUITE(TestFlyWeightListView)

namespace {

void visibleItemsChanged(FwlvTestContext* ctx, const HelloWorldItemWrapper* begin,const HelloWorldItemWrapper* end)
{
    ++ctx->visibleItemsChangedCount;

    ctx->fillExpectedAfterLoad();

    std::string msg=std::string("visibleItemsChanged, step ")+std::to_string(ctx->step);
    BOOST_TEST_CONTEXT(msg.c_str())
    {
        ctx->doChecks(false);

        UISE_TEST_REQUIRE(begin!=nullptr);
        UISE_TEST_REQUIRE(end!=nullptr);

        UISE_TEST_CHECK_EQUAL(begin->id(),ctx->expectedFirstVisibleItemId);
        UISE_TEST_CHECK_EQUAL(end->id(),ctx->expectedLastVisibleItemId);
    }
}

void runTest(FwlvTestContext* ctx)
{
    ctx->testWidget->loadItems();
    ++ctx->step;

    QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
    [ctx]()
    {
        ctx->view->setViewportChangedCb(
            [ctx](const HelloWorldItemWrapper* begin,const HelloWorldItemWrapper* end)
            {
                visibleItemsChanged(ctx,begin,end);
            }
        );

        QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
        [ctx]()
        {
            ctx->fillExpectedAfterLoad();
            BOOST_TEST_CONTEXT("After load with delay") {ctx->doChecks();}

            ctx->mainWindow->resize(ctx->mainWindow->width()+ctx->mainWindow->width()/4,ctx->mainWindow->height()+ctx->mainWindow->height()/4);
            ++ctx->step;

            QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
            [ctx]()
            {
                ctx->fillExpectedAfterLoad();
                BOOST_TEST_CONTEXT("After increase") {ctx->doChecks();}

                ctx->mainWindow->resize(ctx->mainWindow->width()-ctx->initialWidth/2,ctx->mainWindow->height()-ctx->initialHeight/2);
                ++ctx->step;

                QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
                [ctx]()
                {
                    ctx->fillExpectedAfterLoad();
                    BOOST_TEST_CONTEXT("After decrease") {ctx->doChecks();}

                    UISE_TEST_CHECK_GE(ctx->visibleItemsChangedCount,2);

                    ctx->endTestCase();
               });
            });
        });
    });
}

}

BOOST_AUTO_TEST_CASE(TestResize)
{
    auto handler=[](FwlvTestContext* ctx)
    {
        runTest(ctx);
    };
    FwlvTestContext::execAllModes(handler);
}

BOOST_AUTO_TEST_SUITE_END()
