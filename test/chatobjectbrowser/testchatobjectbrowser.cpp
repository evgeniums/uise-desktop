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

/** @file uise/test/chatobjectbrowser/testchatobjectbrowser.cpp
*
*  Test of the paging state machine of AbstractChatObjectBrowser (requests, answers, edges,
*  removal, empty state) through a browser without a view, and of ChatObjectBrowserRegistry.
*
*/

/****************************************************************************/

#include <memory>
#include <vector>

#include <QLabel>

#include <boost/test/unit_test.hpp>

#include <uise/test/uise-testthread.hpp>
#include <uise/desktop/chatobjectbrowser.hpp>
#include <uise/desktop/chatobjectbrowserpanel.hpp>
#include <uise/desktop/style.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

//--------------------------------------------------------------------------
// Strict inequalities are written as UISE_TEST_CHECK(a<b): UISE_TEST_CHECK_GT is wired to
// BOOST_CHECK_GE and there is no UISE_TEST_CHECK_LT at all.
//--------------------------------------------------------------------------

namespace {

ChatObjectKey makeKey(int sortId, quint32 ordinal=0)
{
    ChatObjectKey key;
    key.sortId=QByteArray::number(sortId).rightJustified(6,'0');
    key.ordinal=ordinal;
    return key;
}

ChatObjectItem makeItem(int sortId)
{
    ChatObjectItem item;
    item.kind=ChatObjectKind::File;
    item.key=makeKey(sortId);
    item.messageId=QStringLiteral("m%1").arg(sortId);
    return item;
}

//! Items from..to-1 as the answer to the request.
ChatObjectsPage makePage(const ChatObjectsRequest& request, int from, int to, bool endBefore, bool endAfter)
{
    ChatObjectsPage page;
    page.requestId=request.requestId;
    for (int i=from; i<to; ++i)
    {
        page.items.push_back(makeItem(i));
    }
    page.endBefore=endBefore;
    page.endAfter=endAfter;
    return page;
}

/**
 * @brief A browser without a view: records what the base class asks its view to do, and makes
 *  serveEdge() callable, as the view does when it scrolls.
 *
 * No Q_OBJECT: nothing here needs a signal or a slot.
 */
class TestBrowser : public AbstractChatObjectBrowser
{
    public:

        explicit TestBrowser(QWidget* parent=nullptr)
            : AbstractChatObjectBrowser(ChatObjectKind::File,parent)
        {
            setPageCount(10);
            setRequestCb(
                [this](const ChatObjectsRequest& request)
                {
                    requests.push_back(request);
                }
            );
        }

        using AbstractChatObjectBrowser::serveEdge;

        QLabel* emptyLabel() const
        {
            return findChild<QLabel*>("emptyLabel");
        }

        std::vector<ChatObjectsRequest> requests;

        int resetCount=0;
        int loadCount=0;
        std::vector<ChatObjectItem> loaded;
        ChatObjectKey loadedFocus;
        std::vector<ChatObjectKey> removed;
        std::vector<ChatObjectItem> updated;
        int edgeOlder=0;
        int edgeNewer=0;

    protected:

        void doReset() override
        {
            ++resetCount;
            loaded.clear();
        }

        void doLoad(const std::vector<ChatObjectItem>& items, const ChatObjectKey& focus) override
        {
            ++loadCount;
            loaded=items;
            loadedFocus=focus;
        }

        void doRemove(const std::vector<ChatObjectKey>& keys) override
        {
            removed.insert(removed.end(),keys.begin(),keys.end());
        }

        void doUpdate(const ChatObjectItem& item) override
        {
            updated.push_back(item);
        }

