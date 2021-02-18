/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/test/flyweightlistview/testfwlvjump.cpp
*
*  Test jump in FlyweightListView.
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

void delItems(FwlvTestContext* ctx, size_t firstID, size_t count)
{
    ctx->testWidget->pimpl->delFrom->setValue(firstID);
    ctx->testWidget->pimpl->delNum->setValue(count);
    ctx->testWidget->pimpl->delButton->click();
}

void delWidgets(FwlvTestContext* ctx, size_t firstID, size_t count)
{
    QStringList ids;
    for (auto id=firstID;id<firstID+count;id++)
    {
        ids.append(QString("%1").arg(id));
    }

    ctx->testWidget->pimpl->delWidgets->setText(ids.join(","));
    ctx->testWidget->pimpl->delWidgetsButton->click();
}

void checkDeleteInsert(FwlvTestContext* ctx, std::function<void (FwlvTestContext* ctx, size_t firstID, size_t count)> delHandler)
{
    ctx->testWidget->loadItems();
    ++ctx->step;

    QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
    [ctx,delHandler]()
    {
        ctx->fillExpectedAfterLoad();
        BOOST_TEST_CONTEXT("After load with delay") {ctx->doChecks();}

        ++ctx->step;

        auto offset=ctx->idOffset(ctx->view->itemCount()/2);
        auto firstID=(ctx->stickMode==Direction::END?ctx->backID():ctx->frontID())+offset;
        auto countBefore=ctx->view->itemCount();
        auto delCount=countBefore/4;

        delHandler(ctx,firstID,delCount);

        QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
        [ctx,firstID,offset,countBefore,delCount]()
        {
            ++ctx->step;

            UISE_TEST_CHECK_EQUAL(ctx->view->itemCount(),countBefore-delCount);

            UISE_TEST_REQUIRE(ctx->view->lastItem());
            UISE_TEST_REQUIRE(ctx->view->firstItem());

            // check deletion
            auto afterLastID=firstID+delCount;
            bool ok=true;
            for (auto id=ctx->view->lastItem()->id();id<=ctx->view->firstItem()->id();id++)
            {
                auto item=ctx->view->item(id);
                if (id>=firstID && id<afterLastID)
                {
                    ok=item==nullptr;
                    if (!ok)
                    {
                        // compare with ID to see failed ID in log
                        UISE_TEST_CHECK_EQUAL(0,id);
                    }
                }
                else
                {
                    ok=item!=nullptr;
                    if (!ok)
                    {
                        // compare with ID to see failed ID in log
                        UISE_TEST_CHECK_EQUAL(0,id);
                    }
                }
            }

            // insert continuous items with single item gaps
            auto beforeFirstItem=ctx->view->item(firstID+delCount);
            UISE_TEST_REQUIRE(beforeFirstItem);
            std::vector<HelloWorldItemWrapper> newItems;

            auto firstPos=beforeFirstItem->sortValue()+1;
            auto lastPos=beforeFirstItem->sortValue()+delCount;
            size_t insCount=0;
            for (auto i=firstPos;i<=lastPos;i++)
            {
                if (i%2==0)
                {
                    newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,ctx->testWidget->pimpl->items[i])));
                    ++insCount;
                }
            }
            ctx->view->insertContinuousItems(newItems);
            UISE_TEST_CHECK_EQUAL(ctx->view->itemCount(),countBefore-delCount+insCount);

            // insert random items
            newItems.clear();
            for (auto i=firstPos;i<=lastPos;i++)
            {
                if (i%2!=0)
                {
                    newItems.emplace_back(HelloWorldItemWrapper(new HelloWorldItem(i,ctx->testWidget->pimpl->items[i])));
                    ++insCount;
                }
            }
            ctx->view->insertItems(newItems);
            UISE_TEST_CHECK_EQUAL(ctx->view->itemCount(),countBefore);

            // try to insert existing item
            auto itemID=ctx->testWidget->pimpl->items[firstPos];
            auto existingItem=ctx->view->item(itemID);
            auto existingWidget=existingItem->widget();
            UISE_TEST_CHECK(existingItem!=nullptr);
            ctx->view->beginUpdate();
            ctx->view->insertItem(HelloWorldItemWrapper(existingItem->item()));
            ctx->view->endUpdate();
            UISE_TEST_CHECK_EQUAL(ctx->view->itemCount(),countBefore);
            UISE_TEST_CHECK(existingWidget==ctx->view->item(itemID)->widget());

            // overwrite exiting item
            existingItem=ctx->view->item(itemID);
            UISE_TEST_CHECK(existingItem!=nullptr);
            auto item=new HelloWorldItem(firstPos,ctx->testWidget->pimpl->items[firstPos]);
            ctx->view->beginUpdate();
            ctx->view->insertItem(HelloWorldItemWrapper(item));
            ctx->view->endUpdate();
            UISE_TEST_CHECK_EQUAL(ctx->view->itemCount(),countBefore);
            UISE_TEST_CHECK(existingWidget!=ctx->view->item(itemID)->widget());
            UISE_TEST_CHECK(item==ctx->view->item(itemID)->widget());

            // schedule final check
            QTimer::singleShot(FwlvTestContext::PlayStepPeriod,ctx->mainWindow,
            [ctx]()
            {
                ctx->fillExpectedAfterLoad();
                BOOST_TEST_CONTEXT("After insert with delay") {ctx->doChecks();}

                ctx->endTestCase();
            });
        });
    });
}

}

BOOST_AUTO_TEST_CASE(TestDeleteItemsInsert)
{
    auto handler=[](FwlvTestContext* ctx)
    {
        checkDeleteInsert(ctx,delItems);
    };
//    FwlvTestContext::execSingleMode(handler,Qt::Vertical,Direction::HOME,false);
    FwlvTestContext::execAllModes(handler);
}

BOOST_AUTO_TEST_CASE(TestDeleteWidgetsInsert)
{
    auto handler=[](FwlvTestContext* ctx)
    {
        checkDeleteInsert(ctx,delWidgets);
    };
    FwlvTestContext::execAllModes(handler);
}

BOOST_AUTO_TEST_SUITE_END()
