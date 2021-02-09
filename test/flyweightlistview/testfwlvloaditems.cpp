/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/test/flyweightlistview/testfwlvloaditems.cpp
*
*  Test loading items into FlyweightListView.
*
*/

/****************************************************************************/

#include <boost/test/unit_test.hpp>

#include <uise/test/uise-testthread.hpp>

#include <uise/desktop/utils/singleshottimer.hpp>
#include <uise/desktop/utils/destroywidget.hpp>

#include "fwlvtestwidget.hpp"

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

BOOST_AUTO_TEST_SUITE(TestFlyWeightListView)

namespace {

struct Context
{
    FwlvTestWidget* testWidget=nullptr;
    FlwListType* view=nullptr;
    QMainWindow* mainWindow=nullptr;

    Qt::Orientation orientation=Qt::Vertical;
    Direction stickMode=Direction::END;
    bool flyweightMode=true;

    size_t initialWidth=1000;
    size_t initialHeight=800;

    bool isHorizontal() const noexcept
    {
        return orientation==Qt::Horizontal;
    }
};

static void endTestCase(
        Context* ctx
    )
{
    destroyWidget(ctx->mainWindow);
    delete ctx;
    TestThread::instance()->continueTest();
}

void initWidgets(Context* ctx, std::function<void ()> handler)
{
    ctx->mainWindow=new QMainWindow();
    ctx->testWidget=new FwlvTestWidget();
    ctx->view=ctx->testWidget->pimpl->view;
    ctx->mainWindow->setCentralWidget(ctx->testWidget);
    ctx->mainWindow->show();
    ctx->mainWindow->resize(ctx->initialWidth,ctx->initialHeight);

    ctx->testWidget->pimpl->stickMode->setCurrentIndex(ctx->testWidget->pimpl->stickMode->findData(static_cast<int>(ctx->stickMode)));
    ctx->testWidget->pimpl->orientationButton->setChecked(ctx->isHorizontal());
    ctx->testWidget->pimpl->flyweightButton->setChecked(!ctx->flyweightMode);

    QTimer::singleShot(0,ctx->mainWindow,handler);
}

void doChecks(Context* ctx, size_t actualCount)
{
    UISE_TEST_CHECK_EQUAL(ctx->view->itemCount(),actualCount);

    const auto* firstItem=ctx->view->firstItem();
    UISE_TEST_REQUIRE(firstItem!=nullptr);
    const auto* firstViewportItem=ctx->view->firstViewportItem();
    UISE_TEST_REQUIRE(firstViewportItem!=nullptr);

    const auto* lastItem=ctx->view->lastItem();
    UISE_TEST_REQUIRE(lastItem!=nullptr);
    const auto* lastViewportItem=ctx->view->lastViewportItem();
    UISE_TEST_REQUIRE(lastViewportItem!=nullptr);

    UISE_TEST_CHECK(ctx->view->hasItem(firstItem->id()));
    UISE_TEST_CHECK(ctx->view->hasItem(lastItem->id()));
    UISE_TEST_CHECK(ctx->view->item(firstItem->id())==firstItem);
    UISE_TEST_CHECK(ctx->view->item(lastItem->id())==lastItem);

    auto frontID=ctx->testWidget->pimpl->items.begin()->second;
    auto backID=ctx->testWidget->pimpl->items.rbegin()->second;

    if (ctx->stickMode==Direction::END)
    {
        UISE_TEST_CHECK(firstItem!=firstViewportItem);
        UISE_TEST_CHECK(lastItem==lastViewportItem);
        UISE_TEST_CHECK_EQUAL(firstItem->id(),backID+actualCount-1);
        UISE_TEST_CHECK_EQUAL(lastItem->id(),backID);

        if (ctx->isHorizontal())
        {
            UISE_TEST_CHECK_EQUAL(ctx->view->visibleItemCount(),4);
            UISE_TEST_CHECK_EQUAL(firstViewportItem->id(),backID+ctx->view->visibleItemCount()-1);
        }
        else
        {
            UISE_TEST_CHECK_EQUAL(ctx->view->visibleItemCount(),5);
            UISE_TEST_CHECK_EQUAL(firstViewportItem->id(),backID+ctx->view->visibleItemCount()-1);
        }

        UISE_TEST_CHECK(ctx->view->isScrollAtEdge(Direction::END));
        UISE_TEST_CHECK(!ctx->view->isScrollAtEdge(Direction::HOME));
    }
    else
    {
        UISE_TEST_CHECK(firstItem==firstViewportItem);
        UISE_TEST_CHECK(lastItem!=lastViewportItem);
        UISE_TEST_CHECK_EQUAL(firstItem->id(),frontID);
        UISE_TEST_CHECK_EQUAL(lastItem->id(),frontID-actualCount+1);

        if (ctx->isHorizontal())
        {
            UISE_TEST_CHECK_EQUAL(ctx->view->visibleItemCount(),4);
            UISE_TEST_CHECK_EQUAL(lastViewportItem->id(),frontID-ctx->view->visibleItemCount()+1);
        }
        else
        {
            UISE_TEST_CHECK_EQUAL(ctx->view->visibleItemCount(),5);
            UISE_TEST_CHECK_EQUAL(lastViewportItem->id(),frontID-ctx->view->visibleItemCount()+1);
        }

        UISE_TEST_CHECK(!ctx->view->isScrollAtEdge(Direction::END));
        UISE_TEST_CHECK(ctx->view->isScrollAtEdge(Direction::HOME));
    }
}

void checkLoadItems(Context* ctx, bool clear);

void checkClearItems(Context* ctx)
{
    ctx->testWidget->pimpl->clearButton->click();

    QTimer::singleShot(10,ctx->mainWindow,
    [ctx]()
    {
        checkLoadItems(ctx,false);
    });
}

void checkLoadItems(Context* ctx, bool clear)
{
    UISE_TEST_CHECK(!ctx->view->horizontalScrollBar()->isVisible());
    UISE_TEST_CHECK(!ctx->view->verticalScrollBar()->isVisible());
    UISE_TEST_CHECK_EQUAL(ctx->view->itemCount(),0);
    UISE_TEST_CHECK_EQUAL(ctx->view->visibleItemCount(),0);
    const auto* firstItem=ctx->view->firstItem();
    UISE_TEST_CHECK(firstItem==nullptr);
    const auto* firstViewportItem=ctx->view->firstViewportItem();
    UISE_TEST_CHECK(firstViewportItem==nullptr);
    const auto* lastItem=ctx->view->lastItem();
    UISE_TEST_CHECK(lastItem==nullptr);
    const auto* lastViewportItem=ctx->view->lastViewportItem();
    UISE_TEST_CHECK(lastViewportItem==nullptr);

    ctx->testWidget->loadItems();
    doChecks(ctx,ctx->testWidget->initialItemCount);

    QTimer::singleShot(100,ctx->mainWindow,
    [ctx,clear]()
    {
        if (ctx->isHorizontal())
        {
            UISE_TEST_CHECK(ctx->view->horizontalScrollBar()->isVisible());
            UISE_TEST_CHECK(!ctx->view->verticalScrollBar()->isVisible());
        }
        else
        {
            UISE_TEST_CHECK(!ctx->view->horizontalScrollBar()->isVisible());
            UISE_TEST_CHECK(ctx->view->verticalScrollBar()->isVisible());
        }

        if (ctx->flyweightMode)
        {
            doChecks(ctx,ctx->testWidget->initialItemCount+ctx->view->prefetchItemCount());
        }
        else
        {
            doChecks(ctx,ctx->testWidget->initialItemCount);
        }

        if (clear)
        {
            checkClearItems(ctx);
        }
        else
        {
            endTestCase(ctx);
        }
    });
}

void runTest(Context* ctx)
{
    auto handler=[ctx]()
    {
        checkLoadItems(ctx,true);
    };
    TestThread::instance()->postGuiThread(
        [ctx,handler](){initWidgets(ctx,handler);}
    );
    auto ret=TestThread::instance()->execTest(15000);
    UISE_TEST_CHECK(ret);
}

}

