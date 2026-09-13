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

/** @file uise/test/chatreactions/testreactionsearch.cpp
*
*  Test of ChatReactionId and DefaultReactionIconPack's keyword prefix search.
*
*/

/****************************************************************************/

#include <algorithm>
#include <set>

#include <boost/test/unit_test.hpp>

#include <uise/test/uise-testthread.hpp>
#include <uise/desktop/chatreaction.hpp>
#include <uise/desktop/reactioniconpack.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

//--------------------------------------------------------------------------
// ChatReactionId -- plain string logic, no Qt GUI object involved, runs directly on the test
// thread (see testalbumlayout.cpp's TestSubstituteColors-style tests for the same reasoning).
//--------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(TestChatReactionId)

BOOST_AUTO_TEST_CASE(TestMakeAndSplit)
{
    auto id=ChatReactionId::make(QStringLiteral("heart"),QStringLiteral("origin.example.com"));
    UISE_TEST_CHECK_EQUAL_QSTR(id,QStringLiteral("heart@origin.example.com"));
    UISE_TEST_CHECK_EQUAL_QSTR(ChatReactionId::iconId(id),QStringLiteral("heart"));
    UISE_TEST_CHECK_EQUAL_QSTR(ChatReactionId::packUri(id),QStringLiteral("origin.example.com"));
}

BOOST_AUTO_TEST_CASE(TestEmptyPackUriMeansDefaultPack)
{
    // empty packUri() -- the default embedded pack -- collapses to a bare icon id, per the task
    // spec's own id grammar.
    auto id=ChatReactionId::make(QStringLiteral("heart"),QString{});
    UISE_TEST_CHECK_EQUAL_QSTR(id,QStringLiteral("heart"));
    UISE_TEST_CHECK_EQUAL_QSTR(ChatReactionId::iconId(id),QStringLiteral("heart"));
    UISE_TEST_CHECK(ChatReactionId::packUri(id).isEmpty());
}

BOOST_AUTO_TEST_CASE(TestPackUriMayContainAtSign)
{
    // split on the FIRST '@' -- a URI legitimately containing '@' (e.g. userinfo) must not be
    // truncated.
    auto id=QStringLiteral("heart@https://user@host/pack");
    UISE_TEST_CHECK_EQUAL_QSTR(ChatReactionId::iconId(id),QStringLiteral("heart"));
    UISE_TEST_CHECK_EQUAL_QSTR(ChatReactionId::packUri(id),QStringLiteral("https://user@host/pack"));
}

BOOST_AUTO_TEST_SUITE_END()

//--------------------------------------------------------------------------
// DefaultReactionIconPack -- constructs SvgIcon instances via Style::instance(), which touches
// QPixmap/QSvgRenderer; must run on the GUI thread, same as any other Qt-object-creating test in
// this suite (see test/utils/testmiscutils.cpp's TestDirectChildWidget).
//--------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(TestDefaultReactionIconPack)

BOOST_AUTO_TEST_CASE(TestPackContents)
{
    auto handler=[]()
    {
        DefaultReactionIconPack pack;

        UISE_TEST_CHECK_EQUAL(pack.count(),size_t{50});

        // 7 basic icons, in pack order, matching the task spec's quick-bar row.
        auto basics=pack.basicIconIds();
        UISE_TEST_REQUIRE_EQUAL(basics.size(),size_t{7});
        UISE_TEST_CHECK_EQUAL_QSTR(basics[0],QStringLiteral("thumbsup"));
        UISE_TEST_CHECK_EQUAL_QSTR(basics[1],QStringLiteral("heart"));
        UISE_TEST_CHECK_EQUAL_QSTR(basics[6],QStringLiteral("fire"));

        auto* heart=pack.find(QStringLiteral("heart"));
        UISE_TEST_REQUIRE(heart!=nullptr);
        UISE_TEST_CHECK_EQUAL_QSTR(heart->iconId,QStringLiteral("heart"));
        // resolved through the ChatReactionPack alias context (resources/style/chatreactions.json)
        // -- a null icon here means the alias table and the pack's id table have drifted apart.
        UISE_TEST_CHECK(heart->icon!=nullptr);

        UISE_TEST_CHECK(pack.find(QStringLiteral("does-not-exist"))==nullptr);
        UISE_TEST_CHECK(pack.at(pack.count())==nullptr);

        TestThread::instance()->continueTest();
    };

    TestThread::instance()->postGuiThread(handler);
    TestThread::instance()->execTest();
}

