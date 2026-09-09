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

/** @file uise/test/messageformatting/testsyntaxhighlighter.cpp
*
*  Tests SyntaxLanguageRegistry, tokenizeSyntaxLine() and SyntaxHighlighter
*  (task-message-formatting-plan.md, Stage 3).
*
*  Split into two groups, matching the split in the production code (syntaxtokenizer.cpp is a
*  pure function, no QTextDocument/QApplication involved at all):
*   - Pure tokenizer cases run DIRECTLY on this file's own BOOST_AUTO_TEST_CASE body (the separate
*     TestThread Boost itself runs on -- see testmarkdownrenderer.cpp's own header comment for why
*     that thread split exists at all) -- QString/QStringView/std::vector have no GUI-thread
*     affinity, unlike markdownToHtml()'s QTextDocument construction.
*   - Document-integration cases (QTextDocument, SyntaxHighlighter attached to one, a live
*     ChatMessageTextBrowser) ARE marshalled through TestThread::execGuiThread(), same as every
*     case in testmarkdownrenderer.cpp.
*
*/

/****************************************************************************/

#include <boost/test/unit_test.hpp>

#include <QCoreApplication>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include <QTextFormat>
#include <QTextTable>
#include <QTextFrame>
#include <QTextCursor>
#include <QAbstractTextDocumentLayout>
#include <QScrollBar>
#include <QMimeData>

#include <uise/test/uise-testthread.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/markdownrenderer.hpp>
#include <uise/desktop/syntaxtheme.hpp>
#include <uise/desktop/syntaxlanguage.hpp>
#include <uise/desktop/syntaxtokenizer.hpp>
#include <uise/desktop/syntaxhighlighter.hpp>
#include <uise/desktop/chatmessagetext.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

BOOST_AUTO_TEST_SUITE(TestSyntaxHighlighter)

/****************************** pure tokenizer cases ******************************/

BOOST_AUTO_TEST_CASE(TestEmptyLine)
{
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    std::vector<SyntaxSpan> spans;
    auto state=tokenizeSyntaxLine(cpp,QString{},SyntaxNoCodeState,spans);
    UISE_TEST_CHECK(spans.empty());
    UISE_TEST_CHECK_EQUAL(state,SyntaxNoCodeState);
}

BOOST_AUTO_TEST_CASE(TestWhitespaceOnlyLine)
{
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    auto spans=tokenizeSyntaxLine(cpp,QStringLiteral("     "));
    UISE_TEST_CHECK(spans.empty());
}

BOOST_AUTO_TEST_CASE(TestPunctuationOnlyLine)
{
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    auto spans=tokenizeSyntaxLine(cpp,QStringLiteral("(); {} [] ="));
    UISE_TEST_CHECK(spans.empty());
}

BOOST_AUTO_TEST_CASE(TestKeywordBucket)
{
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    auto spans=tokenizeSyntaxLine(cpp,QStringLiteral("if"));
    UISE_TEST_REQUIRE(spans.size()==1);
    UISE_TEST_CHECK(spans[0].bucket==SyntaxBucket::Keyword);
    UISE_TEST_CHECK_EQUAL(spans[0].start,0);
    UISE_TEST_CHECK_EQUAL(spans[0].length,2);
}

BOOST_AUTO_TEST_CASE(TestTypeBucket)
{
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    auto spans=tokenizeSyntaxLine(cpp,QStringLiteral("int"));
    UISE_TEST_REQUIRE(spans.size()==1);
    UISE_TEST_CHECK(spans[0].bucket==SyntaxBucket::Type);
}

BOOST_AUTO_TEST_CASE(TestLiteralBucketWord)
{
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    auto spans=tokenizeSyntaxLine(cpp,QStringLiteral("true"));
    UISE_TEST_REQUIRE(spans.size()==1);
    UISE_TEST_CHECK(spans[0].bucket==SyntaxBucket::Literal);
}

BOOST_AUTO_TEST_CASE(TestCallableBucketFromTable)
{
    // "printf" is a cpp _builtin word (-> Callable, decision 2), found even with NO trailing
    // '(' -- distinct from the `(`-callable HEURISTIC exercised by the next few cases below.
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    auto spans=tokenizeSyntaxLine(cpp,QStringLiteral("printf"));
    UISE_TEST_REQUIRE(spans.size()==1);
    UISE_TEST_CHECK(spans[0].bucket==SyntaxBucket::Callable);
}

BOOST_AUTO_TEST_CASE(TestPreprocessorOtherMapsToKeyword)
{
    // cpp's upstream _other table (preprocessor directives) collapses to Keyword (decision 2).
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    auto spans=tokenizeSyntaxLine(cpp,QStringLiteral("#include <stdio.h>"));
    bool foundIncludeKeyword=false;
    for (const auto& s : spans)
    {
        if (s.bucket==SyntaxBucket::Keyword && s.start==1 && s.length==7)
        {
            foundIncludeKeyword=true;
        }
    }
    UISE_TEST_CHECK(foundIncludeKeyword);
}

BOOST_AUTO_TEST_CASE(TestWordBoundaries)
{
    // "interface" must not match "int"; nor must an identifier that merely CONTAINS "int" as a
    // substring at either end -- upstream's own boundary bug class (qsourcehighliter.cpp).
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    UISE_TEST_CHECK(tokenizeSyntaxLine(cpp,QStringLiteral("interface")).empty());
    UISE_TEST_CHECK(tokenizeSyntaxLine(cpp,QStringLiteral("_int")).empty());
    UISE_TEST_CHECK(tokenizeSyntaxLine(cpp,QStringLiteral("int_t")).empty());
}