BOOST_AUTO_TEST_CASE(TestLoadItemsVerticalEnd)
{
    auto ctx=new Context();
    runTest(ctx);
}

BOOST_AUTO_TEST_CASE(TestLoadItemsHorizontalEnd)
{
    auto ctx=new Context();
    ctx->orientation=Qt::Horizontal;
    runTest(ctx);
}

BOOST_AUTO_TEST_CASE(TestLoadItemsVerticalHome)
{
    auto ctx=new Context();
    ctx->stickMode=Direction::HOME;
    runTest(ctx);
}

BOOST_AUTO_TEST_CASE(TestLoadItemsHorizontalHome)
{
    auto ctx=new Context();
    ctx->orientation=Qt::Horizontal;
    ctx->stickMode=Direction::HOME;
    runTest(ctx);
}

BOOST_AUTO_TEST_CASE(TestLoadItemsVerticalNone)
{
    auto ctx=new Context();
    ctx->stickMode=Direction::NONE;
    runTest(ctx);
}

BOOST_AUTO_TEST_CASE(TestLoadItemsHorizontalNone)
{
    auto ctx=new Context();
    ctx->orientation=Qt::Horizontal;
    ctx->stickMode=Direction::NONE;
    runTest(ctx);
}

BOOST_AUTO_TEST_CASE(TestLoadItemsVerticalEndNoFlw)
{
    auto ctx=new Context();
    ctx->flyweightMode=false;
    runTest(ctx);
}

BOOST_AUTO_TEST_CASE(TestLoadItemsHorizontalEndNoFlw)
{
    auto ctx=new Context();
    ctx->orientation=Qt::Horizontal;
    ctx->flyweightMode=false;
    runTest(ctx);
}

BOOST_AUTO_TEST_CASE(TestLoadItemsVerticalHomeNoFlw)
{
    auto ctx=new Context();
    ctx->stickMode=Direction::HOME;
    ctx->flyweightMode=false;
    runTest(ctx);
}

BOOST_AUTO_TEST_CASE(TestLoadItemsHorizontalHomeNoFlw)
{
    auto ctx=new Context();
    ctx->orientation=Qt::Horizontal;
    ctx->stickMode=Direction::HOME;
    ctx->flyweightMode=false;
    runTest(ctx);
}

BOOST_AUTO_TEST_CASE(TestLoadItemsVerticalNoneNoFlw)
{
    auto ctx=new Context();
    ctx->stickMode=Direction::NONE;
    ctx->flyweightMode=false;
    runTest(ctx);
}

BOOST_AUTO_TEST_CASE(TestLoadItemsHorizontalNoneNoFlw)
{
    auto ctx=new Context();
    ctx->orientation=Qt::Horizontal;
    ctx->stickMode=Direction::NONE;
    ctx->flyweightMode=false;
    runTest(ctx);
}

BOOST_AUTO_TEST_SUITE_END()