BOOST_AUTO_TEST_CASE(TestSearchEmptyPrefixReturnsWholePackInOrder)
{
    auto handler=[]()
    {
        DefaultReactionIconPack pack;

        auto all=pack.search(QString{});
        UISE_TEST_REQUIRE_EQUAL(all.size(),pack.count());
        UISE_TEST_CHECK(std::is_sorted(all.begin(),all.end()));
        for (size_t i=0; i<all.size(); ++i)
        {
            UISE_TEST_CHECK_EQUAL(all[i],i);
        }

        TestThread::instance()->continueTest();
    };

    TestThread::instance()->postGuiThread(handler);
    TestThread::instance()->execTest();
}

BOOST_AUTO_TEST_CASE(TestSearchMatchesIconIdItself)
{
    auto handler=[]()
    {
        DefaultReactionIconPack pack;

        auto result=pack.search(QStringLiteral("thumbsup"));
        auto* thumbsup=pack.find(QStringLiteral("thumbsup"));
        UISE_TEST_REQUIRE(thumbsup!=nullptr);
        // at()/find() both point into the pack's own contiguous storage -- pointer subtraction
        // between the two is well-defined and gives back the found entry's index.
        auto expectedIndex=static_cast<size_t>(std::distance(pack.at(0),thumbsup));
        UISE_TEST_CHECK(std::find(result.begin(),result.end(),expectedIndex)!=result.end());

        TestThread::instance()->continueTest();
    };

    TestThread::instance()->postGuiThread(handler);
    TestThread::instance()->execTest();
}

BOOST_AUTO_TEST_CASE(TestSearchMatchesWordInsideMultiWordKeyword)
{
    auto handler=[]()
    {
        DefaultReactionIconPack pack;

        // "heart" keyword-word inside the multi-word "red heart" keyword must be found by the
        // word "heart" alone, not just the full phrase.
        auto byWord=pack.search(QStringLiteral("heart"));
        auto* heart=pack.find(QStringLiteral("heart"));
        UISE_TEST_REQUIRE(heart!=nullptr);

        bool found=false;
        for (auto idx : byWord)
        {
            if (pack.at(idx)==heart)
            {
                found=true;
                break;
            }
        }
        UISE_TEST_CHECK(found);

        TestThread::instance()->continueTest();
    };

    TestThread::instance()->postGuiThread(handler);
    TestThread::instance()->execTest();
}

BOOST_AUTO_TEST_CASE(TestSearchIsCaseInsensitivePrefix)
{
    auto handler=[]()
    {
        DefaultReactionIconPack pack;

        auto lower=pack.search(QStringLiteral("hea"));
        auto upper=pack.search(QStringLiteral("HEA"));
        auto mixed=pack.search(QStringLiteral("Hea"));

        std::sort(lower.begin(),lower.end());
        std::sort(upper.begin(),upper.end());
        std::sort(mixed.begin(),mixed.end());

        UISE_TEST_CHECK(lower==upper);
        UISE_TEST_CHECK(lower==mixed);
        UISE_TEST_CHECK(!lower.empty());

        TestThread::instance()->continueTest();
    };

    TestThread::instance()->postGuiThread(handler);
    TestThread::instance()->execTest();
}

BOOST_AUTO_TEST_CASE(TestSearchResultsHaveNoDuplicates)
{
    auto handler=[]()
    {
        DefaultReactionIconPack pack;

        // "s" is a common-enough prefix (smile, sob, scream, star, ...) that an icon with
        // several matching keywords would show up more than once if the index were not deduped.
        auto result=pack.search(QStringLiteral("s"));
        std::set<size_t> unique(result.begin(),result.end());
        UISE_TEST_CHECK_EQUAL(unique.size(),result.size());

        TestThread::instance()->continueTest();
    };

    TestThread::instance()->postGuiThread(handler);
    TestThread::instance()->execTest();
}

BOOST_AUTO_TEST_CASE(TestSearchNoMatch)
{
    auto handler=[]()
    {
        DefaultReactionIconPack pack;

        auto result=pack.search(QStringLiteral("zzz-not-a-real-keyword"));
        UISE_TEST_CHECK(result.empty());

        TestThread::instance()->continueTest();
    };

    TestThread::instance()->postGuiThread(handler);
    TestThread::instance()->execTest();
}

BOOST_AUTO_TEST_SUITE_END()
