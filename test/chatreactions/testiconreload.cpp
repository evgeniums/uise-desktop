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

/** @file uise/test/chatreactions/testiconreload.cpp
*
*  Regression test for bug-emoji-gallery-icons-sometimes-blank.md: a persistent (code-registered)
*  SvgIconLocator name path must survive SvgIconLocator::reloadIconThemes(), and a
*  DefaultReactionIconPack's generated (non-curated) icons must still resolve after a style/
*  appearance reload triggers that same reload path.
*
*/

/****************************************************************************/

#include <boost/test/unit_test.hpp>

#include <uise/test/uise-testthread.hpp>
#include <uise/desktop/svgiconlocator.hpp>
#include <uise/desktop/reactioniconpack.hpp>
#include <uise/desktop/style.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

//--------------------------------------------------------------------------
// SvgIconLocator -- plain object, no Qt GUI resources touched by reloadIconThemes() with an
// empty theme list, so this can run directly on the test thread.
//--------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(TestSvgIconLocatorReload)

BOOST_AUTO_TEST_CASE(TestPersistentNamePathSurvivesReload)
{
    SvgIconLocator locator;

    locator.addPersistentNamePath(QStringLiteral("Ctx::a"),QStringLiteral("/path/a.svg"));
    locator.addNamePath(QStringLiteral("Ctx::b"),QStringLiteral("/path/b.svg"));

    UISE_TEST_CHECK_EQUAL_QSTR(locator.namePath(QStringLiteral("Ctx::a")),QStringLiteral("/path/a.svg"));
    UISE_TEST_CHECK_EQUAL_QSTR(locator.namePath(QStringLiteral("Ctx::b")),QStringLiteral("/path/b.svg"));

    // No themes to reload -- exercises exactly the clearBeforeReload()/reseed step that
    // previously dropped every plain addNamePath() entry (see reloadIconThemes() ->
    // clearBeforeReload()).
    locator.reloadIconThemes({});

    UISE_TEST_CHECK_EQUAL_QSTR(locator.namePath(QStringLiteral("Ctx::a")),QStringLiteral("/path/a.svg"));
    UISE_TEST_CHECK(locator.namePath(QStringLiteral("Ctx::b")).isEmpty());
}

BOOST_AUTO_TEST_SUITE_END()

//--------------------------------------------------------------------------
// DefaultReactionIconPack -- constructs SvgIcon instances via Style::instance(), which touches
// QPixmap/QSvgRenderer; must run on the GUI thread, same as TestDefaultReactionIconPack in
// testreactionsearch.cpp.
//--------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(TestReactionIconPackReload)

BOOST_AUTO_TEST_CASE(TestGeneratedPackIconResolvesAfterStyleReload)
{
    auto handler=[]()
    {
        DefaultReactionIconPack pack;

        // "innocent" is a generated (non-curated) icon, resolved (and thus cached on the pack's
        // own ReactionIconInfo::icon field, see resolveIcon()) BEFORE the reload -- a control
        // confirming the mechanism generally works, and that the icon involved is not somehow
        // special-cased.
        auto* primed=pack.find(QStringLiteral("innocent"));
        UISE_TEST_REQUIRE(primed!=nullptr);
        UISE_TEST_CHECK(primed->icon!=nullptr);

        // Simulate the appearance/system color-scheme switch that
        // Appearance::applySelected()/enableSystemColorSchemeTracking() trigger in the real app.
        Style::instance().reloadSvgIconTheme();

        // "yum" is a DIFFERENT generated icon, resolved for the first time only AFTER the
        // reload. This is the actual regression: resolveIcon() only calls into the locator when
        // an icon has not been resolved before, so re-checking "innocent" here would just return
        // its already-cached shared_ptr and prove nothing about whether the reload preserved the
        // generated icons' name paths. Before the fix, SvgIconLocator::reloadIconThemes() wipes
        // every addNamePath() entry the pack registered at build() time, so this lookup fails and
        // returns a null (or fallback) icon.
        auto* fresh=pack.find(QStringLiteral("yum"));
        UISE_TEST_REQUIRE(fresh!=nullptr);
        UISE_TEST_CHECK(fresh->icon!=nullptr);

        TestThread::instance()->continueTest();
    };

    TestThread::instance()->postGuiThread(handler);
    TestThread::instance()->execTest();
}

BOOST_AUTO_TEST_SUITE_END()
