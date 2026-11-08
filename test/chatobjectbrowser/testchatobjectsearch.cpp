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

/** @file uise/test/chatobjectbrowser/testchatobjectsearch.cpp
*
*  Test of the "search" kind of the chat object browsers: the highlight HTML of a snippet (escaping, merging and clamping of
*  the ranges, one-to-one replacement of whitespace), ChatObjectSearchSnippet, the row factory and the browser maker.
*
*/

/****************************************************************************/

#include <climits>
#include <memory>
#include <vector>

#include <QFrame>
#include <QLineEdit>
#include <QWidget>

#include <boost/test/unit_test.hpp>

#include <uise/test/uise-testthread.hpp>
#include <uise/desktop/chatobjectsearchsnippet.hpp>
#include <uise/desktop/chatobjectlistbrowser.hpp>
#include <uise/desktop/style.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

//--------------------------------------------------------------------------
// Strict inequalities are written as UISE_TEST_CHECK(a<b): UISE_TEST_CHECK_GT is wired to
// BOOST_CHECK_GE and there is no UISE_TEST_CHECK_LT at all.
//--------------------------------------------------------------------------

namespace {

const QColor TextColor(10,102,194);
const QColor BackColor(10,102,194,28);

using Ranges=std::vector<ChatObjectTextRange>;

//! A match as the function writes it with the two colours above.
QString span(const QString& escaped)
{
    return QStringLiteral("<span style=\"color:rgba(10,102,194,255);background-color:rgba(10,102,194,28);\">%1</span>").arg(escaped);
}

QString html(const QString& text, const Ranges& ranges)
{
    return uise::chatObjectHighlightHtml(text,ranges,TextColor,BackColor);
}

template <typename Fn>
void runInGui(Fn fn)
{
    auto handler=[fn]()
    {
        // the rows own avatars and the browsers a SearchLineEdit, which look their icons up when they are built
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

BOOST_AUTO_TEST_SUITE(TestChatObjectSearch)

BOOST_AUTO_TEST_CASE(TestHighlightNoRanges)
{
    // a pure function, no GUI object involved
    UISE_TEST_CHECK_EQUAL_QSTR(html(QStringLiteral("plain text"),{}),QStringLiteral("plain text"));
    UISE_TEST_CHECK_EQUAL_QSTR(html(QString{},{}),QString{});
    // nothing to highlight in nothing
    UISE_TEST_CHECK_EQUAL_QSTR(html(QString{},{{0,3}}),QString{});
}

BOOST_AUTO_TEST_CASE(TestHighlightEscaping)
{
    UISE_TEST_CHECK_EQUAL_QSTR(
        html(QStringLiteral("a<b && \"c\" > d"),{}),
        QStringLiteral("a&lt;b &amp;&amp; &quot;c&quot; &gt; d")
    );

    // the matched piece is escaped as well, and the text around it
    UISE_TEST_CHECK_EQUAL_QSTR(
        html(QStringLiteral("x<y&z"),{{1,3}}),
        QStringLiteral("x")+span(QStringLiteral("&lt;y&amp;"))+QStringLiteral("z")
    );

    // markup in the text is text
    UISE_TEST_CHECK_EQUAL_QSTR(
        html(QStringLiteral("<b>bold</b>"),{{3,4}}),
        QStringLiteral("&lt;b&gt;")+span(QStringLiteral("bold"))+QStringLiteral("&lt;/b&gt;")
    );
}

BOOST_AUTO_TEST_CASE(TestHighlightBasicAndMerging)
{
    UISE_TEST_CHECK_EQUAL_QSTR(
        html(QStringLiteral("hello big world"),{{6,3}}),
        QStringLiteral("hello ")+span(QStringLiteral("big"))+QStringLiteral(" world")
    );

    // two ranges apart
    UISE_TEST_CHECK_EQUAL_QSTR(
        html(QStringLiteral("one two three"),{{0,3},{8,5}}),
        span(QStringLiteral("one"))+QStringLiteral(" two ")+span(QStringLiteral("three"))
    );

    // adjacent ranges are one
    UISE_TEST_CHECK_EQUAL_QSTR(
        html(QStringLiteral("abcdef"),{{0,3},{3,3}}),
        span(QStringLiteral("abcdef"))
    );

    // overlapping ranges are one, whatever the order they come in
    UISE_TEST_CHECK_EQUAL_QSTR(
        html(QStringLiteral("abcdefg"),{{2,3},{0,3}}),
        span(QStringLiteral("abcde"))+QStringLiteral("fg")
    );

    // one inside another
    UISE_TEST_CHECK_EQUAL_QSTR(
        html(QStringLiteral("abcdefg"),{{1,5},{2,2}}),
        QStringLiteral("a")+span(QStringLiteral("bcdef"))+QStringLiteral("g")
    );

    // equal ranges
    UISE_TEST_CHECK_EQUAL_QSTR(
        html(QStringLiteral("abc"),{{1,1},{1,1}}),
        QStringLiteral("a")+span(QStringLiteral("b"))+QStringLiteral("c")
    );
}

BOOST_AUTO_TEST_CASE(TestHighlightOutOfRange)
{
    const QString text=QStringLiteral("abcdef");

    // empty and negative lengths are dropped
    UISE_TEST_CHECK_EQUAL_QSTR(html(text,{{2,0},{3,-1}}),text);

    // after the end, and at the end: dropped
    UISE_TEST_CHECK_EQUAL_QSTR(html(text,{{10,3},{6,2}}),text);

    // clamped: a start before the text, a length beyond it
    UISE_TEST_CHECK_EQUAL_QSTR(
        html(text,{{-2,4}}),
        span(QStringLiteral("ab"))+QStringLiteral("cdef")
    );
    UISE_TEST_CHECK_EQUAL_QSTR(
        html(text,{{4,100}}),
        QStringLiteral("abcd")+span(QStringLiteral("ef"))
    );

    // all of it in one list: valid, invalid, and a valid one again
    UISE_TEST_CHECK_EQUAL_QSTR(
        html(text,{{-2,4},{10,3},{2,0},{3,-1},{4,100}}),
        span(QStringLiteral("ab"))+QStringLiteral("cd")+span(QStringLiteral("ef"))
    );

    // numbers that would wrap around when added
    UISE_TEST_CHECK_EQUAL_QSTR(html(text,{{INT_MAX,INT_MAX}}),text);
    UISE_TEST_CHECK_EQUAL_QSTR(
        html(text,{{1,INT_MAX}}),
        QStringLiteral("a")+span(QStringLiteral("bcdef"))
    );
    UISE_TEST_CHECK_EQUAL_QSTR(html(text,{{INT_MIN,10}}),text);
    // INT_MIN+INT_MAX is -1: ends before the text
    UISE_TEST_CHECK_EQUAL_QSTR(html(text,{{INT_MIN,INT_MAX}}),text);
}

BOOST_AUTO_TEST_CASE(TestHighlightWhitespaceIsOneToOne)
{
    // a(0) \n(1) b(2) \t(3) c(4) \r(5) \n(6) d(7): "d" is at 7 and must stay there
    const QString text=QStringLiteral("a\nb\tc\r\nd");
    UISE_TEST_REQUIRE_EQUAL(static_cast<int>(text.size()),8);

    UISE_TEST_CHECK_EQUAL_QSTR(
        html(text,{{7,1}}),
        QStringLiteral("a b c  ")+span(QStringLiteral("d"))
    );

    // the line breaks themselves may be inside a match: they become spaces in it
    UISE_TEST_CHECK_EQUAL_QSTR(
        html(text,{{0,3}}),
        span(QStringLiteral("a b"))+QStringLiteral(" c  d")
    );

    // no range: the same replacement
    UISE_TEST_CHECK_EQUAL_QSTR(html(text,{}),QStringLiteral("a b c  d"));

    // the other line and paragraph separators, vertical tab and form feed
    const QString text2=QString(QStringLiteral("x"))+QChar(0x2028)+QChar(0x2029)+QLatin1Char('\v')+QLatin1Char('\f')+QStringLiteral("y");
    UISE_TEST_CHECK_EQUAL_QSTR(
        html(text2,{{5,1}}),
        QStringLiteral("x    ")+span(QStringLiteral("y"))
    );
}

BOOST_AUTO_TEST_CASE(TestHighlightKeepsSurrogatePairsWhole)
{
    // a(0) high(1) low(2) b(3): an emoji is two units
    const QString text=QStringLiteral("a")+QString::fromUcs4(U"\U0001F600")+QStringLiteral("b");
    UISE_TEST_REQUIRE_EQUAL(static_cast<int>(text.size()),4);
    const QString emoji=text.mid(1,2);

    // a range that starts or ends in the middle of the pair takes the whole pair
    UISE_TEST_CHECK_EQUAL_QSTR(html(text,{{2,1}}),QStringLiteral("a")+span(emoji)+QStringLiteral("b"));
    UISE_TEST_CHECK_EQUAL_QSTR(html(text,{{1,1}}),QStringLiteral("a")+span(emoji)+QStringLiteral("b"));
    UISE_TEST_CHECK_EQUAL_QSTR(html(text,{{0,2}}),span(QStringLiteral("a")+emoji)+QStringLiteral("b"));
}

BOOST_AUTO_TEST_CASE(TestHighlightColours)
{
    const QString text=QStringLiteral("abc");

    // without any colour (a host without the style sheet) a match is told by its weight
    UISE_TEST_CHECK_EQUAL_QSTR(
        uise::chatObjectHighlightHtml(text,{{1,1}},QColor{},QColor{}),
        QStringLiteral("a<span style=\"font-weight:600;\">b</span>c")
    );

    // one colour of the two
    UISE_TEST_CHECK_EQUAL_QSTR(
        uise::chatObjectHighlightHtml(text,{{1,1}},TextColor,QColor{}),
        QStringLiteral("a<span style=\"color:rgba(10,102,194,255);\">b</span>c")
    );
    UISE_TEST_CHECK_EQUAL_QSTR(
        uise::chatObjectHighlightHtml(text,{{1,1}},QColor{},BackColor),
        QStringLiteral("a<span style=\"background-color:rgba(10,102,194,28);\">b</span>c")
    );
}

BOOST_AUTO_TEST_CASE(TestSnippetWidget)
{
    runInGui(
        []()
        {
            uise::ChatObjectSearchSnippet snippet;
            UISE_TEST_CHECK(snippet.textFormat()==Qt::RichText);
            UISE_TEST_CHECK(snippet.wordWrap());

            // not taller than three lines, whatever the text
            UISE_TEST_CHECK_EQUAL(snippet.maximumHeight(),3*snippet.fontMetrics().lineSpacing()+2);

            // no colours yet: the weight tells the match
            snippet.setSnippet(QStringLiteral("a<b c"),{{2,1}});
            UISE_TEST_CHECK_EQUAL_QSTR(snippet.snippetText(),QStringLiteral("a<b c"));
            UISE_TEST_CHECK_EQUAL_QSTR(
                snippet.text(),
                uise::chatObjectHighlightHtml(QStringLiteral("a<b c"),{{2,1}},QColor{},QColor{})
            );
            UISE_TEST_CHECK(snippet.text().contains(QStringLiteral("font-weight:600")));

            // a property that changes renders the snippet again
            snippet.setHighlightColor(TextColor);
            UISE_TEST_CHECK_EQUAL_QSTR(snippet.text(),uise::chatObjectHighlightHtml(QStringLiteral("a<b c"),{{2,1}},TextColor,QColor{}));
            snippet.setHighlightBackground(BackColor);
            UISE_TEST_CHECK_EQUAL_QSTR(snippet.text(),html(QStringLiteral("a<b c"),{{2,1}}));
            UISE_TEST_CHECK(snippet.highlightColor()==TextColor);
            UISE_TEST_CHECK(snippet.highlightBackground()==BackColor);

            // the same value is not a change
            const auto before=snippet.text();
            snippet.setHighlightColor(TextColor);
            UISE_TEST_CHECK_EQUAL_QSTR(snippet.text(),before);

            // a new snippet with no ranges has no highlight
            snippet.setSnippet(QStringLiteral("other"),{});
            UISE_TEST_CHECK_EQUAL_QSTR(snippet.text(),QStringLiteral("other"));

            // the properties are those of the style sheet
            UISE_TEST_CHECK(snippet.property("highlightColor").isValid());
            UISE_TEST_CHECK(snippet.property("highlightBackground").isValid());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestSearchRowFactory)
{
    runInGui(
        []()
        {
            const auto factory=uise::makeChatObjectSearchRowFactory();
            UISE_TEST_REQUIRE(static_cast<bool>(factory.build));
            UISE_TEST_REQUIRE(static_cast<bool>(factory.update));
            UISE_TEST_REQUIRE(static_cast<bool>(factory.buildTitle));
            UISE_TEST_REQUIRE(static_cast<bool>(factory.updateTitle));

            ChatObjectItem item;
            item.kind=ChatObjectKind::Search;
            item.key.sortId=QByteArray("000001");
            item.senderName=QStringLiteral("Alice");
            item.text=QStringLiteral("…the report is\nready");
            item.matches={{5,6}};

            ChatObjectRowHooks hooks;
            std::unique_ptr<QFrame> row{factory.build(item,nullptr,hooks)};
            UISE_TEST_REQUIRE(row!=nullptr);
            UISE_TEST_CHECK_EQUAL_QSTR(row->objectName(),QStringLiteral("searchRow"));

            auto* snippet=row->findChild<uise::ChatObjectSearchSnippet*>("snippet");
            UISE_TEST_REQUIRE(snippet!=nullptr);
            UISE_TEST_CHECK_EQUAL_QSTR(snippet->snippetText(),item.text);
            UISE_TEST_CHECK(snippet->text().contains(QStringLiteral("report")));
            // the line break is a space of the same size: the ranges still count
            UISE_TEST_CHECK(!snippet->text().contains(QLatin1Char('\n')));

            // an update takes the new text and ranges
            item.text=QStringLiteral("new text");
            item.matches={{0,3}};
            factory.update(row.get(),item);
            UISE_TEST_CHECK_EQUAL_QSTR(snippet->snippetText(),QStringLiteral("new text"));

            std::unique_ptr<QWidget> title{factory.buildTitle(nullptr)};
            UISE_TEST_REQUIRE(title!=nullptr);
            factory.updateTitle(title.get(),item);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestSearchBrowserSettings)
{
    runInGui(
        []()
        {
            std::unique_ptr<uise::ChatObjectListBrowser> browser{uise::makeChatObjectSearchBrowser(nullptr)};
            UISE_TEST_REQUIRE(browser!=nullptr);

            UISE_TEST_CHECK_EQUAL_QSTR(browser->kind(),ChatObjectKind::Search);
            UISE_TEST_CHECK(!browser->isGroupByMonth());
            UISE_TEST_CHECK(browser->isSearchVisible());
            UISE_TEST_CHECK(browser->isQueryRequired());
            UISE_TEST_CHECK(!browser->emptyText().isEmpty());

            auto* edit=browser->findChild<QLineEdit*>("searchEdit");
            UISE_TEST_REQUIRE(edit!=nullptr);
            UISE_TEST_CHECK(!edit->placeholderText().isEmpty());
            // the placeholder is not the generic one
            UISE_TEST_CHECK(edit->placeholderText()!=QStringLiteral("Search"));

            // with no text nothing is asked for
            int requests=0;
            browser->setRequestCb(
                [&requests](const ChatObjectsRequest&)
                {
                    ++requests;
                }
            );
            browser->reload();
            UISE_TEST_CHECK_EQUAL(requests,0);
            UISE_TEST_CHECK(!browser->isLoading());

            // and with a text it is
            browser->setSearchText(QStringLiteral("rep"));
            UISE_TEST_CHECK_EQUAL(requests,1);
            UISE_TEST_CHECK(browser->isLoading());
        }
    );
}

BOOST_AUTO_TEST_SUITE_END()
