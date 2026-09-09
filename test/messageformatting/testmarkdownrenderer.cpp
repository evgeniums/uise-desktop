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

/** @file uise/test/messageformatting/testmarkdownrenderer.cpp
*
*  Tests markdownToHtml() and markdownToPlainText() (task-message-formatting-plan.md, Stage 2).
*
*  Assertions check for PROPERTIES of the emitted HTML (contains/does not contain a tag or
*  attribute, or a small set of structural counts) rather than exact-string equality -- an exact
*  match would be brittle across Qt versions, since this whole approach relies on Qt's own
*  markdown importer for parsing and only this file's own writer is actually under test.
*
*  Every call into markdownToHtml()/markdownToPlainText() is marshalled onto the GUI thread via
*  TestThread::execGuiThread() rather than called directly from the Boost test-case body: per
*  test/inc/uise/test/uise-testwrapper.hpp's own runTest(), the QApplication lives on the process's
*  main thread but boost::unit_test_main() (and therefore every BOOST_AUTO_TEST_CASE body) runs on
*  a SEPARATE TestThread -- fine for a pure QString function like substituteColors(), but
*  markdownToHtml() constructs and lays out a QTextDocument, which is safest kept on the same
*  thread as the QApplication instance rather than risk Qt's font/text machinery being touched
*  concurrently from two threads.
*
*/

/****************************************************************************/

#include <boost/test/unit_test.hpp>

#include <QTextDocument>
#include <QTextBlock>
#include <QTextFormat>

#include <uise/test/uise-testthread.hpp>
#include <uise/desktop/markdownrenderer.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

namespace {

QString renderMd(const QString& src, const MarkdownRenderOptions& options=MarkdownRenderOptions{})
{
    QString result;
    TestThread::instance()->execGuiThread(
        [&]()
        {
            result=markdownToHtml(src,options);
        }
    );
    return result;
}

QString stripMd(const QString& src, int maxSourceLength=4096)
{
    QString result;
    TestThread::instance()->execGuiThread(
        [&]()
        {
            result=markdownToPlainText(src,maxSourceLength);
        }
    );
    return result;
}

}

BOOST_AUTO_TEST_SUITE(TestMarkdownRenderer)

BOOST_AUTO_TEST_CASE(TestEmptySource)
{
    auto html=renderMd(QString());
    UISE_TEST_CHECK(!html.contains(QStringLiteral("<script")));
}

BOOST_AUTO_TEST_CASE(TestParagraphEscaping)
{
    auto html=renderMd(QStringLiteral("5 < 10 & \"quoted\""));
    UISE_TEST_CHECK(html.contains(QStringLiteral("&lt;")));
    UISE_TEST_CHECK(html.contains(QStringLiteral("&amp;")));
    UISE_TEST_CHECK(!html.contains(QStringLiteral("5 < 10")));
}

BOOST_AUTO_TEST_CASE(TestLineBreakBetweenLinesStaysOneParagraph)
{
    auto html=renderMd(QStringLiteral("line one\nline two"));
    UISE_TEST_CHECK(html.contains(QStringLiteral("<br/>")));
    UISE_TEST_CHECK(html.contains(QStringLiteral("line one")));
    UISE_TEST_CHECK(html.contains(QStringLiteral("line two")));
    // The whole point of the sentinel mechanism (see preserveChatLineBreaks()'s own comment):
    // two typed lines must render as ONE <p>, not two -- Qt's markdown importer has no way to
    // represent an in-paragraph line break as anything other than a new block, so a naive
    // approach (CommonMark hard-break syntax) would fail this exact assertion.
    UISE_TEST_CHECK_EQUAL(html.count(QStringLiteral("<p>")),1);
}

BOOST_AUTO_TEST_CASE(TestLineBreakSkippedInsideFence)
{
    auto html=renderMd(QStringLiteral("```\nint a;\nint b;\n```"));
    // Inside the fence, lines are joined by a plain '\n' inside <pre><code>, never by <br/> --
    // a <br/> there would inject an actual tag into a compiled program's text.
    auto preStart=html.indexOf(QStringLiteral("<pre"));
    auto preEnd=html.indexOf(QStringLiteral("</pre>"));
    UISE_TEST_REQUIRE(preStart>=0);
    UISE_TEST_REQUIRE(preEnd>preStart);
    auto preContent=html.mid(preStart,preEnd-preStart);
    UISE_TEST_CHECK(!preContent.contains(QStringLiteral("<br/>")));
    UISE_TEST_CHECK(preContent.contains(QStringLiteral("int a;")));
    UISE_TEST_CHECK(preContent.contains(QStringLiteral("int b;")));
}

