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

/** @file uise/test/messageformatting/testchatmessagetext.cpp
*
*  Tests ChatMessageTextBrowser's code-block treatment (task-message-formatting-plan.md, §7a):
*  finding a code block on the loaded document at all, reserving the room its painted padding
*  fills, and keeping the message's timestamp from being overlaid on top of it.
*
*  Every case builds a live widget and touches a QTextDocument, so all of them are marshalled onto
*  the GUI thread via TestThread::execGuiThread(), the same balance as testmessageeditor.cpp.
*
*/

/****************************************************************************/

#include <boost/test/unit_test.hpp>

#include <QApplication>
#include <QSignalSpy>
#include <QClipboard>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include <QAbstractTextDocumentLayout>
#include <QColor>
#include <QUrl>

#include <uise/test/uise-testthread.hpp>

#include <uise/desktop/style.hpp>
#include <uise/desktop/markdownrenderer.hpp>
#include <uise/desktop/chatmessagetext.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

BOOST_AUTO_TEST_SUITE(TestChatMessageText)

namespace {

//! Load rendered markdown into a browser the way ChatMessageText::loadText() does, then force the
//! layout the walk-back in lastLineRect() depends on.
void loadMarkdown(ChatMessageTextBrowser& browser, const QString& markdown, int width=320)
{
    browser.setHtmlContent(markdownToHtml(markdown));
    browser.document()->setTextWidth(width);
    // The format writes in applyCodeBlockLayout() invalidate the block layouts; in the real widget
    // the bubble-width negotiation redoes them before anything reads a line count.
    (void)browser.document()->documentLayout()->documentSize();
}

//! Same as loadMarkdown(), but with the mention scheme allowlisted -- the caller-owned options
//! copy AbstractChatMessageText::setMentionsEnabled() itself builds (task-message-formatting-
//! plan.md, Stage 6). Used to test the RENDERER's opt-in leg directly, independent of
//! ChatMessageText's own wiring (covered separately below via a live ChatMessageText).
void loadMarkdownWithMentions(ChatMessageTextBrowser& browser, const QString& markdown, int width=320)
{
    MarkdownRenderOptions options;
    options.allowedLinkSchemes.append(mentionUrlScheme());
    browser.setHtmlContent(markdownToHtml(markdown,options));
    browser.document()->setTextWidth(width);
    (void)browser.document()->documentLayout()->documentSize();
}

//! Whether the document holds any anchor fragment matching `href`.
bool documentHasAnchor(QTextDocument* document, const QString& href)
{
    for (auto block=document->begin(); block.isValid(); block=block.next())
    {
        for (auto it=block.begin(); !it.atEnd(); ++it)
        {
            const auto format=it.fragment().charFormat();
            if (format.isAnchor() && format.anchorHref()==href)
            {
                return true;
            }
        }
    }
    return false;
}

QTextBlock lastRenderedBlock(const ChatMessageTextBrowser& browser)
{
    auto* doc=browser.document();
    auto block=doc->lastBlock();
    while (block.isValid() && (!block.isVisible() || block.layout()==nullptr
                                || block.layout()->lineCount()==0))
    {
        block=block.previous();
    }
    return block;
}

}