BOOST_AUTO_TEST_CASE(TestPrecedenceHashInsidePythonStringStaysLiteral)
{
    // The upstream bug class this scanner avoids by construction: a '#' inside a string must
    // never be read as a comment start.
    const auto& python=*SyntaxLanguageRegistry::instance().find(QStringLiteral("python"));
    auto line=QStringLiteral("x = \"# not a comment\"");
    auto spans=tokenizeSyntaxLine(python,line);
    bool sawComment=false;
    bool sawWholeStringAsLiteral=false;
    for (const auto& s : spans)
    {
        if (s.bucket==SyntaxBucket::Comment)
        {
            sawComment=true;
        }
        // The quoted string runs from the opening '"' (index 4) to the end of the line.
        if (s.bucket==SyntaxBucket::Literal && s.start==4 && s.start+s.length==line.size())
        {
            sawWholeStringAsLiteral=true;
        }
    }
    UISE_TEST_CHECK(!sawComment);
    UISE_TEST_CHECK(sawWholeStringAsLiteral);
}

BOOST_AUTO_TEST_CASE(TestPrecedenceUrlInCppStringNotComment)
{
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    auto spans=tokenizeSyntaxLine(cpp,QStringLiteral("auto s = \"http://example.com\";"));
    for (const auto& s : spans)
    {
        UISE_TEST_CHECK(s.bucket!=SyntaxBucket::Comment);
    }
}

BOOST_AUTO_TEST_CASE(TestPrecedenceKeywordInsideBlockCommentStaysComment)
{
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    auto line=QStringLiteral("/* if this were code */");
    auto spans=tokenizeSyntaxLine(cpp,line);
    UISE_TEST_REQUIRE(spans.size()==1);
    UISE_TEST_CHECK(spans[0].bucket==SyntaxBucket::Comment);
    UISE_TEST_CHECK_EQUAL(spans[0].start,0);
    UISE_TEST_CHECK_EQUAL(spans[0].length,line.size());
}

BOOST_AUTO_TEST_CASE(TestCallableHeuristicParen)
{
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    auto spans=tokenizeSyntaxLine(cpp,QStringLiteral("foo("));
    UISE_TEST_REQUIRE(spans.size()==1);
    UISE_TEST_CHECK(spans[0].bucket==SyntaxBucket::Callable);
}

BOOST_AUTO_TEST_CASE(TestCallableHeuristicSpaceParen)
{
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    auto spans=tokenizeSyntaxLine(cpp,QStringLiteral("foo ("));
    UISE_TEST_REQUIRE(spans.size()==1);
    UISE_TEST_CHECK(spans[0].bucket==SyntaxBucket::Callable);
}

BOOST_AUTO_TEST_CASE(TestCallableHeuristicNoParenIsPlainText)
{
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    UISE_TEST_CHECK(tokenizeSyntaxLine(cpp,QStringLiteral("foo")).empty());
}

BOOST_AUTO_TEST_CASE(TestKeywordNotOverriddenByCallableHeuristic)
{
    // "if" is a table hit -- must stay Keyword, never fall through to the `(`-heuristic.
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    auto spans=tokenizeSyntaxLine(cpp,QStringLiteral("if ("));
    UISE_TEST_REQUIRE(spans.size()==1);
    UISE_TEST_CHECK(spans[0].bucket==SyntaxBucket::Keyword);
}

BOOST_AUTO_TEST_CASE(TestNumberHex)
{
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    auto spans=tokenizeSyntaxLine(cpp,QStringLiteral("0x1F"));
    UISE_TEST_REQUIRE(spans.size()==1);
    UISE_TEST_CHECK(spans[0].bucket==SyntaxBucket::Literal);
    UISE_TEST_CHECK_EQUAL(spans[0].length,4);
}

BOOST_AUTO_TEST_CASE(TestNumberExponent)
{
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    auto spans=tokenizeSyntaxLine(cpp,QStringLiteral("1e-9"));
    UISE_TEST_REQUIRE(spans.size()==1);
    UISE_TEST_CHECK_EQUAL(spans[0].length,4);
}

BOOST_AUTO_TEST_CASE(TestNumberUnderscoreGroup)
{
    const auto& rust=*SyntaxLanguageRegistry::instance().find(QStringLiteral("rust"));
    auto spans=tokenizeSyntaxLine(rust,QStringLiteral("1_000"));
    UISE_TEST_REQUIRE(spans.size()==1);
    UISE_TEST_CHECK(spans[0].bucket==SyntaxBucket::Literal);
    UISE_TEST_CHECK_EQUAL(spans[0].length,5);
}

BOOST_AUTO_TEST_CASE(TestNumberSuffixFloat)
{
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    auto spans=tokenizeSyntaxLine(cpp,QStringLiteral("3.14f"));
    UISE_TEST_REQUIRE(spans.size()==1);
    UISE_TEST_CHECK_EQUAL(spans[0].length,5);
}

BOOST_AUTO_TEST_CASE(TestIdentifierStartingWithLetterNotMisreadAsNumber)
{
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    // "a1" starts with a letter -- must go through the identifier path, not the number path.
    // Not in the table and not followed by '(' -- no span at all is the correct outcome.
    UISE_TEST_CHECK(tokenizeSyntaxLine(cpp,QStringLiteral("a1")).empty());
}

BOOST_AUTO_TEST_CASE(TestStringWithEscapedQuote)
{
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    auto line=QStringLiteral(R"("a\"b")");
    auto spans=tokenizeSyntaxLine(cpp,line);
    UISE_TEST_REQUIRE(spans.size()==1);
    UISE_TEST_CHECK(spans[0].bucket==SyntaxBucket::Literal);
    UISE_TEST_CHECK_EQUAL(spans[0].start,0);
    UISE_TEST_CHECK_EQUAL(spans[0].length,line.size());
}

