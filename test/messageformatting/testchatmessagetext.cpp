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

BOOST_AUTO_TEST_SUITE_END()