        void doEdgeArrived(bool older) override
        {
            if (older)
            {
                ++edgeOlder;
            }
            else
            {
                ++edgeNewer;
            }
        }
};

template <typename Fn>
void runInGui(Fn fn)
{
    auto handler=[fn]()
    {
        // The browsers own a SearchLineEdit, which looks its icons up when it is built.
        static bool styled=false;
        if (!styled)
        {
            Style::instance().applyStyleSheet();
            styled=true;
        }
        fn();
        TestThread::instance()->continueTest();
    };
    TestThread::instance()->postGuiThread(handler);
    TestThread::instance()->execTest();
}

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(TestChatObjectBrowser)

BOOST_AUTO_TEST_CASE(TestReloadAsksForTheNewestEnd)
{
    runInGui(
        []()
        {
            TestBrowser browser;
            browser.reload();

            UISE_TEST_REQUIRE_EQUAL(static_cast<int>(browser.requests.size()),1);
            const auto& request=browser.requests[0];
            UISE_TEST_CHECK(request.anchor.isNull());
            UISE_TEST_CHECK(!request.anchorInclusive);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(request.countBefore),20);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(request.countAfter),0);
            UISE_TEST_CHECK_EQUAL_QSTR(request.kind,ChatObjectKind::File);
            UISE_TEST_CHECK(request.requestId!=0);
            UISE_TEST_CHECK(browser.isLoading());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestAnswerFillsTheBrowser)
{
    runInGui(
        []()
        {
            TestBrowser browser;
            browser.reload();
            // the owner says nothing about the newer end: a null anchor implies it
            browser.applyPage(makePage(browser.requests[0],90,100,false,false));

            UISE_TEST_CHECK(!browser.isLoading());
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.model().size()),10);
            UISE_TEST_CHECK(browser.model().isEndNewer());
            UISE_TEST_CHECK(!browser.model().isEndOlder());

            UISE_TEST_CHECK_EQUAL(browser.loadCount,1);
            UISE_TEST_REQUIRE_EQUAL(static_cast<int>(browser.loaded.size()),10);
            // the view gets its items newest first
            UISE_TEST_CHECK(browser.loaded.front().key==makeKey(99));
            UISE_TEST_CHECK(browser.loaded.back().key==makeKey(90));
            UISE_TEST_CHECK(browser.loadedFocus.isNull());

            UISE_TEST_REQUIRE(browser.emptyLabel()!=nullptr);
            UISE_TEST_CHECK(browser.emptyLabel()->isHidden());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestEmptyAnswerShowsTheEmptyText)
{
    runInGui(
        []()
        {
            TestBrowser browser;
            browser.setEmptyText(QStringLiteral("Nothing"));
            UISE_TEST_CHECK_EQUAL_QSTR(browser.emptyText(),QStringLiteral("Nothing"));

            browser.reload();
            // nothing is shown while the first answer is pending
            UISE_TEST_REQUIRE(browser.emptyLabel()!=nullptr);
            UISE_TEST_CHECK(browser.emptyLabel()->isHidden());

            // even without the end flags: an empty answer is the end
            browser.applyPage(makePage(browser.requests[0],0,0,false,false));
            UISE_TEST_CHECK(browser.model().empty());
            UISE_TEST_CHECK(browser.model().isEndOlder());
            UISE_TEST_CHECK(browser.model().isEndNewer());
            UISE_TEST_CHECK(!browser.emptyLabel()->isHidden());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestStaleAnswerIsIgnored)
{
    runInGui(
        []()
        {
            TestBrowser browser;
            browser.reload();
            browser.reload();
            UISE_TEST_REQUIRE_EQUAL(static_cast<int>(browser.requests.size()),2);
            UISE_TEST_CHECK(browser.requests[0].requestId!=browser.requests[1].requestId);

            // the answer to the first reload arrives after the second one was issued
            browser.applyPage(makePage(browser.requests[0],0,5,true,true));
            UISE_TEST_CHECK(browser.model().empty());
            UISE_TEST_CHECK(browser.isLoading());
            UISE_TEST_CHECK_EQUAL(browser.loadCount,0);

            // an answer to a request nobody made
            ChatObjectsPage unknown;
            unknown.requestId=browser.requests[1].requestId+1000;
            unknown.items.push_back(makeItem(1));
            browser.applyPage(unknown);
            UISE_TEST_CHECK(browser.model().empty());

            browser.applyPage(makePage(browser.requests[1],0,5,true,true));
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.model().size()),5);
            UISE_TEST_CHECK(!browser.isLoading());

            // and an answer is taken only once
            browser.applyPage(makePage(browser.requests[1],5,9,true,true));
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.model().size()),5);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestOpenAtAsksAroundTheKey)
{
    runInGui(
        []()
        {
            TestBrowser browser;
            browser.openAt(makeKey(50));

            UISE_TEST_REQUIRE_EQUAL(static_cast<int>(browser.requests.size()),1);
            const auto& request=browser.requests[0];
            UISE_TEST_CHECK(request.anchor==makeKey(50));
            UISE_TEST_CHECK(request.anchorInclusive);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(request.countBefore),20);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(request.countAfter),10);

            browser.applyPage(makePage(request,30,60,false,false));
            UISE_TEST_CHECK(browser.loadedFocus==makeKey(50));
            // a window in the middle of the history: neither end is known
            UISE_TEST_CHECK(!browser.model().isEndOlder());
            UISE_TEST_CHECK(!browser.model().isEndNewer());

            // a null key is the same as reload()
            browser.openAt(ChatObjectKey{});
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.requests.size()),2);
            UISE_TEST_CHECK(browser.requests[1].anchor.isNull());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestSynchronousAnswer)
{
    runInGui(
        []()
        {
            TestBrowser browser;
            // the owner answers from inside the callback
            browser.setRequestCb(
                [&browser](const ChatObjectsRequest& request)
                {
                    browser.requests.push_back(request);
                    browser.applyPage(makePage(request,0,3,true,true));
                }
            );
            browser.reload();

            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.model().size()),3);
            UISE_TEST_CHECK(!browser.isLoading());
            UISE_TEST_CHECK_EQUAL(browser.loadCount,1);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestEdgeRequests)
{
    runInGui(
        []()
        {
            TestBrowser browser;
            browser.reload();
            browser.applyPage(makePage(browser.requests[0],90,100,false,false));
            browser.requests.clear();

            // enough in the model: served from it, the owner is not asked
            auto items=browser.serveEdge(makeKey(95),3,true);
            UISE_TEST_REQUIRE_EQUAL(static_cast<int>(items.size()),3);
            UISE_TEST_CHECK(items[0].key==makeKey(94));
            UISE_TEST_CHECK(items[2].key==makeKey(92));
            UISE_TEST_CHECK(browser.requests.empty());

            // not enough: what there is, and one request anchored at the model's oldest object
            items=browser.serveEdge(makeKey(92),5,true);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(items.size()),2);
            UISE_TEST_REQUIRE_EQUAL(static_cast<int>(browser.requests.size()),1);
            UISE_TEST_CHECK(browser.requests[0].anchor==makeKey(90));
            UISE_TEST_CHECK(!browser.requests[0].anchorInclusive);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.requests[0].countBefore),10);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.requests[0].countAfter),0);

            // one request per side at a time
            items=browser.serveEdge(makeKey(90),5,true);
            UISE_TEST_CHECK(items.empty());
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.requests.size()),1);

            // the answer: the model grows and the view is told to take from it
            browser.applyPage(makePage(browser.requests[0],80,90,false,false));
            UISE_TEST_CHECK_EQUAL(browser.edgeOlder,1);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.model().size()),20);
            UISE_TEST_CHECK(!browser.model().isEndOlder());

            // the newer end is known (a null anchor implied it), so there is nothing to ask;
            // the items come in the widgets' order, newest first
            items=browser.serveEdge(makeKey(97),5,false);
            UISE_TEST_REQUIRE_EQUAL(static_cast<int>(items.size()),2);
            UISE_TEST_CHECK(items[0].key==makeKey(99));
            UISE_TEST_CHECK(items[1].key==makeKey(98));
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.requests.size()),1);

            // an empty answer is the end of that side, and it is not asked about again
            items=browser.serveEdge(makeKey(80),5,true);
            UISE_TEST_CHECK(items.empty());
            UISE_TEST_REQUIRE_EQUAL(static_cast<int>(browser.requests.size()),2);
            UISE_TEST_CHECK(browser.requests[1].anchor==makeKey(80));
            browser.applyPage(makePage(browser.requests[1],0,0,false,false));
            UISE_TEST_CHECK(browser.model().isEndOlder());
            items=browser.serveEdge(makeKey(80),5,true);
            UISE_TEST_CHECK(items.empty());
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.requests.size()),2);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestNewerEdgeAfterOpenAt)
{
    runInGui(
        []()
        {
            TestBrowser browser;
            browser.openAt(makeKey(50));
            browser.applyPage(makePage(browser.requests[0],30,60,false,false));
            browser.requests.clear();

            // the newer side is not known to end: asking for more than the model has
            auto items=browser.serveEdge(makeKey(58),5,false);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(items.size()),1);
            UISE_TEST_REQUIRE_EQUAL(static_cast<int>(browser.requests.size()),1);
            UISE_TEST_CHECK(browser.requests[0].anchor==makeKey(59));
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.requests[0].countBefore),0);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.requests[0].countAfter),10);

            // the owner says that is the newest end
            browser.applyPage(makePage(browser.requests[0],60,63,false,true));
            UISE_TEST_CHECK_EQUAL(browser.edgeNewer,1);
            UISE_TEST_CHECK(browser.model().isEndNewer());
            UISE_TEST_CHECK(!browser.model().isEndOlder());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestRemoveMessageObjects)
{
    runInGui(
        []()
        {
            TestBrowser browser;
            browser.reload();
            browser.applyPage(makePage(browser.requests[0],0,5,true,true));

            browser.removeMessageObjects(QStringLiteral("nothing"));
            UISE_TEST_CHECK(browser.removed.empty());
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.model().size()),5);

            browser.removeMessageObjects(QStringLiteral("m2"));
            UISE_TEST_REQUIRE_EQUAL(static_cast<int>(browser.removed.size()),1);
            UISE_TEST_CHECK(browser.removed[0]==makeKey(2));
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.model().size()),4);
            UISE_TEST_CHECK(browser.emptyLabel()->isHidden());

            // everything is gone and both ends were known: that is the empty state, and the
            // owner is not asked again
            browser.removeMessageObjects(QStringLiteral("m0"));
            browser.removeMessageObjects(QStringLiteral("m1"));
            browser.removeMessageObjects(QStringLiteral("m3"));
            browser.removeMessageObjects(QStringLiteral("m4"));
            UISE_TEST_CHECK(browser.model().empty());
            UISE_TEST_CHECK(!browser.emptyLabel()->isHidden());
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.requests.size()),1);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestRemoveEverythingLoadedWhileThereIsMore)
{
    runInGui(
        []()
        {
            TestBrowser browser;
            browser.reload();
            browser.applyPage(makePage(browser.requests[0],90,93,false,false));
            UISE_TEST_CHECK(!browser.model().isEndOlder());

            browser.removeMessageObjects(QStringLiteral("m90"));
            browser.removeMessageObjects(QStringLiteral("m91"));
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.requests.size()),1);
            browser.removeMessageObjects(QStringLiteral("m92"));

            // older objects exist that the browser has not seen: it loads again
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.requests.size()),2);
            UISE_TEST_CHECK(browser.requests[1].anchor.isNull());
            UISE_TEST_CHECK(browser.emptyLabel()->isHidden());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestUpdateItem)
{
    runInGui(
        []()
        {
            TestBrowser browser;
            browser.reload();
            browser.applyPage(makePage(browser.requests[0],0,3,true,true));

            auto changed=makeItem(1);
            changed.senderName=QStringLiteral("Changed");
            browser.updateItem(changed);

            UISE_TEST_REQUIRE_EQUAL(static_cast<int>(browser.updated.size()),1);
            UISE_TEST_CHECK(browser.updated[0].key==makeKey(1));
            UISE_TEST_REQUIRE(browser.model().find(makeKey(1))!=nullptr);
            UISE_TEST_CHECK_EQUAL_QSTR(browser.model().find(makeKey(1))->senderName,QStringLiteral("Changed"));

            // not loaded: nothing to update
            browser.updateItem(makeItem(77));
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.updated.size()),1);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestRequestsCarryTheSearchText)
{
    runInGui(
        []()
        {
            TestBrowser browser;

            // a text set before the first load goes with the first request
            browser.setSearchText(QStringLiteral("abc"));
            UISE_TEST_CHECK(browser.requests.empty());
            UISE_TEST_CHECK_EQUAL_QSTR(browser.searchText(),QStringLiteral("abc"));

            browser.reload();
            UISE_TEST_REQUIRE_EQUAL(static_cast<int>(browser.requests.size()),1);
            UISE_TEST_CHECK_EQUAL_QSTR(browser.requests[0].query,QStringLiteral("abc"));

            // and so does every request for more
            browser.applyPage(makePage(browser.requests[0],90,100,false,false));
            browser.serveEdge(makeKey(92),5,true);
            UISE_TEST_REQUIRE_EQUAL(static_cast<int>(browser.requests.size()),2);
            UISE_TEST_CHECK_EQUAL_QSTR(browser.requests[1].query,QStringLiteral("abc"));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestSearchTextReloads)
{
    runInGui(
        []()
        {
            TestBrowser browser;
            int changes=0;
            QString lastText;
            QObject::connect(&browser,&AbstractChatObjectBrowser::searchTextChanged,&browser,
                [&changes,&lastText](const QString& text)
                {
                    ++changes;
                    lastText=text;
                }
            );

            browser.reload();
            browser.applyPage(makePage(browser.requests[0],0,5,true,true));
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.requests.size()),1);
            const int resets=browser.resetCount;

            browser.setSearchText(QStringLiteral("x"));
            UISE_TEST_REQUIRE_EQUAL(static_cast<int>(browser.requests.size()),2);
            UISE_TEST_CHECK_EQUAL_QSTR(browser.requests[1].query,QStringLiteral("x"));
            // from the newest end, with nothing of the old result left
            UISE_TEST_CHECK(browser.requests[1].anchor.isNull());
            UISE_TEST_CHECK(browser.model().empty());
            UISE_TEST_CHECK_EQUAL(browser.resetCount,resets+1);
            UISE_TEST_CHECK_EQUAL(changes,1);
            UISE_TEST_CHECK_EQUAL_QSTR(lastText,QStringLiteral("x"));

            // the same text, with or without spaces around it, changes nothing
            browser.setSearchText(QStringLiteral("x"));
            browser.setSearchText(QStringLiteral("  x "));
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.requests.size()),2);
            UISE_TEST_CHECK_EQUAL(changes,1);

            // an empty text ends the search
            browser.setSearchText(QString{});
            UISE_TEST_REQUIRE_EQUAL(static_cast<int>(browser.requests.size()),3);
            UISE_TEST_CHECK(browser.requests[2].query.isEmpty());
            UISE_TEST_CHECK_EQUAL(changes,2);

            // the answer to the request before the last text is dropped
            browser.applyPage(makePage(browser.requests[1],0,5,true,true));
            UISE_TEST_CHECK(browser.model().empty());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestSearchLineVisibility)
{
    runInGui(
        []()
        {
            TestBrowser browser;
            auto* edit=browser.findChild<QWidget*>("searchEdit");
            UISE_TEST_REQUIRE(edit!=nullptr);

            // hidden until asked for
            UISE_TEST_CHECK(!browser.isSearchVisible());
            UISE_TEST_CHECK(edit->isHidden());

            browser.setSearchVisible(true);
            UISE_TEST_CHECK(browser.isSearchVisible());
            UISE_TEST_CHECK(!edit->isHidden());

            browser.reload();
            browser.applyPage(makePage(browser.requests[0],0,5,true,true));
            browser.setSearchText(QStringLiteral("q"));
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.requests.size()),2);

            // hiding the line ends the search too
            browser.setSearchVisible(false);
            UISE_TEST_CHECK(!browser.isSearchVisible());
            UISE_TEST_CHECK(edit->isHidden());
            UISE_TEST_CHECK(browser.searchText().isEmpty());
            UISE_TEST_REQUIRE_EQUAL(static_cast<int>(browser.requests.size()),3);
            UISE_TEST_CHECK(browser.requests[2].query.isEmpty());

            // hiding a line without a search does not reload
            browser.setSearchVisible(false);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.requests.size()),3);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestNothingFoundIsNotAnEmptyChat)
{
    runInGui(
        []()
        {
            TestBrowser browser;
            browser.setEmptyText(QStringLiteral("Nothing"));
            UISE_TEST_REQUIRE(browser.emptyLabel()!=nullptr);

            browser.setSearchText(QStringLiteral("zzz"));
            browser.reload();
            browser.applyPage(makePage(browser.requests[0],0,0,true,true));
            UISE_TEST_CHECK(!browser.emptyLabel()->isHidden());
            UISE_TEST_CHECK(!browser.emptyLabel()->text().isEmpty());
            UISE_TEST_CHECK(browser.emptyLabel()->text()!=QStringLiteral("Nothing"));

            // without a search the text of the host is shown
            browser.setSearchText(QString{});
            browser.applyPage(makePage(browser.requests[1],0,0,true,true));
            UISE_TEST_CHECK(!browser.emptyLabel()->isHidden());
            UISE_TEST_CHECK_EQUAL_QSTR(browser.emptyLabel()->text(),QStringLiteral("Nothing"));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestGroupByMonthSwitch)
{
    runInGui(
        []()
        {
            TestBrowser browser;
            UISE_TEST_CHECK(browser.isGroupByMonth());

            // nothing loaded: nothing for the view to redo
            browser.setGroupByMonth(false);
            UISE_TEST_CHECK(!browser.isGroupByMonth());
            UISE_TEST_CHECK_EQUAL(browser.loadCount,0);

            browser.reload();
            browser.applyPage(makePage(browser.requests[0],0,5,true,true));
            UISE_TEST_CHECK_EQUAL(browser.loadCount,1);

            // the view is told, by default with a reload from the model; no request to the owner
            browser.setGroupByMonth(true);
            UISE_TEST_CHECK_EQUAL(browser.loadCount,2);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.requests.size()),1);

            // the same value is not a change
            browser.setGroupByMonth(true);
            UISE_TEST_CHECK_EQUAL(browser.loadCount,2);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestRegistry)
{
    runInGui(
        []()
        {
            auto registry=ChatObjectBrowserRegistry::defaults();

            const auto kinds=registry.kinds();
            UISE_TEST_REQUIRE_EQUAL(static_cast<int>(kinds.size()),5);
            UISE_TEST_CHECK_EQUAL_QSTR(kinds[0],ChatObjectKind::Image);
            UISE_TEST_CHECK_EQUAL_QSTR(kinds[1],ChatObjectKind::File);
            UISE_TEST_CHECK_EQUAL_QSTR(kinds[2],ChatObjectKind::Voice);
            UISE_TEST_CHECK_EQUAL_QSTR(kinds[3],ChatObjectKind::Link);
            UISE_TEST_CHECK_EQUAL_QSTR(kinds[4],ChatObjectKind::Mention);

            UISE_TEST_CHECK(registry.find(QStringLiteral("video"))==nullptr);

            // every stock kind makes a browser of its own kind
            for (const auto& kind : kinds)
            {
                const auto* info=registry.find(kind);
                UISE_TEST_REQUIRE(info!=nullptr);
                UISE_TEST_CHECK(!info->title().isEmpty());
                UISE_TEST_CHECK(!info->emptyText().isEmpty());
                UISE_TEST_REQUIRE(static_cast<bool>(info->factory));

                std::unique_ptr<AbstractChatObjectBrowser> browser{info->factory(nullptr)};
                UISE_TEST_REQUIRE(browser!=nullptr);
                UISE_TEST_CHECK_EQUAL_QSTR(browser->kind(),kind);
            }

            // a new kind is registered, not wired in
            ChatObjectBrowserKindInfo video;
            video.title=[]()
            {
                return QStringLiteral("Videos");
            };
            video.factory=[](QWidget*) -> AbstractChatObjectBrowser*
            {
                return nullptr;
            };
            registry.registerKind(QStringLiteral("video"),video);
            UISE_TEST_CHECK(registry.find(QStringLiteral("video"))!=nullptr);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(registry.kinds().size()),6);

            // registering again replaces
            registry.registerKind(QStringLiteral("video"),video);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(registry.kinds().size()),6);

            UISE_TEST_CHECK(registry.unregisterKind(ChatObjectKind::Image));
            UISE_TEST_CHECK(registry.find(ChatObjectKind::Image)==nullptr);
            UISE_TEST_CHECK(!registry.unregisterKind(ChatObjectKind::Image));
            UISE_TEST_CHECK_EQUAL(static_cast<int>(registry.kinds().size()),5);
        }
    );
}

BOOST_AUTO_TEST_SUITE_END()