BOOST_AUTO_TEST_CASE(TestUnterminatedSingleLineStringEndsAtLineEndNoContinuation)
{
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    std::vector<SyntaxSpan> spans;
    auto state=tokenizeSyntaxLine(cpp,QStringLiteral("\"abc"),SyntaxNoCodeState,spans);
    UISE_TEST_REQUIRE(spans.size()==1);
    UISE_TEST_CHECK(spans[0].bucket==SyntaxBucket::Literal);
    UISE_TEST_CHECK_EQUAL(spans[0].length,4);
    // Deliberate scope limit (see syntaxtokenizer.cpp's own comment on scanQuotedString()): an
    // ordinary quoted string never carries a continuation into the next line.
    UISE_TEST_CHECK_EQUAL(state,SyntaxNoCodeState);
}

BOOST_AUTO_TEST_CASE(TestStateChainingBlockCommentAcrossThreeLines)
{
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));

    std::vector<SyntaxSpan> out1;
    auto state1=tokenizeSyntaxLine(cpp,QStringLiteral("/* start"),SyntaxNoCodeState,out1);
    UISE_TEST_REQUIRE(out1.size()==1);
    UISE_TEST_CHECK(out1[0].bucket==SyntaxBucket::Comment);
    UISE_TEST_CHECK(state1!=SyntaxNoCodeState);

    std::vector<SyntaxSpan> out2;
    auto state2=tokenizeSyntaxLine(cpp,QStringLiteral("middle"),state1,out2);
    UISE_TEST_REQUIRE(out2.size()==1);
    UISE_TEST_CHECK(out2[0].bucket==SyntaxBucket::Comment);
    UISE_TEST_CHECK_EQUAL(out2[0].length,6);
    UISE_TEST_CHECK(state2!=SyntaxNoCodeState);

    std::vector<SyntaxSpan> out3;
    auto line3=QStringLiteral("end */ code();");
    auto state3=tokenizeSyntaxLine(cpp,line3,state2,out3);
    UISE_TEST_CHECK_EQUAL(state3,SyntaxNoCodeState);
    bool sawCommentClose=false;
    bool sawCallable=false;
    for (const auto& s : out3)
    {
        if (s.bucket==SyntaxBucket::Comment && s.start==0 && s.length==6) // "end */"
        {
            sawCommentClose=true;
        }
        if (s.bucket==SyntaxBucket::Callable)
        {
            sawCallable=true;
        }
    }
    UISE_TEST_CHECK(sawCommentClose);
    UISE_TEST_CHECK(sawCallable);
}

BOOST_AUTO_TEST_CASE(TestRustNestedBlockCommentStaysOpenAtOuterLevel)
{
    const auto& rust=*SyntaxLanguageRegistry::instance().find(QStringLiteral("rust"));
    auto line=QStringLiteral("/* outer /* inner */ still outer");
    std::vector<SyntaxSpan> spans;
    auto state=tokenizeSyntaxLine(rust,line,SyntaxNoCodeState,spans);
    // Only the INNER "*/" closed -- the outer level is still open, so the whole line stays
    // Comment and a continuation carries into the next line.
    UISE_TEST_REQUIRE(spans.size()==1);
    UISE_TEST_CHECK(spans[0].bucket==SyntaxBucket::Comment);
    UISE_TEST_CHECK_EQUAL(spans[0].length,line.size());
    UISE_TEST_CHECK(state!=SyntaxNoCodeState);

    std::vector<SyntaxSpan> spans2;
    auto state2=tokenizeSyntaxLine(rust,QStringLiteral("*/ code();"),state,spans2);
    UISE_TEST_CHECK_EQUAL(state2,SyntaxNoCodeState);
    bool sawClose=false;
    for (const auto& s : spans2)
    {
        if (s.bucket==SyntaxBucket::Comment)
        {
            sawClose=true;
        }
    }
    UISE_TEST_CHECK(sawClose);
}

BOOST_AUTO_TEST_CASE(TestPythonTripleQuoteAcrossLines)
{
    const auto& python=*SyntaxLanguageRegistry::instance().find(QStringLiteral("python"));

    std::vector<SyntaxSpan> out1;
    auto state1=tokenizeSyntaxLine(python,QStringLiteral("\"\"\"start"),SyntaxNoCodeState,out1);
    UISE_TEST_REQUIRE(out1.size()==1);
    UISE_TEST_CHECK(out1[0].bucket==SyntaxBucket::Literal);
    UISE_TEST_CHECK(state1!=SyntaxNoCodeState);

    std::vector<SyntaxSpan> out2;
    auto state2=tokenizeSyntaxLine(python,QStringLiteral("middle"),state1,out2);
    UISE_TEST_REQUIRE(out2.size()==1);
    UISE_TEST_CHECK(out2[0].bucket==SyntaxBucket::Literal);
    UISE_TEST_CHECK(state2!=SyntaxNoCodeState);

    std::vector<SyntaxSpan> out3;
    auto state3=tokenizeSyntaxLine(python,QStringLiteral("end\"\"\""),state2,out3);
    UISE_TEST_REQUIRE(out3.size()==1);
    UISE_TEST_CHECK(out3[0].bucket==SyntaxBucket::Literal);
    UISE_TEST_CHECK_EQUAL(state3,SyntaxNoCodeState);
}

BOOST_AUTO_TEST_CASE(TestPythonTripleQuoteSameLineOpenAndClose)
{
    const auto& python=*SyntaxLanguageRegistry::instance().find(QStringLiteral("python"));
    std::vector<SyntaxSpan> spans;
    auto state=tokenizeSyntaxLine(python,QStringLiteral("\"\"\"one line\"\"\""),SyntaxNoCodeState,spans);
    UISE_TEST_REQUIRE(spans.size()==1);
    UISE_TEST_CHECK(spans[0].bucket==SyntaxBucket::Literal);
    UISE_TEST_CHECK_EQUAL(state,SyntaxNoCodeState);
}