BOOST_AUTO_TEST_CASE(TestUntaggedCodeBlockIsFound)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            ChatMessageTextBrowser browser;
            loadMarkdown(browser,QStringLiteral("before\n\n```\nint x = 1;\nint y = 2;\n```\n\nafter\n"));

            // The load-bearing case. An UNTAGGED fence carries no QTextFormat::BlockCodeLanguage,
            // so the only marker it has is QTextBlockFormat::nonBreakableLines() -- which our own
            // messagetext.css used to clear with `white-space: pre-wrap`, leaving an untagged code
            // block indistinguishable from any other styled paragraph.
            UISE_TEST_REQUIRE(browser.codeBlocks().size()==1);
            UISE_TEST_CHECK(browser.codeBlocks().front().language.isEmpty());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestTaggedCodeBlockRecordsItsLanguage)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            ChatMessageTextBrowser browser;
            loadMarkdown(browser,QStringLiteral("```cpp\nauto x = 1;\n```\n"));

            UISE_TEST_REQUIRE(browser.codeBlocks().size()==1);

            // Qt cannot render this itself at all -- toHtml() does not even emit the property, the
            // layout never reads it, and Qt's CSS subset has no `content:` -- so it is recorded
            // here for the floating overlay to draw.
            UISE_TEST_CHECK_EQUAL(browser.codeBlocks().front().language.toStdString(),
                                  std::string("cpp"));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestSeparateCodeBlocksStaySeparate)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            ChatMessageTextBrowser browser;
            loadMarkdown(browser,QStringLiteral("```\na\n```\n\ntext\n\n```\nb\n```\n"));
            UISE_TEST_CHECK_EQUAL(static_cast<int>(browser.codeBlocks().size()),2);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestCodeBlockReservesPaddingRoomAndStillWraps)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            ChatMessageTextBrowser browser;
            loadMarkdown(browser,QStringLiteral("```\nint x = 1;\nint y = 2;\n```\n"));
            UISE_TEST_REQUIRE(browser.codeBlocks().size()==1);

            const auto& tracked=browser.codeBlocks().front();
            auto* doc=browser.document();
            const auto first=doc->findBlock(tracked.firstPosition);
            const auto last=doc->findBlock(tracked.lastPosition);
            const auto padding=browser.codeBlockPadding();

            // Padding on a text block is parsed and then ignored by Qt, so the room is reserved as
            // block margins and paintEvent() fills the box back out over it.
            UISE_TEST_CHECK_EQUAL(static_cast<int>(first.blockFormat().leftMargin()),padding);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(first.blockFormat().rightMargin()),padding);

            // Top gap on the first line and bottom on the last only -- otherwise the gap repeats
            // between every pair of code lines.
            UISE_TEST_CHECK_EQUAL(static_cast<int>(first.blockFormat().topMargin()),padding);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(last.blockFormat().bottomMargin()),padding);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(first.blockFormat().bottomMargin()),0);

            // The marker is cleared once read, which restores exactly the wrapping the removed
            // `white-space: pre-wrap` rule used to provide.
            UISE_TEST_CHECK(!first.blockFormat().nonBreakableLines());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestTimestampIsNotOverlaidOnACodeBlock)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            ChatMessageTextBrowser browser;
            loadMarkdown(browser,QStringLiteral("hello\n\n```\nint x = 1;\n```\n"));

            const auto last=lastRenderedBlock(browser);
            UISE_TEST_REQUIRE(last.isValid());
            UISE_TEST_REQUIRE(browser.codeBlocks().size()==1);
            UISE_TEST_REQUIRE(last.position()>=browser.codeBlocks().front().firstPosition);

            // The trailing-space overlay rests on there being blank room to the right of the last
            // WORD. A code block paints a background across the full bubble width, so the
            // timestamp does not land in empty space -- it lands on the code. An invalid rect is
            // already the "no room" signal evaluateInlineBottom() understands.
            UISE_TEST_CHECK(!browser.lastLineRect().isValid());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestOrdinaryTextStillGetsTheInlineOverlay)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            ChatMessageTextBrowser browser;
            loadMarkdown(browser,QStringLiteral("```\nint x = 1;\n```\n\nand some prose after it\n"));

            // The guard is per-block, not per-message: a message that merely CONTAINS a code block
            // keeps the inline timestamp as long as it does not END with one.
            UISE_TEST_REQUIRE(browser.codeBlocks().size()==1);
            UISE_TEST_CHECK(browser.lastLineRect().isValid());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestMessageWithoutCodeBlockIsUntouched)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            ChatMessageTextBrowser browser;
            loadMarkdown(browser,QStringLiteral("just prose here\n"));

            UISE_TEST_CHECK(browser.codeBlocks().empty());
            UISE_TEST_CHECK_EQUAL(
                static_cast<int>(browser.document()->firstBlock().blockFormat().leftMargin()),0);
            UISE_TEST_CHECK(browser.lastLineRect().isValid());
        }
    );
}

namespace {

//! A table wide enough that no reasonable bubble width fits it unpinned -- same shape as
//! demo/messageformatting's own "too wide for the bubble" sample -- followed by a short prose
//! line, so the LAST rendered block is ordinary text rather than the table itself. Isolates the
//! reservesHorizontalScrollBar() gate (this test group) from the separate "ends inside a table
//! cell" exclusion (TestMessageEndingInATableThatFitsStillExcludesInlineOverlay below).
QString wideTableThenProseMarkdown()
{
    return QStringLiteral(
        "| Package | Version | Licence | Maintainer | Updated | Description |\n"
        "|---|---|---|---|---|---|\n"
        "| libexample-core | 1.24.7 | Apache-2.0 | infrastructure-team | 2026-08-14 | Shared runtime helpers |\n"
        "\n"
        "and a short line after it\n"
    );
}

}