BOOST_AUTO_TEST_CASE(TestLineBreakSkippedInsideTable)
{
    auto html=renderMd(QStringLiteral("| A | B |\n|---|---|\n| 1 | 2 |"));
    UISE_TEST_CHECK(html.contains(QStringLiteral("<table>")));
}

BOOST_AUTO_TEST_CASE(TestHeadings)
{
    for (int level=1;level<=6;++level)
    {
        QString hashes(level,QLatin1Char('#'));
        auto html=renderMd(hashes+QStringLiteral(" Heading"));
        auto openTag=QStringLiteral("<h%1>").arg(level);
        UISE_TEST_CHECK(html.contains(openTag));
    }
}

BOOST_AUTO_TEST_CASE(TestBoldItalicStrike)
{
    auto html=renderMd(QStringLiteral("**bold** *italic* ~~strike~~"));
    UISE_TEST_CHECK(html.contains(QStringLiteral("<b>")));
    UISE_TEST_CHECK(html.contains(QStringLiteral("<i>")));
    UISE_TEST_CHECK(html.contains(QStringLiteral("<s>")));
}

BOOST_AUTO_TEST_CASE(TestUnderlineNotDoubledInsideAnchor)
{
    // A bare autolink is underlined by ChatMessageTextBrowser's own linkColor/linkUnderline QSS
    // rule -- this renderer must not ALSO wrap it in <u>, which would fight that rule.
    auto html=renderMd(QStringLiteral("https://example.com"));
    auto anchorStart=html.indexOf(QStringLiteral("<a "));
    auto anchorEnd=html.indexOf(QStringLiteral("</a>"));
    UISE_TEST_REQUIRE(anchorStart>=0);
    UISE_TEST_REQUIRE(anchorEnd>anchorStart);
    auto anchorContent=html.mid(anchorStart,anchorEnd-anchorStart);
    UISE_TEST_CHECK(!anchorContent.contains(QStringLiteral("<u>")));
}

BOOST_AUTO_TEST_CASE(TestInlineCodePreservesAngleBracketsAndAmpersand)
{
    auto html=renderMd(QStringLiteral("`if (a < b && c)`"));
    UISE_TEST_CHECK(html.contains(QStringLiteral("<code>")));
    UISE_TEST_CHECK(html.contains(QStringLiteral("&lt;")));
    UISE_TEST_CHECK(html.contains(QStringLiteral("&amp;")));
    UISE_TEST_CHECK(!html.contains(QStringLiteral("< b")));
}

BOOST_AUTO_TEST_CASE(TestFencedBlockLanguageClassIsOnPre)
{
    auto html=renderMd(QStringLiteral("```cpp\nint a=1;\n```"));
    // Deliberately on <pre>, not <code> -- see writeCodeLine()'s own comment: Qt's HTML parser
    // only reads "class=language-x" on Html_pre.
    UISE_TEST_CHECK(html.contains(QStringLiteral("<pre class=\"language-cpp\">")));
    UISE_TEST_CHECK(!html.contains(QStringLiteral("<code class=")));
}

BOOST_AUTO_TEST_CASE(TestCodeLanguageRoundTripsThroughSetHtml)
{
    // Pins the load-bearing fact behind markdownrenderer.hpp's own doc comment: Stage 3's
    // syntax highlighter can recover a code block's language straight off the LIVE rendered
    // document, with no need for ChatMessageText's separately cached markdown source.
    auto html=renderMd(QStringLiteral("```cpp\nint a=1;\n```"));

    QString language;
    TestThread::instance()->execGuiThread(
        [&]()
        {
            QTextDocument doc;
            doc.setHtml(html);
            for (auto block=doc.begin(); block!=doc.end(); block=block.next())
            {
                auto fmt=block.blockFormat();
                if (fmt.hasProperty(QTextFormat::BlockCodeLanguage))
                {
                    language=fmt.stringProperty(QTextFormat::BlockCodeLanguage);
                    break;
                }
            }
        }
    );
    UISE_TEST_CHECK_EQUAL_QSTR(language,QStringLiteral("cpp"));
}