BOOST_AUTO_TEST_CASE(TestSingleQuoteInsideTripleQuoteDoesNotCrossTerminate)
{
    const auto& python=*SyntaxLanguageRegistry::instance().find(QStringLiteral("python"));
    auto line=QStringLiteral("'''triple' still open");
    std::vector<SyntaxSpan> spans;
    auto state=tokenizeSyntaxLine(python,line,SyntaxNoCodeState,spans);
    UISE_TEST_REQUIRE(spans.size()==1);
    UISE_TEST_CHECK_EQUAL(spans[0].length,line.size());
    UISE_TEST_CHECK(state!=SyntaxNoCodeState);
}

BOOST_AUTO_TEST_CASE(TestLanguageResetAcrossDifferentLanguages)
{
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    const auto& python=*SyntaxLanguageRegistry::instance().find(QStringLiteral("python"));

    std::vector<SyntaxSpan> cppOut;
    auto cppState=tokenizeSyntaxLine(cpp,QStringLiteral("/* unterminated"),SyntaxNoCodeState,cppOut);
    UISE_TEST_CHECK(cppState!=SyntaxNoCodeState);

    // Feeding cpp's own in-comment state to a PYTHON line must be treated as fresh, not as a
    // continued comment -- "def" must highlight normally as a keyword.
    std::vector<SyntaxSpan> pyOut;
    tokenizeSyntaxLine(python,QStringLiteral("def f():"),cppState,pyOut);
    bool sawDefKeyword=false;
    bool sawComment=false;
    for (const auto& s : pyOut)
    {
        if (s.bucket==SyntaxBucket::Keyword && s.start==0 && s.length==3)
        {
            sawDefKeyword=true;
        }
        if (s.bucket==SyntaxBucket::Comment)
        {
            sawComment=true;
        }
    }
    UISE_TEST_CHECK(sawDefKeyword);
    UISE_TEST_CHECK(!sawComment);
}

BOOST_AUTO_TEST_CASE(TestSqlCaseInsensitiveKeywords)
{
    const auto& sql=*SyntaxLanguageRegistry::instance().find(QStringLiteral("sql"));
    for (const auto& word : {QStringLiteral("select"),QStringLiteral("SELECT"),QStringLiteral("SeLeCt")})
    {
        auto spans=tokenizeSyntaxLine(sql,word);
        UISE_TEST_REQUIRE(spans.size()==1);
        UISE_TEST_CHECK(spans[0].bucket==SyntaxBucket::Keyword);
    }
}

BOOST_AUTO_TEST_CASE(TestGenericFallbackNoKeywordsButNumbersAndCommentsWork)
{
    // "kotlin" is not a seeded language -- resolves to the generic fallback (never nullptr).
    const auto& generic=*SyntaxLanguageRegistry::instance().find(QStringLiteral("kotlin"));
    UISE_TEST_CHECK_EQUAL(generic.index(),SyntaxLanguageRegistry::GenericLanguageIndex);

    auto spans=tokenizeSyntaxLine(generic,QStringLiteral("fun x = 42"));
    bool sawFunOrX=false;
    bool sawNumber=false;
    for (const auto& s : spans)
    {
        if (s.start==0 && s.length==3) // "fun"
        {
            sawFunOrX=true;
        }
        if (s.bucket==SyntaxBucket::Literal)
        {
            sawNumber=true;
        }
    }
    UISE_TEST_CHECK(!sawFunOrX);
    UISE_TEST_CHECK(sawNumber);

    auto commentSpans=tokenizeSyntaxLine(generic,QStringLiteral("// trailing comment"));
    UISE_TEST_REQUIRE(commentSpans.size()==1);
    UISE_TEST_CHECK(commentSpans[0].bucket==SyntaxBucket::Comment);
}

BOOST_AUTO_TEST_CASE(TestAliasResolution)
{
    auto& registry=SyntaxLanguageRegistry::instance();
    UISE_TEST_CHECK_EQUAL_QSTR(registry.find(QStringLiteral("C++"))->id(),QStringLiteral("cpp"));
    UISE_TEST_CHECK_EQUAL_QSTR(registry.find(QStringLiteral("cxx"))->id(),QStringLiteral("cpp"));
    UISE_TEST_CHECK_EQUAL_QSTR(registry.find(QStringLiteral("  CPP  "))->id(),QStringLiteral("cpp"));
    UISE_TEST_CHECK_EQUAL_QSTR(registry.find(QStringLiteral(".cpp"))->id(),QStringLiteral("cpp"));
    UISE_TEST_CHECK_EQUAL_QSTR(registry.find(QStringLiteral("h"))->id(),QStringLiteral("cpp"));
    UISE_TEST_CHECK_EQUAL_QSTR(registry.find(QStringLiteral("unknownlang123"))->id(),QStringLiteral("generic"));
    UISE_TEST_CHECK(registry.find(QString{})==nullptr);
    UISE_TEST_CHECK(registry.find(QStringLiteral("   "))==nullptr);
}

BOOST_AUTO_TEST_CASE(TestNonAsciiLineDoesNotCrashAndSpansStayInBounds)
{
    const auto& cpp=*SyntaxLanguageRegistry::instance().find(QStringLiteral("cpp"));
    auto line=QString::fromUtf8("\xd0\xbf\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82 \xf0\x9f\x98\x80 code();");
    auto spans=tokenizeSyntaxLine(cpp,line);
    for (const auto& s : spans)
    {
        UISE_TEST_CHECK(s.start>=0);
        UISE_TEST_CHECK(s.start+s.length<=line.size());
    }
    bool sawCallable=false;
    for (const auto& s : spans)
    {
        if (s.bucket==SyntaxBucket::Callable)
        {
            sawCallable=true;
        }
    }
    UISE_TEST_CHECK(sawCallable);
}

/****************************** document-integration cases ******************************/

