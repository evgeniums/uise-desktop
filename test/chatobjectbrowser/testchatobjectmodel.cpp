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

/** @file uise/test/chatobjectbrowser/testchatobjectmodel.cpp
*
*  Test of ChatObjectKey and ChatObjectBrowserModel, the keyset bookkeeping of the chat object
*  browsers.
*
*/

/****************************************************************************/

#include <vector>

#include <boost/test/unit_test.hpp>

#include <uise/test/uise-testthread.hpp>
#include <uise/desktop/chatobjectbrowser.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

//--------------------------------------------------------------------------
// Plain data structures, no Qt GUI object involved, so everything runs directly on the test
// thread (see testreactionsearch.cpp for the same reasoning).
//
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

ChatObjectItem makeItem(int sortId, quint32 ordinal=0, const QString& messageId=QString{})
{
    ChatObjectItem item;
    item.kind=ChatObjectKind::File;
    item.key=makeKey(sortId,ordinal);
    item.messageId=messageId.isEmpty() ? QStringLiteral("m%1").arg(sortId) : messageId;
    return item;
}

std::vector<ChatObjectItem> makeItems(int from, int to)
{
    std::vector<ChatObjectItem> items;
    for (int i=from; i<to; ++i)
    {
        items.push_back(makeItem(i));
    }
    return items;
}

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(TestChatObjectBrowserModel)

BOOST_AUTO_TEST_CASE(TestKeyOrder)
{
    // bytes first, then the ordinal
    UISE_TEST_CHECK(makeKey(1)<makeKey(2));
    UISE_TEST_CHECK(!(makeKey(2)<makeKey(1)));
    UISE_TEST_CHECK(makeKey(2,0)<makeKey(2,1));
    UISE_TEST_CHECK(makeKey(2,5)<makeKey(3,0));
    UISE_TEST_CHECK(makeKey(2,1)==makeKey(2,1));
    UISE_TEST_CHECK(makeKey(2,1)!=makeKey(2,2));
    UISE_TEST_CHECK(ChatObjectKey{}.isNull());
    UISE_TEST_CHECK(!makeKey(1).isNull());
    UISE_TEST_CHECK(makeKey(1,0).toString()!=makeKey(1,1).toString());
}

BOOST_AUTO_TEST_CASE(TestMergeSortsAndDeduplicates)
{
    ChatObjectBrowserModel model;

    std::vector<ChatObjectItem> incoming;
    incoming.push_back(makeItem(5));
    incoming.push_back(makeItem(2));
    incoming.push_back(makeItem(9));
    incoming.push_back(makeItem(2));
    auto result=model.merge(std::move(incoming));

    UISE_TEST_CHECK_EQUAL(static_cast<int>(model.size()),3);
    UISE_TEST_CHECK_EQUAL(static_cast<int>(result.added.size()),3);
    UISE_TEST_CHECK(result.updated.empty());
    UISE_TEST_CHECK(model.items()[0].key==makeKey(2));
    UISE_TEST_CHECK(model.items()[1].key==makeKey(5));
    UISE_TEST_CHECK(model.items()[2].key==makeKey(9));
    UISE_TEST_CHECK(model.oldest()->key==makeKey(2));
    UISE_TEST_CHECK(model.newest()->key==makeKey(9));

    // a second page with an overlap and a gap
    result=model.merge(makeItems(5,8));
    UISE_TEST_CHECK_EQUAL(static_cast<int>(model.size()),5);
    UISE_TEST_CHECK_EQUAL(static_cast<int>(result.added.size()),2);
    UISE_TEST_CHECK_EQUAL(static_cast<int>(result.updated.size()),1);
    for (size_t i=1; i<model.size(); ++i)
    {
        UISE_TEST_CHECK(model.items()[i-1].key<model.items()[i].key);
    }
}