BOOST_AUTO_TEST_CASE(TestWideTablePinnedReservesScrollbarAndExcludesInlineOverlay)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            ChatMessageTextBrowser browser;
            browser.setHtmlContent(markdownToHtml(wideTableThenProseMarkdown()));

            // Narrow enough that the table cannot fit -- applyWideTableLayout() pins it to its
            // natural width and switches the horizontal scrollbar on.
            browser.setWrapWidth(320);

            UISE_TEST_REQUIRE(browser.reservesHorizontalScrollBar());

            // The row is a fixed sibling overlay, positioned once per negotiation pass and never
            // re-derived as the user scrolls -- tucking it onto a line inside a horizontally
            // scrollable document would let that line slide out from under a timestamp that never
            // moves. This must hold even though the trailing "and a short line after it" block is
            // itself plain LTR text outside the table.
            UISE_TEST_CHECK(!browser.lastLineRect().isValid());

            // The reserved band must be counted in the hint WITHOUT the widget ever being shown --
            // horizontalScrollBar()->isVisible() stays false on a widget that was never laid out
            // on screen, which is exactly the stale read this gate replaces.
            UISE_TEST_REQUIRE(browser.horizontalScrollBar()!=nullptr);
            auto docHeight=static_cast<int>(browser.document()->size().height()+2*browser.frameWidth());
            UISE_TEST_CHECK_EQUAL(browser.sizeHint().height(),
                                   docHeight+browser.horizontalScrollBar()->sizeHint().height());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestWideTableUnpinsAndRestoresInlineOverlayWhenBubbleWidens)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            ChatMessageTextBrowser browser;
            browser.setHtmlContent(markdownToHtml(wideTableThenProseMarkdown()));
            browser.setWrapWidth(320);
            UISE_TEST_REQUIRE(browser.reservesHorizontalScrollBar());
            UISE_TEST_REQUIRE(!browser.lastLineRect().isValid());

            // Widen well past the table's own natural width -- applyWideTableLayout() re-measures
            // on every setWrapWidth() call (the un-pin loop at its own top), so a table pinned for
            // a narrower bubble un-pins once it genuinely fits.
            browser.setWrapWidth(2000);

            UISE_TEST_CHECK(!browser.reservesHorizontalScrollBar());
            UISE_TEST_CHECK(browser.lastLineRect().isValid());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestMessageEndingInATableThatFitsStillExcludesInlineOverlay)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            ChatMessageTextBrowser browser;
            loadMarkdown(browser,QStringLiteral(
                "short table below:\n\n| a | b |\n|---|---|\n| 1 | 2 |\n"));

            // Small enough that it was never pinned -- the scrollbar reservation must stay off...
            UISE_TEST_CHECK(!browser.reservesHorizontalScrollBar());

            // ...but a timestamp landing inside a table cell reads as one of the table's OWN
            // values rather than the message's status row, so the overlay is refused regardless.
            UISE_TEST_CHECK(!browser.lastLineRect().isValid());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestCopyCodeBlockEmitsSignalAndCopiesText)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            ChatMessageTextBrowser browser;
            loadMarkdown(browser,QStringLiteral("```cpp\nint x = 1;\nint y = 2;\n```\n"));
            UISE_TEST_REQUIRE(browser.codeBlocks().size()==1);

            QSignalSpy spy(&browser,&ChatMessageTextBrowser::codeBlockCopied);
            QGuiApplication::clipboard()->clear();

            browser.copyCodeBlock(browser.codeBlocks().front());

            // Same contract as ChatMessageTableViewer::tableCopied(): emitted whenever the content
            // reaches the clipboard, whether or not a toast was shown.
            UISE_TEST_REQUIRE(spy.count()==1);
            UISE_TEST_CHECK_EQUAL(spy.at(0).at(0).toString().toStdString(),std::string("cpp"));

            // Every line of the block, and nothing around it.
            const auto copied=QGuiApplication::clipboard()->text();
            UISE_TEST_CHECK_EQUAL(copied.toStdString(),std::string("int x = 1;\nint y = 2;"));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestCopyCodeBlockStillSignalsWithTheToastOff)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            ChatMessageTextBrowser browser;
            loadMarkdown(browser,QStringLiteral("```\nplain code\n```\n"));
            UISE_TEST_REQUIRE(browser.codeBlocks().size()==1);

            // The whole point of the opt-out: a host that presents its own confirmation turns the
            // built-in toast off and reacts to the signal instead, so the signal must NOT be
            // conditional on it.
            UISE_TEST_CHECK(browser.isCopyToastEnabled());
            browser.setCopyToastEnabled(false);

            QSignalSpy spy(&browser,&ChatMessageTextBrowser::codeBlockCopied);
            browser.copyCodeBlock(browser.codeBlocks().front());

            UISE_TEST_REQUIRE(spy.count()==1);
            // An untagged fence reports an empty language rather than not reporting at all.
            UISE_TEST_CHECK(spy.at(0).at(0).toString().isEmpty());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestPlainTextContentClearsTrackedCodeBlocks)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            ChatMessageTextBrowser browser;
            loadMarkdown(browser,QStringLiteral("```\nint x = 1;\n```\n"));
            UISE_TEST_REQUIRE(browser.codeBlocks().size()==1);

            // A recycled flyweight bubble reused for a plain-text message would otherwise keep
            // painting the previous message's boxes.
            browser.setPlainTextContent(QStringLiteral("plain"));
            UISE_TEST_CHECK(browser.codeBlocks().empty());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestMentionsDisabledByDefault)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            ChatMessageText message;
            UISE_TEST_CHECK(!message.isMentionsEnabled());

            // The renderer's OWN default options reject the scheme regardless -- this is the
            // sanitization leg testmarkdownrenderer.cpp's TestMentionSchemeRejectedByDefaultOptions
            // already covers; here the concern is the WIDGET-level default.
            message.loadText(QStringLiteral("[Alice](whitem-mention:usr1)"),TextFormat::Markdown);

            auto* browser=message.findChild<ChatMessageTextBrowser*>();
            UISE_TEST_REQUIRE(browser!=nullptr);
            UISE_TEST_CHECK(!documentHasAnchor(browser->document(),QStringLiteral("whitem-mention:usr1")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestMentionAnchorRenderedOnlyWhenMentionsEnabled)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            ChatMessageText message;
            message.setMentionsEnabled(true);
            message.loadText(QStringLiteral("[Alice](whitem-mention:usr1)"),TextFormat::Markdown);

            auto* browser=message.findChild<ChatMessageTextBrowser*>();
            UISE_TEST_REQUIRE(browser!=nullptr);
            UISE_TEST_CHECK(documentHasAnchor(browser->document(),QStringLiteral("whitem-mention:usr1")));

            // An ordinary link in the SAME message is unaffected either way.
            message.loadText(QStringLiteral("[site](https://example.com)"),TextFormat::Markdown);
            UISE_TEST_CHECK(documentHasAnchor(browser->document(),QStringLiteral("https://example.com")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestTogglingMentionsEnabledRerendersLoadedContent)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            ChatMessageText message;
            message.loadText(QStringLiteral("[Alice](whitem-mention:usr1)"),TextFormat::Markdown);

            auto* browser=message.findChild<ChatMessageTextBrowser*>();
            UISE_TEST_REQUIRE(browser!=nullptr);
            UISE_TEST_CHECK(!documentHasAnchor(browser->document(),QStringLiteral("whitem-mention:usr1")));

            // setMentionsEnabled() re-renders the CACHED source -- ChatMessageText_p::sourceText,
            // whose own doc comment already names Stage 6 as the reason it exists -- without a
            // second loadText() call from the host.
            message.setMentionsEnabled(true);
            UISE_TEST_CHECK(documentHasAnchor(browser->document(),QStringLiteral("whitem-mention:usr1")));

            message.setMentionsEnabled(false);
            UISE_TEST_CHECK(!documentHasAnchor(browser->document(),QStringLiteral("whitem-mention:usr1")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestExtraLinkifyResolvesPlainMentionInBubble)
{
    // The concrete reported scenario: MessageEditor::insertMentionText() puts a literal
    // "@alice" into the SENT message with no anchor formatting at all (by design). Rendering it
    // as a clickable mention in the BUBBLE needs a directory lookup this widget cannot do on its
    // own -- mentionsEnabled alone is not enough, since it only allowlists the whitem-mention:
    // scheme for anchors that already exist in the source; a host also has to supply
    // extraLinkify to actually RECOGNIZE the bare "@alice" text and turn it into one.
    TestThread::instance()->execGuiThread(
        [&]()
        {
            ChatMessageText message;
            message.setMentionsEnabled(true);
            message.setExtraLinkify(
                [](const QString& text) -> QString
                {
                    if (!text.contains(QStringLiteral("@alice")))
                    {
                        return QString{};
                    }
                    auto escaped=text.toHtmlEscaped();
                    escaped.replace(QStringLiteral("@alice"),
                        QStringLiteral("<a href=\"whitem-mention:usr-0001\">Alice Anderson</a>"));
                    return escaped;
                }
            );

            message.loadText(QStringLiteral("hi @alice, look at this"),TextFormat::Markdown);

            auto* browser=message.findChild<ChatMessageTextBrowser*>();
            UISE_TEST_REQUIRE(browser!=nullptr);
            UISE_TEST_CHECK(documentHasAnchor(browser->document(),QStringLiteral("whitem-mention:usr-0001")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestExtraLinkifyReactivelyRerendersLoadedContent)
{
    // setExtraLinkify(), like setMentionsEnabled(), re-renders the CACHED source immediately --
    // a qproperty- style setter arriving after loadText() already ran still takes effect.
    TestThread::instance()->execGuiThread(
        [&]()
        {
            ChatMessageText message;
            message.loadText(QStringLiteral("hi @alice"),TextFormat::Markdown);

            auto* browser=message.findChild<ChatMessageTextBrowser*>();
            UISE_TEST_REQUIRE(browser!=nullptr);
            UISE_TEST_CHECK(!documentHasAnchor(browser->document(),QStringLiteral("whitem-mention:usr-0001")));

            message.setExtraLinkify(
                [](const QString& text) -> QString
                {
                    if (!text.contains(QStringLiteral("@alice")))
                    {
                        return QString{};
                    }
                    return QStringLiteral("hi <a href=\"whitem-mention:usr-0001\">Alice Anderson</a>");
                }
            );
            // mentionsEnabled was never turned on here -- extraLinkify's own returned HTML
            // bypasses allowedLinkSchemes entirely (see its TRUST BOUNDARY doc comment), so it
            // does not need mentionsEnabled to work.
            UISE_TEST_CHECK(documentHasAnchor(browser->document(),QStringLiteral("whitem-mention:usr-0001")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestMentionStyledDistinctlyFromOrdinaryLink)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            ChatMessageTextBrowser browser;
            browser.setLinkColor(QColor(0x1A,0x6F,0xD4));
            browser.setMentionColor(QColor(0x7A,0x3F,0xBF));

            loadMarkdownWithMentions(
                browser,
                QStringLiteral("ordinary [site](https://example.com) and mention [Alice](whitem-mention:usr1)")
            );

            QColor linkColor;
            QColor mentionColor;
            for (auto block=browser.document()->begin(); block.isValid(); block=block.next())
            {
                for (auto it=block.begin(); !it.atEnd(); ++it)
                {
                    const auto format=it.fragment().charFormat();
                    if (!format.isAnchor())
                    {
                        continue;
                    }
                    if (format.anchorHref()==QStringLiteral("https://example.com"))
                    {
                        linkColor=format.foreground().color();
                    }
                    else if (format.anchorHref()==QStringLiteral("whitem-mention:usr1"))
                    {
                        mentionColor=format.foreground().color();
                    }
                }
            }
            UISE_TEST_CHECK_EQUAL_QSTR(linkColor.name(),QColor(0x1A,0x6F,0xD4).name());
            UISE_TEST_CHECK_EQUAL_QSTR(mentionColor.name(),QColor(0x7A,0x3F,0xBF).name());
            UISE_TEST_CHECK(linkColor!=mentionColor);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestMentionUrlReachesLinkActivated)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            ChatMessageTextBrowser browser;
            loadMarkdownWithMentions(browser,QStringLiteral("[Alice](whitem-mention:usr1)"));

            // setOpenLinks(false) already keeps activation with the host for ANY scheme --
            // confirmed here rather than assumed, since a custom scheme is exactly the case a
            // browser's OWN openLinks=true default would otherwise swallow via setSource().
            UISE_TEST_CHECK(!browser.openLinks());

            QSignalSpy spy(&browser,&ChatMessageTextBrowser::linkActivated);
            const QUrl mentionUrl(QStringLiteral("whitem-mention:usr1"));
            emit browser.anchorClicked(mentionUrl);

            UISE_TEST_REQUIRE_EQUAL(spy.count(),1);
            UISE_TEST_CHECK_EQUAL_QSTR(spy.at(0).at(0).toUrl().toString(),mentionUrl.toString());
        }
    );
}

BOOST_AUTO_TEST_SUITE_END()