BOOST_AUTO_TEST_CASE(TestCodeBlockFormatsPresentNonCodeAbsent)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            QTextDocument doc;
            doc.setHtml(markdownToHtml(QStringLiteral("```cpp\nint x = 1;\n```\n\nplain paragraph")));

            SyntaxHighlighter highlighter(&doc);
            highlighter.rehighlight();

            bool sawCode=false;
            bool sawPlain=false;
            for (auto block=doc.begin();block!=doc.end();block=block.next())
            {
                auto text=block.text();
                if (text.contains(QStringLiteral("int x")))
                {
                    UISE_TEST_CHECK(!block.layout()->formats().isEmpty());
                    sawCode=true;
                }
                else if (text.contains(QStringLiteral("plain paragraph")))
                {
                    UISE_TEST_CHECK(block.layout()->formats().isEmpty());
                    sawPlain=true;
                }
            }
            UISE_TEST_CHECK(sawCode);
            UISE_TEST_CHECK(sawPlain);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestMetricsIdenticalWithAndWithoutHighlighter)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            auto html=markdownToHtml(QStringLiteral("```cpp\nint compute(int a, int b) { return a + b; }\n```"));

            QTextDocument plainDoc;
            plainDoc.setHtml(html);
            auto plainWidth=plainDoc.idealWidth();
            auto plainSize=plainDoc.size();

            QTextDocument highlightedDoc;
            highlightedDoc.setHtml(html);
            SyntaxHighlighter highlighter(&highlightedDoc);
            highlighter.rehighlight();
            auto highlightedWidth=highlightedDoc.idealWidth();
            auto highlightedSize=highlightedDoc.size();

            // Colour-only formatting must never change glyph metrics -- see SyntaxHighlighter's
            // own class doc comment.
            UISE_TEST_CHECK_EQUAL(plainWidth,highlightedWidth);
            UISE_TEST_CHECK_EQUAL(plainSize.width(),highlightedSize.width());
            UISE_TEST_CHECK_EQUAL(plainSize.height(),highlightedSize.height());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestDeferredHighlightNeedsExplicitRehighlight)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            QTextDocument doc;
            doc.setHtml(markdownToHtml(QStringLiteral("```cpp\nint x = 1;\n```")));

            // Deliberately NOT followed by rehighlight() -- the executable form of the Qt gotcha
            // ChatMessageTextBrowser::ensureSyntaxHighlighter()'s own doc comment explains:
            // attaching to an already non-empty document sets rehighlightPending and DISCARDS the
            // very reformat this test checks for, until the queued singleShot(0) fires.
            SyntaxHighlighter highlighter(&doc);

            QTextBlock codeBlock;
            for (auto block=doc.begin();block!=doc.end();block=block.next())
            {
                if (block.text().contains(QStringLiteral("int x")))
                {
                    codeBlock=block;
                    break;
                }
            }
            UISE_TEST_REQUIRE(codeBlock.isValid());
            UISE_TEST_CHECK(codeBlock.layout()->formats().isEmpty());

            QCoreApplication::processEvents();

            UISE_TEST_CHECK(!codeBlock.layout()->formats().isEmpty());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestUntaggedFenceNoFormats)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            QTextDocument doc;
            doc.setHtml(markdownToHtml(QStringLiteral("```\nplain fenced text\n```")));

            SyntaxHighlighter highlighter(&doc);
            highlighter.rehighlight();

            bool sawBlock=false;
            for (auto block=doc.begin();block!=doc.end();block=block.next())
            {
                if (block.text().contains(QStringLiteral("plain fenced text")))
                {
                    UISE_TEST_CHECK(block.layout()->formats().isEmpty());
                    UISE_TEST_CHECK_EQUAL(block.userState(),SyntaxNoCodeState);
                    sawBlock=true;
                }
            }
            UISE_TEST_CHECK(sawBlock);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestMultiFenceIndependentResolution)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            auto src=QStringLiteral(
                "```python\n"
                "def f():\n"
                "    pass\n"
                "```\n"
                "\n"
                "some text between\n"
                "\n"
                "```cpp\n"
                "int y = 2;\n"
                "```\n"
            );
            QTextDocument doc;
            doc.setHtml(markdownToHtml(src));

            SyntaxHighlighter highlighter(&doc);
            highlighter.rehighlight();

            bool sawPythonDef=false;
            bool sawBetween=false;
            bool sawCppInt=false;
            for (auto block=doc.begin();block!=doc.end();block=block.next())
            {
                auto text=block.text();
                if (text.contains(QStringLiteral("def f")))
                {
                    UISE_TEST_CHECK(!block.layout()->formats().isEmpty());
                    sawPythonDef=true;
                }
                else if (text.contains(QStringLiteral("some text between")))
                {
                    UISE_TEST_CHECK(block.layout()->formats().isEmpty());
                    UISE_TEST_CHECK_EQUAL(block.userState(),SyntaxNoCodeState);
                    sawBetween=true;
                }
                else if (text.contains(QStringLiteral("int y")))
                {
                    UISE_TEST_CHECK(!block.layout()->formats().isEmpty());
                    sawCppInt=true;
                }
            }
            UISE_TEST_CHECK(sawPythonDef);
            UISE_TEST_CHECK(sawBetween);
            UISE_TEST_CHECK(sawCppInt);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestUnclosedCommentDoesNotLeakPastCodeBlock)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            auto src=QStringLiteral(
                "```cpp\n"
                "/* unterminated\n"
                "```\n"
                "\n"
                "plain text after\n"
            );
            QTextDocument doc;
            doc.setHtml(markdownToHtml(src));

            SyntaxHighlighter highlighter(&doc);
            highlighter.rehighlight();

            bool sawAfter=false;
            for (auto block=doc.begin();block!=doc.end();block=block.next())
            {
                if (block.text().contains(QStringLiteral("plain text after")))
                {
                    UISE_TEST_CHECK_EQUAL(block.userState(),SyntaxNoCodeState);
                    UISE_TEST_CHECK(block.layout()->formats().isEmpty());
                    sawAfter=true;
                }
            }
            UISE_TEST_CHECK(sawAfter);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestColorMatchesCurrentSyntaxTheme)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            QTextDocument doc;
            doc.setHtml(markdownToHtml(QStringLiteral("```cpp\nif (x) {}\n```")));

            SyntaxHighlighter highlighter(&doc);
            highlighter.rehighlight();

            QTextBlock codeBlock;
            for (auto block=doc.begin();block!=doc.end();block=block.next())
            {
                if (block.text().contains(QStringLiteral("if")))
                {
                    codeBlock=block;
                    break;
                }
            }
            UISE_TEST_REQUIRE(codeBlock.isValid());

            auto ranges=codeBlock.layout()->formats();
            UISE_TEST_REQUIRE(!ranges.isEmpty());

            auto expected=Style::instance().syntaxColor(syntaxBucketName(SyntaxBucket::Keyword));
            UISE_TEST_REQUIRE(expected.has_value());

            bool foundKeywordColor=false;
            for (const auto& r : ranges)
            {
                if (r.format.foreground().color()==*expected)
                {
                    foundKeywordColor=true;
                }
            }
            UISE_TEST_CHECK(foundKeywordColor);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestPlainTextSurvivesStyleChangeReplay)
{
    // Regression guard for a pre-existing bug this stage's own testing made visible (see
    // ChatMessageTextBrowser::setPlainTextContent()'s own doc comment): without clearing
    // m_lastHtml, a QEvent::StyleChange's applyDocumentStyle() would replay a PREVIOUS message's
    // HTML back over plain-text content.
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            ChatMessageTextBrowser browser;
            browser.setHtmlContent(QStringLiteral("<pre class=\"language-cpp\"><code>int x = 1;</code></pre>"));
            browser.setPlainTextContent(QStringLiteral("just plain text"));

            QEvent styleChangeEvent(QEvent::StyleChange);
            QCoreApplication::sendEvent(&browser,&styleChangeEvent);

            UISE_TEST_CHECK_EQUAL_QSTR(browser.toPlainText(),QStringLiteral("just plain text"));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestClearAfterHighlighterAttachedNoCrash)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            ChatMessageTextBrowser browser;
            browser.setHtmlContent(QStringLiteral("<pre class=\"language-cpp\"><code>int x = 1;</code></pre>"));
            UISE_TEST_CHECK(browser.isSyntaxHighlightingEnabled());

            browser.setHtmlContent(QString{});
            browser.clear();

            UISE_TEST_CHECK(browser.toPlainText().isEmpty());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestSyntaxHighlightingToggleIsReactive)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            ChatMessageTextBrowser browser;
            browser.setHtmlContent(QStringLiteral("<pre class=\"language-cpp\"><code>if (x) {}</code></pre>"));

            QTextBlock codeBlock;
            for (auto block=browser.document()->begin();block!=browser.document()->end();block=block.next())
            {
                if (block.text().contains(QStringLiteral("if")))
                {
                    codeBlock=block;
                    break;
                }
            }
            UISE_TEST_REQUIRE(codeBlock.isValid());
            UISE_TEST_CHECK(!codeBlock.layout()->formats().isEmpty());

            browser.setSyntaxHighlightingEnabled(false);
            UISE_TEST_CHECK(codeBlock.layout()->formats().isEmpty());

            browser.setSyntaxHighlightingEnabled(true);
            UISE_TEST_CHECK(!codeBlock.layout()->formats().isEmpty());
        }
    );
}