BOOST_AUTO_TEST_CASE(TestMergeReplacesKnownContent)
{
    ChatObjectBrowserModel model;
    model.merge(makeItems(0,3));

    auto changed=makeItem(1);
    changed.senderName=QStringLiteral("Changed");
    std::vector<ChatObjectItem> incoming;
    incoming.push_back(changed);
    const auto result=model.merge(std::move(incoming));

    UISE_TEST_CHECK(result.added.empty());
    UISE_TEST_CHECK_EQUAL(static_cast<int>(result.updated.size()),1);
    UISE_TEST_CHECK_EQUAL(static_cast<int>(model.size()),3);
    UISE_TEST_REQUIRE(model.find(makeKey(1))!=nullptr);
    UISE_TEST_CHECK_EQUAL_QSTR(model.find(makeKey(1))->senderName,QStringLiteral("Changed"));
    UISE_TEST_CHECK(model.find(makeKey(7))==nullptr);

    UISE_TEST_CHECK(model.replace(changed));
    UISE_TEST_CHECK(!model.replace(makeItem(7)));
}

BOOST_AUTO_TEST_CASE(TestOlderAndNewerThan)
{
    ChatObjectBrowserModel model;
    model.merge(makeItems(10,20));

    // nearest first: descending keys
    auto older=model.olderThan(makeKey(15),3);
    UISE_TEST_REQUIRE_EQUAL(static_cast<int>(older.size()),3);
    UISE_TEST_CHECK(older[0].key==makeKey(14));
    UISE_TEST_CHECK(older[1].key==makeKey(13));
    UISE_TEST_CHECK(older[2].key==makeKey(12));

    // nearest first: ascending keys
    auto newer=model.newerThan(makeKey(15),3);
    UISE_TEST_REQUIRE_EQUAL(static_cast<int>(newer.size()),3);
    UISE_TEST_CHECK(newer[0].key==makeKey(16));
    UISE_TEST_CHECK(newer[2].key==makeKey(18));

    // fewer than asked for at the edges, none beyond them
    UISE_TEST_CHECK_EQUAL(static_cast<int>(model.olderThan(makeKey(12),5).size()),2);
    UISE_TEST_CHECK(model.olderThan(makeKey(10),5).empty());
    UISE_TEST_CHECK_EQUAL(static_cast<int>(model.newerThan(makeKey(17),5).size()),2);
    UISE_TEST_CHECK(model.newerThan(makeKey(19),5).empty());

    // an anchor that is not in the model still splits it
    UISE_TEST_CHECK_EQUAL(static_cast<int>(model.olderThan(makeKey(15,1),2).size()),2);
    UISE_TEST_CHECK(model.olderThan(makeKey(15,1),1)[0].key==makeKey(15));
    UISE_TEST_CHECK(model.newerThan(makeKey(15,1),1)[0].key==makeKey(16));
}

BOOST_AUTO_TEST_CASE(TestRemove)
{
    ChatObjectBrowserModel model;
    std::vector<ChatObjectItem> items;
    // one message with three objects, as an album
    items.push_back(makeItem(1,0,QStringLiteral("album")));
    items.push_back(makeItem(1,1,QStringLiteral("album")));
    items.push_back(makeItem(1,2,QStringLiteral("album")));
    items.push_back(makeItem(2));
    items.push_back(makeItem(3));
    model.merge(std::move(items));
    UISE_TEST_CHECK_EQUAL(static_cast<int>(model.size()),5);

    const auto removed=model.removeMessage(QStringLiteral("album"));
    UISE_TEST_REQUIRE_EQUAL(static_cast<int>(removed.size()),3);
    UISE_TEST_CHECK(removed[0]==makeKey(1,0));
    UISE_TEST_CHECK(removed[2]==makeKey(1,2));
    UISE_TEST_CHECK_EQUAL(static_cast<int>(model.size()),2);
    UISE_TEST_CHECK(model.removeMessage(QStringLiteral("album")).empty());

    UISE_TEST_CHECK(model.remove(makeKey(2)));
    UISE_TEST_CHECK(!model.remove(makeKey(2)));
    UISE_TEST_CHECK_EQUAL(static_cast<int>(model.size()),1);
}

