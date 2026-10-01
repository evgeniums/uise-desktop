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

/** @file uise/test/messageformatting/testcolorpalette.cpp
*
*  Tests ColorPaletteTheme, the palette index helpers and Style's palette lookup
*  (the "kind":"palette" style JSON driving sender-name and generated-avatar colours).
*
*  The pure cases run directly on Boost's own thread; the ones that go through the Style
*  singleton are marshalled through TestThread::execGuiThread(), like every Style case in
*  testsyntaxhighlighter.cpp.
*
*/

/****************************************************************************/

#include <algorithm>
#include <cstring>
#include <set>

#include <boost/test/unit_test.hpp>

#include <QCryptographicHash>

#include <uise/test/uise-testthread.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/colorpalette.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

BOOST_AUTO_TEST_SUITE(TestColorPalette)

BOOST_AUTO_TEST_CASE(TestLoadValid)
{
    ColorPaletteTheme theme;
    QString err;
    auto ok=theme.loadFromJson(QStringLiteral(
        R"({"kind":"palette","theme":"dark","palettes":{"a":["#112233","#445566"],"b":["#ffffff"]}})"),&err);
    UISE_TEST_REQUIRE(ok);
    UISE_TEST_CHECK_EQUAL_QSTR(theme.name(),QStringLiteral("dark"));
    UISE_TEST_CHECK(theme.hasPalette(QStringLiteral("a")));
    UISE_TEST_REQUIRE_EQUAL(theme.palette(QStringLiteral("a")).size(),size_t(2));
    UISE_TEST_CHECK_EQUAL_QSTR(theme.palette(QStringLiteral("a")).at(1).name(),QStringLiteral("#445566"));
    UISE_TEST_CHECK_EQUAL(theme.palette(QStringLiteral("b")).size(),size_t(1));
}

BOOST_AUTO_TEST_CASE(TestNoThemeMeansAny)
{
    ColorPaletteTheme theme;
    UISE_TEST_REQUIRE(theme.loadFromJson(QStringLiteral(R"({"palettes":{"a":["#112233"]}})")));
    UISE_TEST_CHECK_EQUAL_QSTR(theme.name(),QString(Style::AnyColorTheme));
}

BOOST_AUTO_TEST_CASE(TestInvalidColourIsSkippedNotFatal)
{
    // the very typo the original avatar list shipped with: no leading '#'
    ColorPaletteTheme theme;
    UISE_TEST_REQUIRE(theme.loadFromJson(QStringLiteral(
        R"({"palettes":{"a":["#112233","ef476f",42,"#445566"]}})")));
    UISE_TEST_CHECK_EQUAL(theme.palette(QStringLiteral("a")).size(),size_t(2));
}

BOOST_AUTO_TEST_CASE(TestEmptyArrayIsDefinedButEmpty)
{
    // the opt-out: defined (so it overrides an earlier theme's list) yet empty
    ColorPaletteTheme theme;
    UISE_TEST_REQUIRE(theme.loadFromJson(QStringLiteral(R"({"palettes":{"a":[]}})")));
    UISE_TEST_CHECK(theme.hasPalette(QStringLiteral("a")));
    UISE_TEST_CHECK(theme.palette(QStringLiteral("a")).empty());
    UISE_TEST_CHECK(!theme.hasPalette(QStringLiteral("missing")));
}

BOOST_AUTO_TEST_CASE(TestMalformedDocuments)
{
    ColorPaletteTheme theme;
    QString err;
    UISE_TEST_CHECK(!theme.loadFromJson(QStringLiteral("not json"),&err));
    UISE_TEST_CHECK(!theme.loadFromJson(QStringLiteral("[]"),&err));
    UISE_TEST_CHECK(!theme.loadFromJson(QStringLiteral(R"({"theme":"dark"})"),&err));          // no "palettes"
    UISE_TEST_CHECK(!theme.loadFromJson(QStringLiteral(R"({"palettes":{"a":"#112233"}})"),&err)); // not an array
    UISE_TEST_CHECK(!err.isEmpty());
}

BOOST_AUTO_TEST_CASE(TestPaletteIndexMatchesLegacyAvatarAlgorithm)
{
    // Pins paletteIndex() to what avatar.cpp did inline before it was factored out, so a palette
    // of unchanged length keeps every avatar's colour.
    for (const char* key : {"a","user-1","3f786850e387550fdab836ed7e6dc881de23001b",""})
    {
        QCryptographicHash hash{QCryptographicHash::Sha1};
        hash.addData(QByteArrayView(key));
        auto digest=hash.result();

        size_t idx=0;
        memcpy(&idx,digest.constData(),sizeof(idx));

        for (size_t count : {size_t(1),size_t(8),size_t(15),size_t(16)})
        {
            UISE_TEST_CHECK_EQUAL(paletteIndex(digest,count),idx%count);
            UISE_TEST_CHECK_EQUAL(paletteIndexForKey(QByteArrayView(key),count),idx%count);
        }
    }
}

BOOST_AUTO_TEST_CASE(TestPaletteIndexDegenerateInput)
{
    UISE_TEST_CHECK_EQUAL(paletteIndex(QByteArray("abc"),8),size_t(0));    // digest too short
    UISE_TEST_CHECK_EQUAL(paletteIndexForKey(QByteArrayView("x"),0),size_t(0)); // no entries
}

BOOST_AUTO_TEST_CASE(TestStyleShipsBothPalettesInBothThemes)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            auto& style=Style::instance();
            size_t senderCount=0;
            size_t avatarCount=0;

            for (auto mode : {Style::StyleSheetMode::Light,Style::StyleSheetMode::Dark})
            {
                style.setStyleSheetMode(mode);
                style.applyStyleSheet();

                auto sender=style.colorPalette(ColorPaletteNames::ChatSenderTitle);
                auto avatar=style.colorPalette(ColorPaletteNames::AvatarBackground);
                UISE_TEST_CHECK(!sender.empty());
                UISE_TEST_CHECK(!avatar.empty());

                // same length in both themes: a sender keeps their slot across a theme switch
                if (senderCount==0)
                {
                    senderCount=sender.size();
                    avatarCount=avatar.size();
                }
                UISE_TEST_CHECK_EQUAL(sender.size(),senderCount);
                UISE_TEST_CHECK_EQUAL(avatar.size(),avatarCount);
            }
            UISE_TEST_CHECK_EQUAL(avatarCount,size_t(15));

            style.setStyleSheetMode(Style::StyleSheetMode::Auto);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestPaletteColorIsStableAndWithinPalette)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            auto& style=Style::instance();
            style.applyStyleSheet();

            auto palette=style.colorPalette(ColorPaletteNames::ChatSenderTitle);
            UISE_TEST_REQUIRE(!palette.empty());

            auto a=style.paletteColor(ColorPaletteNames::ChatSenderTitle,QByteArrayView("sender-1"));
            auto b=style.paletteColor(ColorPaletteNames::ChatSenderTitle,QByteArrayView("sender-1"));
            UISE_TEST_REQUIRE(a.has_value());
            UISE_TEST_REQUIRE(b.has_value());
            UISE_TEST_CHECK(*a==*b);
            UISE_TEST_CHECK(std::find(palette.begin(),palette.end(),*a)!=palette.end());

            // many keys must spread over more than one slot, or the feature is pointless
            std::set<QRgb> seen;
            for (int i=0;i<64;i++)
            {
                auto c=style.paletteColor(ColorPaletteNames::ChatSenderTitle,
                                          QByteArray("sender-")+QByteArray::number(i));
                UISE_TEST_REQUIRE(c.has_value());
                seen.insert(c->rgb());
            }
            UISE_TEST_CHECK_GT(seen.size(),size_t(3));

            UISE_TEST_CHECK(!style.paletteColor(QStringLiteral("no-such-palette"),QByteArrayView("k")).has_value());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestPalettesChangedHandlerFiresOnReload)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            auto& style=Style::instance();
            int calls=0;
            auto id=style.addPalettesChangedHandler([&calls](){++calls;});

            style.applyStyleSheet();
            UISE_TEST_CHECK_EQUAL(calls,1);

            style.removePalettesChangedHandler(id);
            style.applyStyleSheet();
            UISE_TEST_CHECK_EQUAL(calls,1);
        }
    );
}

BOOST_AUTO_TEST_SUITE_END()