/****************************** Stage 4: wide tables ******************************/

namespace {

//! The markdown the wide-table cases share: a 6-column table that wants far more width than any
//! chat bubble gives it, wrapped in paragraphs so the "surrounding prose keeps wrapping" property
//! is actually observable.
QString wideTableMarkdown()
{
    return QStringLiteral(
        "An intro paragraph long enough that it must wrap at the bubble width no matter how wide "
        "the table below it turns out to be.\n"
        "\n"
        "| Package | Version | Licence | Maintainer | Updated | Description |\n"
        "|---|---|---|---|---|---|\n"
        "| libexample-core | 1.24.7 | Apache-2.0 | infrastructure-team | 2026-08-14 | Shared runtime helpers |\n"
        "| libexample-net | 0.9.3 | MIT | networking-team | 2026-09-01 | Transport and retry policy |\n"
        "\n"
        "A trailing paragraph that must also keep wrapping at the bubble width.\n"
    );
}

QTextTable* firstTable(QTextDocument* doc)
{
    for (auto* frame : doc->rootFrame()->childFrames())
    {
        auto* table=qobject_cast<QTextTable*>(frame);
        if (table!=nullptr)
        {
            return table;
        }
    }
    return nullptr;
}

}

BOOST_AUTO_TEST_CASE(TestWideTableIsPinnedNotCompressed)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            constexpr int wrapWidth=380;

            ChatMessageTextBrowser browser;
            browser.setWrapWidth(wrapWidth);
            browser.setHtmlContent(markdownToHtml(wideTableMarkdown()));
            browser.setWrapWidth(wrapWidth);

            auto* table=firstTable(browser.document());
            UISE_TEST_REQUIRE(table!=nullptr);

            auto tableWidth=browser.document()->documentLayout()->frameBoundingRect(table).width();

            // Qt's own behaviour would compress this table to <= the wrap width; pinning is what
            // lets it keep its natural width and overflow instead.
            UISE_TEST_CHECK(tableWidth>wrapWidth);

            // ... and the overflow must be reachable, i.e. the browser grew a horizontal scrollbar.
            UISE_TEST_CHECK(browser.horizontalScrollBarPolicy()==Qt::ScrollBarAsNeeded);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestWideTablePinningLeavesParagraphsWrapped)
{
    // The whole point of pinning the TABLE's own frame rather than widening the document: prose
    // around it must keep wrapping at the bubble width.
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            constexpr int wrapWidth=380;

            ChatMessageTextBrowser browser;
            browser.setWrapWidth(wrapWidth);
            browser.setHtmlContent(markdownToHtml(wideTableMarkdown()));
            browser.setWrapWidth(wrapWidth);

            auto* doc=browser.document();
            auto paragraphWidth=doc->documentLayout()->blockBoundingRect(doc->firstBlock()).width();
            UISE_TEST_CHECK(paragraphWidth<=static_cast<qreal>(wrapWidth));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestNarrowTableIsNotPinned)
{
    // A table that already fits keeps Qt's own column balancing -- pinning it would be worse, and
    // must not drag in a scrollbar the message does not need.
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            ChatMessageTextBrowser browser;
            browser.setWrapWidth(380);
            browser.setHtmlContent(markdownToHtml(QStringLiteral(
                "| a | b |\n|---|---|\n| 1 | 2 |\n")));
            browser.setWrapWidth(380);

            UISE_TEST_CHECK(browser.horizontalScrollBarPolicy()==Qt::ScrollBarAlwaysOff);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestWideTableUnpinsWhenBubbleGrows)
{
    // A table pinned for a narrow bubble must be released again once the bubble is wide enough to
    // hold it -- otherwise it would stay overflowing (and keep its scrollbar) forever.
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            ChatMessageTextBrowser browser;
            browser.setWrapWidth(380);
            browser.setHtmlContent(markdownToHtml(wideTableMarkdown()));
            browser.setWrapWidth(380);
            UISE_TEST_REQUIRE(browser.horizontalScrollBarPolicy()==Qt::ScrollBarAsNeeded);

            browser.setWrapWidth(2000);
            UISE_TEST_CHECK(browser.horizontalScrollBarPolicy()==Qt::ScrollBarAlwaysOff);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestSizeHintStaysWithinWrapWidthDespitePinnedTable)
{
    // Regression guard for the trap this stage had to fix: pinning inflates
    // QTextDocument::idealWidth() (measured 380 -> 612 for one 6-column table), and sizeHint()
    // reported that verbatim -- which would have asked the layout for a bubble as wide as the
    // table and defeated maxBubbleWidth entirely.
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            constexpr int wrapWidth=380;

            ChatMessageTextBrowser browser;
            browser.setWrapWidth(wrapWidth);
            browser.setHtmlContent(markdownToHtml(wideTableMarkdown()));
            browser.setWrapWidth(wrapWidth);

            UISE_TEST_CHECK(browser.document()->idealWidth()>static_cast<qreal>(wrapWidth));
            UISE_TEST_CHECK(browser.sizeHint().width()<=wrapWidth+2*browser.frameWidth());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestWideTableTreatmentCanBeDisabled)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            ChatMessageTextBrowser browser;
            browser.setWrapWidth(380);
            browser.setHtmlContent(markdownToHtml(wideTableMarkdown()));
            browser.setWrapWidth(380);
            UISE_TEST_REQUIRE(browser.isWideTableScrollEnabled());
            UISE_TEST_REQUIRE(browser.horizontalScrollBarPolicy()==Qt::ScrollBarAsNeeded);

            browser.setWideTableScrollEnabled(false);
            UISE_TEST_CHECK(browser.horizontalScrollBarPolicy()==Qt::ScrollBarAlwaysOff);

            // Back to Qt's own compress-to-fit behaviour.
            browser.setWrapWidth(380);
            auto* table=firstTable(browser.document());
            UISE_TEST_REQUIRE(table!=nullptr);
            auto w=browser.document()->documentLayout()->frameBoundingRect(table).width();
            UISE_TEST_CHECK(w<=380.0);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestPlainTextClearsWideTableState)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            ChatMessageTextBrowser browser;
            browser.setWrapWidth(380);
            browser.setHtmlContent(markdownToHtml(wideTableMarkdown()));
            browser.setWrapWidth(380);
            UISE_TEST_REQUIRE(browser.horizontalScrollBarPolicy()==Qt::ScrollBarAsNeeded);

            browser.setPlainTextContent(QStringLiteral("just plain text, no table here"));
            UISE_TEST_CHECK(browser.horizontalScrollBarPolicy()==Qt::ScrollBarAlwaysOff);
            UISE_TEST_CHECK_EQUAL_QSTR(browser.toPlainText(),
                                       QStringLiteral("just plain text, no table here"));
        }
    );
}