BOOST_AUTO_TEST_CASE(TestMonthKey)
{
    UISE_TEST_CHECK_EQUAL(chatObjectMonthKey(QDateTime{}),-1);

    const auto first=chatObjectMonthKey(QDateTime(QDate(2026,9,10),QTime(12,0)));
    const auto same=chatObjectMonthKey(QDateTime(QDate(2026,9,28),QTime(12,0)));
    const auto next=chatObjectMonthKey(QDateTime(QDate(2026,10,1),QTime(12,0)));
    const auto nextYear=chatObjectMonthKey(QDateTime(QDate(2027,9,10),QTime(12,0)));
    UISE_TEST_CHECK_EQUAL(first,same);
    UISE_TEST_CHECK(first<next);
    UISE_TEST_CHECK(next<nextYear);
    UISE_TEST_CHECK(first>=0);

    UISE_TEST_CHECK(chatObjectMonthTitle(QDateTime{}).isEmpty());
    UISE_TEST_CHECK(!chatObjectMonthTitle(QDateTime(QDate(2026,9,10),QTime(12,0))).isEmpty());
    UISE_TEST_CHECK(chatObjectMonthTitle(QDateTime(QDate(2026,9,10),QTime(12,0)))
                    !=chatObjectMonthTitle(QDateTime(QDate(2026,10,10),QTime(12,0))));
}

BOOST_AUTO_TEST_CASE(TestStartsMonth)
{
    ChatObjectBrowserModel model;

    // oldest to newest: two in September, two in October, one in November
    const QDate dates[]={QDate(2026,9,10),QDate(2026,9,20),QDate(2026,10,3),QDate(2026,10,4),QDate(2026,11,1)};
    std::vector<ChatObjectItem> items;
    for (int i=0; i<5; ++i)
    {
        auto item=makeItem(i+1);
        item.dateTime=QDateTime(dates[i],QTime(12,0));
        items.push_back(std::move(item));
    }
    model.merge(std::move(items));

    // the first of a month is its NEWEST object, the widgets show newest first
    UISE_TEST_CHECK(model.startsMonth(makeKey(5)));
    UISE_TEST_CHECK(model.startsMonth(makeKey(4)));
    UISE_TEST_CHECK(!model.startsMonth(makeKey(3)));
    UISE_TEST_CHECK(model.startsMonth(makeKey(2)));
    UISE_TEST_CHECK(!model.startsMonth(makeKey(1)));
    UISE_TEST_CHECK(!model.startsMonth(makeKey(77)));

    // a newer object of the month takes the heading from the one that had it
    auto newer=makeItem(6);
    newer.dateTime=QDateTime(QDate(2026,11,15),QTime(12,0));
    std::vector<ChatObjectItem> more;
    more.push_back(std::move(newer));
    model.merge(std::move(more));
    UISE_TEST_CHECK(!model.startsMonth(makeKey(5)));
    UISE_TEST_CHECK(model.startsMonth(makeKey(6)));
}

BOOST_AUTO_TEST_CASE(TestEndFlagsAndClear)
{
    ChatObjectBrowserModel model;
    UISE_TEST_CHECK(!model.isEndOlder());
    UISE_TEST_CHECK(!model.isEndNewer());

    model.merge(makeItems(0,3));
    model.setEndOlder(true);
    model.setEndNewer(true);
    UISE_TEST_CHECK(model.isEndOlder());
    UISE_TEST_CHECK(model.isEndNewer());

    model.clear();
    UISE_TEST_CHECK(model.empty());
    UISE_TEST_CHECK(!model.isEndOlder());
    UISE_TEST_CHECK(!model.isEndNewer());
    UISE_TEST_CHECK(model.oldest()==nullptr);
    UISE_TEST_CHECK(model.newest()==nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