BOOST_AUTO_TEST_CASE(TestScriptInsideFenceEscaped)
{
    auto html=renderMd(QStringLiteral("```\n<script>alert(1)</script>\n```"));
    UISE_TEST_CHECK(!html.contains(QStringLiteral("<script>")));
    UISE_TEST_CHECK(html.contains(QStringLiteral("&lt;script&gt;")));
}

BOOST_AUTO_TEST_CASE(TestIndentedCodeBlock)
{
    auto html=renderMd(QStringLiteral("Paragraph.\n\n    indented code\n    second line\n"));
    UISE_TEST_CHECK(html.contains(QStringLiteral("<pre")));
    UISE_TEST_CHECK(html.contains(QStringLiteral("indented code")));
}

BOOST_AUTO_TEST_CASE(TestBulletList)
{
    auto html=renderMd(QStringLiteral("- one\n- two\n- three"));
    UISE_TEST_CHECK(html.contains(QStringLiteral("<ul>")));
    UISE_TEST_CHECK(html.contains(QStringLiteral("<li>")));
    UISE_TEST_CHECK(!html.contains(QStringLiteral("<ol")));
}

BOOST_AUTO_TEST_CASE(TestOrderedList)
{
    auto html=renderMd(QStringLiteral("1. one\n2. two\n3. three"));
    UISE_TEST_CHECK(html.contains(QStringLiteral("<ol")));
    UISE_TEST_CHECK(!html.contains(QStringLiteral("<ul>")));
}

BOOST_AUTO_TEST_CASE(TestNestedLists)
{
    auto html=renderMd(QStringLiteral("- one\n  - nested\n- two"));
    // At least two <ul> opens (outer + nested), correctly balanced by </ul> closes.
    auto openCount=html.count(QStringLiteral("<ul>"));
    auto closeCount=html.count(QStringLiteral("</ul>"));
    UISE_TEST_CHECK_GE(openCount,2);
    UISE_TEST_CHECK_EQUAL(openCount,closeCount);
}

BOOST_AUTO_TEST_CASE(TestTaskList)
{
    auto html=renderMd(QStringLiteral("- [ ] todo\n- [x] done"));
    UISE_TEST_CHECK(html.contains(QChar(0x2610)));
    UISE_TEST_CHECK(html.contains(QChar(0x2612)));
}

BOOST_AUTO_TEST_CASE(TestBlockquoteAndNested)
{
    auto html=renderMd(QStringLiteral("> level one\n> > level two"));
    auto openCount=html.count(QStringLiteral("<blockquote>"));
    auto closeCount=html.count(QStringLiteral("</blockquote>"));
    UISE_TEST_CHECK_GE(openCount,2);
    UISE_TEST_CHECK_EQUAL(openCount,closeCount);
}

BOOST_AUTO_TEST_CASE(TestThematicBreak)
{
    auto html=renderMd(QStringLiteral("above\n\n---\n\nbelow"));
    UISE_TEST_CHECK(html.contains(QStringLiteral("<hr/>")));
}

BOOST_AUTO_TEST_CASE(TestTableWithHeaderAndRows)
{
    auto html=renderMd(QStringLiteral("| A | B |\n|---|---|\n| 1 | 2 |\n| 3 | 4 |"));
    UISE_TEST_CHECK(html.contains(QStringLiteral("<table>")));
    UISE_TEST_CHECK(html.contains(QStringLiteral("<th>")));
    UISE_TEST_CHECK(html.contains(QStringLiteral("<td>")));
    auto rowCount=html.count(QStringLiteral("<tr>"));
    UISE_TEST_CHECK_EQUAL(rowCount,3); // header + two data rows
}

BOOST_AUTO_TEST_CASE(TestExplicitLinkNotDoubleLinkified)
{
    auto html=renderMd(QStringLiteral("[Example](https://example.com)"));
    auto anchorCount=html.count(QStringLiteral("<a "));
    UISE_TEST_CHECK_EQUAL(anchorCount,1);
    UISE_TEST_CHECK(html.contains(QStringLiteral("Example")));
    UISE_TEST_CHECK(!html.contains(QStringLiteral("https://example.com<")));
}