namespace {

//! Exposes the protected clipboard hook so a test can inspect exactly what Ctrl+C would put on
//! the clipboard, without depending on a working platform clipboard under `offscreen`.
class TableViewerProbe : public ChatMessageTableViewer
{
    public:

        using ChatMessageTableViewer::createMimeDataFromSelection;
};

QString viewerTableHtml()
{
    return QStringLiteral(
        "<table>"
        "<tr><th>Package</th><th>Version</th><th>Licence</th></tr>"
        "<tr><td>libexample-core</td><td>1.24.7</td><td>Apache-2.0</td></tr>"
        "<tr><td>libexample-net</td><td>0.9.3</td><td>MIT</td></tr>"
        "</table>");
}

}

BOOST_AUTO_TEST_CASE(TestExpandedTableCopiesPlainTextAsTabSeparated)
{
    // Qt's own text/plain for a table selection is one cell PER LINE, which pastes into a
    // plain-text target as a single column. ChatMessageTableViewer replaces that flavour with
    // the tab-between-cells / newline-between-rows convention every spreadsheet expects.
    TestThread::instance()->execGuiThread(
        [&]()
        {
            TableViewerProbe view;
            view.setHtml(viewerTableHtml());
            view.selectAll();

            auto* mime=view.createMimeDataFromSelection();
            UISE_TEST_REQUIRE(mime!=nullptr);

            auto plain=mime->text();
            UISE_TEST_CHECK(plain.contains(QStringLiteral("Package\tVersion\tLicence")));
            UISE_TEST_CHECK(plain.contains(QStringLiteral("libexample-core\t1.24.7\tApache-2.0")));
            UISE_TEST_CHECK(plain.contains(QStringLiteral("libexample-net\t0.9.3\tMIT")));

            delete mime;
        }
    );
}

