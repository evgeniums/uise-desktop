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

    size_t step=0;

    size_t initialWidth=1000;
    size_t initialHeight=800;

    size_t expectedVisibleItemCount=0;
    size_t expectedItemCount=0;

    size_t expectedFirstItemId=0;
    size_t expectedLastItemId=0;
    size_t expectedFirstVisibleItemId=0;
    size_t expectedLastVisibleItemId=0;

    bool expectedVisibleScrollBar=false;

    QSize itemSize() const noexcept
    {
        if (view)
        {
            auto item=view->firstItem();
            if (item)
            {
                auto widget=item->widget();
                if (widget)
                {
                    return widget->size();
                }
            }
        }

        return QSize();
    }

    size_t maybeVisibleCount() const noexcept
    {
        if (view)
        {
            auto isize=itemSize();
            if (isize.isValid() && !isize.isNull())
            {
                if (view->isHorizontal())
                {
                    return ceil(static_cast<float>(view->width())/isize.width());
                }
                else
                {
                    return ceil(static_cast<float>(view->height())/isize.height());
                }
            }
        }

        return 0;
    }

    void fillExpectedIds(size_t frontID, size_t backID, int visibleIdOffset=0) noexcept
    {
        if (stickMode==Direction::END)
        {
            expectedFirstItemId=backID+expectedItemCount-1;
            expectedLastItemId=backID;

            expectedLastVisibleItemId=backID+visibleIdOffset;
            expectedFirstVisibleItemId=expectedLastVisibleItemId+expectedVisibleItemCount-1;
        }
        else
        {
            expectedFirstItemId=frontID;
            expectedLastItemId=frontID-expectedItemCount+1;

            expectedFirstVisibleItemId=frontID+visibleIdOffset;
            expectedLastVisibleItemId=expectedFirstVisibleItemId-expectedVisibleItemCount+1;
        }
    }

    void fillExpectedAfterLoad() noexcept
    {
        expectedVisibleItemCount=maybeVisibleCount();

        auto frontID=testWidget->pimpl->items.begin()->second;
        auto backID=testWidget->pimpl->items.rbegin()->second;
        if (flyweightMode)
        {
            expectedItemCount=testWidget->initialItemCount+view->prefetchItemCount();
        }
        else
        {
            expectedItemCount=testWidget->initialItemCount;
        }
        expectedVisibleScrollBar=true;
        fillExpectedIds(frontID,backID);
    }

    void doChecks(bool withCsrollbars=true)
    {
        UISE_TEST_CHECK_EQUAL(view->itemCount(),expectedItemCount);

        const auto* firstItem=view->firstItem();
        UISE_TEST_REQUIRE(firstItem!=nullptr);
        const auto* firstViewportItem=view->firstViewportItem();
        UISE_TEST_REQUIRE(firstViewportItem!=nullptr);

        const auto* lastItem=view->lastItem();
        UISE_TEST_REQUIRE(lastItem!=nullptr);
        const auto* lastViewportItem=view->lastViewportItem();
        UISE_TEST_REQUIRE(lastViewportItem!=nullptr);

        UISE_TEST_CHECK(view->hasItem(firstItem->id()));
        UISE_TEST_CHECK(view->hasItem(lastItem->id()));
        UISE_TEST_CHECK(view->item(firstItem->id())==firstItem);
        UISE_TEST_CHECK(view->item(lastItem->id())==lastItem);

        UISE_TEST_CHECK_EQUAL(firstItem->id(),expectedFirstItemId);
        UISE_TEST_CHECK_EQUAL(lastItem->id(),expectedLastItemId);
        UISE_TEST_CHECK_EQUAL(firstViewportItem->id(),expectedFirstVisibleItemId);
        UISE_TEST_CHECK_EQUAL(lastViewportItem->id(),expectedLastVisibleItemId);
        UISE_TEST_CHECK_EQUAL(view->visibleItemCount(),expectedVisibleItemCount);

        if (stickMode==Direction::END)
        {
            UISE_TEST_CHECK(view->isScrollAtEdge(Direction::END));
            UISE_TEST_CHECK(!view->isScrollAtEdge(Direction::HOME));
        }
        else
        {
            UISE_TEST_CHECK(!view->isScrollAtEdge(Direction::END));
            UISE_TEST_CHECK(view->isScrollAtEdge(Direction::HOME));
        }

        if (withCsrollbars)
        {
            if (expectedVisibleScrollBar)
            {
                auto isize=itemSize();
                if (isHorizontal())
                {
                    UISE_TEST_CHECK(view->horizontalScrollBar()->isVisible());

                    if (isize.isValid())
                    {
                        if (isize.height()<=view->viewportSize().height())
                        {
                            UISE_TEST_CHECK(!view->verticalScrollBar()->isVisible());
                        }
                        else
                        {
                            UISE_TEST_CHECK(view->verticalScrollBar()->isVisible());
                        }
                    }
                }
                else
                {
                    if (isize.isValid())
                    {
                        if (isize.width()<=view->viewportSize().width())
                        {
                            UISE_TEST_CHECK(!view->horizontalScrollBar()->isVisible());
                        }
                        else
                        {
                            UISE_TEST_CHECK(view->horizontalScrollBar()->isVisible());
                        }
                    }

                    UISE_TEST_CHECK(view->verticalScrollBar()->isVisible());
                }
            }
            else
            {
                UISE_TEST_CHECK(!view->horizontalScrollBar()->isVisible());
                UISE_TEST_CHECK(!view->verticalScrollBar()->isVisible());
            }
        }
    }

    bool isHorizontal() const noexcept
    {
        return orientation==Qt::Horizontal;
    }

    void endTestCase()
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

    static void execSingleMode(std::function<void (FwlvTestContext* ctx)> handler,Qt::Orientation orientation, Direction stickMode, bool flyweightMode)
    {
        auto ctx=new FwlvTestContext();
        ctx->orientation=orientation;
        ctx->stickMode=stickMode;
        ctx->flyweightMode=flyweightMode;
        ctx->runTest(handler);
    }

    static void execAllModes(std::function<void (FwlvTestContext* ctx)> handler)
    {
        UISE_TEST_CONTEXT("VerticalStickEndFlyweight") {execSingleMode(handler,Qt::Vertical,Direction::END,true);}
        UISE_TEST_CONTEXT("HorizontalStickEndFlyweight") {execSingleMode(handler,Qt::Horizontal,Direction::END,true);}
        UISE_TEST_CONTEXT("VerticalStickHomeFlyweight") {execSingleMode(handler,Qt::Vertical,Direction::HOME,true);}
        UISE_TEST_CONTEXT("HorizontalStickHomeFlyweight") {execSingleMode(handler,Qt::Horizontal,Direction::HOME,true);}
        UISE_TEST_CONTEXT("VerticalStickNoneFlyweight") {execSingleMode(handler,Qt::Vertical,Direction::NONE,true);}
        UISE_TEST_CONTEXT("HorizontalStickNoneFlyweight") {execSingleMode(handler,Qt::Horizontal,Direction::NONE,true);}

        UISE_TEST_CONTEXT("VerticalStickEndNoFlyweight") {execSingleMode(handler,Qt::Vertical,Direction::END,false);}
        UISE_TEST_CONTEXT("HorizontalStickEndNoFlyweight") {execSingleMode(handler,Qt::Horizontal,Direction::END,false);}
        UISE_TEST_CONTEXT("VerticalStickHomeNoFlyweight") {execSingleMode(handler,Qt::Vertical,Direction::HOME,false);}
        UISE_TEST_CONTEXT("HorizontalStickHomeNoFlyweight") {execSingleMode(handler,Qt::Horizontal,Direction::HOME,false);}
        UISE_TEST_CONTEXT("VerticalStickNoneNoFlyweight") {execSingleMode(handler,Qt::Vertical,Direction::NONE,false);}
        UISE_TEST_CONTEXT("HorizontalStickNoneNoFlyweight") {execSingleMode(handler,Qt::Horizontal,Direction::NONE,false);}
    }
};

UISE_TEST_NAMESPACE_END

#endif // UISE_DESKTOP_TEST_FWLVTESTCONTEXT_HPP