BOOST_AUTO_TEST_CASE(TestBareUrlAndWwwAutolinked)
{
    auto html=renderMd(QStringLiteral("visit https://example.com or www.example.org"));
    UISE_TEST_CHECK_EQUAL(html.count(QStringLiteral("<a ")),2);
}

BOOST_AUTO_TEST_CASE(TestEmailAutolinked)
{
    auto html=renderMd(QStringLiteral("contact me@example.com"));
    UISE_TEST_CHECK(html.contains(QStringLiteral("<a ")));
    UISE_TEST_CHECK(html.contains(QStringLiteral("mailto:")));
}

BOOST_AUTO_TEST_CASE(TestJavascriptSchemeDegradedToText)
{
    auto html=renderMd(QStringLiteral("[bad](javascript:alert(1))"));
    UISE_TEST_CHECK(!html.contains(QStringLiteral("javascript:")));
    UISE_TEST_CHECK(!html.contains(QStringLiteral("<a ")));
}

BOOST_AUTO_TEST_CASE(TestCustomSchemeAllowedWhenAdded)
{
    // Forward-compatibility check for Stage 6 (mentions), which adds its own scheme to a
    // caller-supplied copy of the allowlist.
    MarkdownRenderOptions options;
    options.allowedLinkSchemes<<QStringLiteral("whitem-mention");
    auto html=renderMd(QStringLiteral("[User](whitem-mention:12345)"),options);
    UISE_TEST_CHECK(html.contains(QStringLiteral("<a ")));
    UISE_TEST_CHECK(html.contains(QStringLiteral("whitem-mention:12345")));
}

BOOST_AUTO_TEST_CASE(TestRawScriptTagNeutralized)
{
    auto html=renderMd(QStringLiteral("hello <script>alert(1)</script> world"));
    UISE_TEST_CHECK(!html.contains(QStringLiteral("<script>")));
    UISE_TEST_CHECK(!html.contains(QStringLiteral("<script")));
}

BOOST_AUTO_TEST_CASE(TestMarkdownImageProducesNoImgTag)
{
    auto html=renderMd(QStringLiteral("![alt text](https://example.com/pic.png)"));
    UISE_TEST_CHECK(!html.contains(QStringLiteral("<img")));
    UISE_TEST_CHECK(html.contains(QStringLiteral("alt text")));
}

BOOST_AUTO_TEST_CASE(TestAnchorBudgetExceeded)
{
    MarkdownRenderOptions options;
    options.maxAnchors=2;
    QString src;
    for (int i=0;i<5;++i)
    {
        src+=QStringLiteral("https://example.com/%1 ").arg(i);
    }
    auto html=renderMd(src,options);
    UISE_TEST_CHECK_EQUAL(html.count(QStringLiteral("<a ")),2);
}

BOOST_AUTO_TEST_CASE(TestBacktickEscapedMentionPrintedLiterally)
{
    auto html=renderMd(QStringLiteral("`@user` printed literally"));
    UISE_TEST_CHECK(html.contains(QStringLiteral("@user")));
    UISE_TEST_CHECK(!html.contains(QStringLiteral("<a ")));
}

BOOST_AUTO_TEST_CASE(TestMarkdownToPlainTextStripsSyntax)
{
    auto text=stripMd(QStringLiteral("**bold** and *italic* and `code`"));
    UISE_TEST_CHECK(!text.contains(QLatin1Char('*')));
    UISE_TEST_CHECK(!text.contains(QLatin1Char('`')));
    UISE_TEST_CHECK(text.contains(QStringLiteral("bold")));
    UISE_TEST_CHECK(text.contains(QStringLiteral("italic")));
    UISE_TEST_CHECK(text.contains(QStringLiteral("code")));
}

BOOST_AUTO_TEST_CASE(TestMarkdownToPlainTextRespectsSourceCap)
{
    QString src(500,QLatin1Char('a'));
    auto text=stripMd(src,100);
    UISE_TEST_CHECK_LE(text.size(),100);
}

BOOST_AUTO_TEST_SUITE_END()