BOOST_AUTO_TEST_CASE(TestExpandedTableKeepsRichClipboardFlavours)
{
    // The text/plain override must not cost us the flavours that make an external paste land as a
    // REAL table -- text/html is what Excel/Word/Sheets consume, and text/markdown is what lets a
    // copied table go straight back into another chat message.
    TestThread::instance()->execGuiThread(
        [&]()
        {
            TableViewerProbe view;
            view.setHtml(viewerTableHtml());
            view.selectAll();

            auto* mime=view.createMimeDataFromSelection();
            UISE_TEST_REQUIRE(mime!=nullptr);

            UISE_TEST_CHECK(mime->hasHtml());
            UISE_TEST_CHECK(mime->html().contains(QStringLiteral("<table"),Qt::CaseInsensitive));
            UISE_TEST_CHECK(mime->html().contains(QStringLiteral("libexample-core")));
            UISE_TEST_CHECK(mime->formats().contains(QStringLiteral("text/markdown")));

            delete mime;
        }
    );
}

BOOST_AUTO_TEST_CASE(TestExpandedTableCopyIsLimitedToTheSelection)
{
    // A partial selection must copy only what was selected -- copying the whole table regardless
    // would be a correctness bug, not a convenience.
    TestThread::instance()->execGuiThread(
        [&]()
        {
            TableViewerProbe view;
            view.setHtml(viewerTableHtml());

            auto* table=firstTable(view.document());
            UISE_TEST_REQUIRE(table!=nullptr);

            // Select the last data row only.
            QTextCursor cursor(view.document());
            cursor.setPosition(table->cellAt(2,0).firstCursorPosition().position());
            cursor.setPosition(table->cellAt(2,2).lastCursorPosition().position(),
                               QTextCursor::KeepAnchor);
            view.setTextCursor(cursor);

            auto* mime=view.createMimeDataFromSelection();
            UISE_TEST_REQUIRE(mime!=nullptr);

            auto plain=mime->text();
            UISE_TEST_CHECK(plain.contains(QStringLiteral("libexample-net\t0.9.3\tMIT")));
            UISE_TEST_CHECK(!plain.contains(QStringLiteral("libexample-core")));
            UISE_TEST_CHECK(!plain.contains(QStringLiteral("Package")));

            delete mime;
        }
    );
}

BOOST_AUTO_TEST_CASE(TestExpandedTableMergedCellAppearsOnce)
{
    // cellAt() reports a merged cell at every position it spans; without the origin dedup a
    // colspan=3 cell would be emitted three times across the row.
    TestThread::instance()->execGuiThread(
        [&]()
        {
            TableViewerProbe view;
            view.setHtml(QStringLiteral(
                "<table>"
                "<tr><td colspan=\"3\">MERGED</td></tr>"
                "<tr><td>a</td><td>b</td><td>c</td></tr>"
                "</table>"));
            view.selectAll();

            auto* mime=view.createMimeDataFromSelection();
            UISE_TEST_REQUIRE(mime!=nullptr);

            UISE_TEST_CHECK_EQUAL(mime->text().count(QStringLiteral("MERGED")),1);
            UISE_TEST_CHECK(mime->text().contains(QStringLiteral("a\tb\tc")));

            delete mime;
        }
    );
}

BOOST_AUTO_TEST_CASE(TestExpandedTableEmitsCopiedSignalEvenWithToastDisabled)
{
    // The signal is the hook for a host presenting its own confirmation, so it must fire whether
    // or not the built-in toast is in play -- otherwise disabling the toast would silently leave
    // such a host with no notification at all.
    TestThread::instance()->execGuiThread(
        [&]()
        {
            TableViewerProbe view;
            view.setHtml(viewerTableHtml());

            int copiedCount=0;
            QObject::connect(&view,&ChatMessageTableViewer::tableCopied,
                             &view,[&copiedCount](){++copiedCount;});

            UISE_TEST_CHECK(view.isCopyToastEnabled());
            view.copyTable();
            UISE_TEST_CHECK_EQUAL(copiedCount,1);

            view.setCopyToastEnabled(false);
            UISE_TEST_CHECK(!view.isCopyToastEnabled());
            view.copyTable();
            UISE_TEST_CHECK_EQUAL(copiedCount,2);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestExpandedTableCopyTableNeedsNoPriorSelection)
{
    // copyTable() is what the Copy table button and a selection-less Ctrl+C both go through.
    TestThread::instance()->execGuiThread(
        [&]()
        {
            TableViewerProbe view;
            view.setHtml(viewerTableHtml());
            UISE_TEST_REQUIRE(!view.textCursor().hasSelection());

            view.copyTable();

            // It selects the whole table on the way, which is what makes the ordinary copy path
            // produce every flavour.
            UISE_TEST_CHECK(view.textCursor().hasSelection());

            auto* mime=view.createMimeDataFromSelection();
            UISE_TEST_REQUIRE(mime!=nullptr);
            UISE_TEST_CHECK(mime->text().contains(QStringLiteral("Package\tVersion\tLicence")));
            UISE_TEST_CHECK(mime->text().contains(QStringLiteral("libexample-net\t0.9.3\tMIT")));
            delete mime;
        }
    );
}

BOOST_AUTO_TEST_SUITE_END()
