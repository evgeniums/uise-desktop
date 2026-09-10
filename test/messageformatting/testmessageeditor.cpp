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

/** @file uise/test/messageformatting/testmessageeditor.cpp
*
*  Tests MessageEditor/AbstractMessageEditor/MessageEditorToolbar's Stage 5a additions
*  (task-message-formatting-plan.md): the three-value MessageEditingMode, the deprecated
*  Qt::TextFormat forwarding, the formatting toolbar, and the expand/max-height toggle.
*
*  Almost every case here constructs a QWidget or touches a QTextDocument through a live
*  MessageEditor, so almost every case is marshalled onto the GUI thread via
*  TestThread::execGuiThread() -- the opposite balance from testsyntaxhighlighter.cpp's pure
*  tokenizer cases, and the same reasoning as every case in testmarkdownrenderer.cpp. Widgets are
*  stack-allocated inside the lambda and never shown -- isHidden() is used throughout instead of
*  isVisible(), since an unshown widget's isVisible() is always false regardless of what was
*  explicitly requested.
*
*/

/****************************************************************************/

#include <boost/test/unit_test.hpp>

#include <utility>

#include <QSignalSpy>
#include <QApplication>
#include <QPushButton>
#include <QFrame>
#include <QBoxLayout>
#include <QKeyEvent>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextTable>
#include <QTextList>
#include <QTextFormat>
#include <QMetaObject>
#include <QPoint>
#include <QFont>
#include <QColor>
#include <QTextLayout>
#include <QFontMetricsF>
#include <QTextFrame>
#include <QMimeData>
#include <QClipboard>
#include <QImage>

#include <uise/test/uise-testthread.hpp>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/chatmessagetext.hpp>
#include <uise/desktop/messageeditor.hpp>
#include <uise/desktop/messageeditortoolbar.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/hyperlinkdialog.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

namespace {

//! Minimal AbstractMessageEditor stub with every pure virtual trivially implemented -- used only
//! to test the deprecated updateEditingMode()/updateMessageEditingMode() hook chain in
//! isolation, without pulling in MessageEditor's own (now-removed) override of the deprecated
//! hook.
class StubMessageEditor : public AbstractMessageEditor
{
    public:

        using AbstractMessageEditor::AbstractMessageEditor;

        bool legacyHookCalled=false;

        void loadText(const QString&, TextFormat) override {}
        QString text(TextFormat) const override { return {}; }
        QString selectedText(TextFormat) const override { return {}; }
        void setFocusIn() override {}
        void setPlaceHolderText(const QString&) override {}
        bool hasSelection() const override { return false; }
        bool isEmpty() const override { return true; }
        bool canPasteFromClipboard() const override { return false; }
        void addLeadingWidget(QWidget*) override {}
        void addTrailingWidget(QWidget*) override {}
        void selectAll() override {}
        void clearSelection() override {}
        void clear() override {}
        void cut() override {}
        void copy() override {}
        void paste() override {}

    protected:

        void updateEditingMode() override
        {
            legacyHookCalled=true;
        }
};

//! MessageEditor subclass overriding the deprecated hook directly -- used to prove MessageEditor
//! itself no longer chains into it (documented accepted break, Stage 5a).
class SubMessageEditor : public MessageEditor
{
    public:

        using MessageEditor::MessageEditor;

        bool legacyHookCalled=false;

    protected:

        void updateEditingMode() override
        {
            legacyHookCalled=true;
        }
};

}

BOOST_AUTO_TEST_SUITE(TestMessageEditor)

BOOST_AUTO_TEST_CASE(TestModeConversionRoundTrip)
{
    UISE_TEST_CHECK(messageEditingModeFromTextFormat(Qt::PlainText)==MessageEditingMode::Plaintext);
    UISE_TEST_CHECK(messageEditingModeFromTextFormat(Qt::RichText)==MessageEditingMode::Wysiwyg);
    UISE_TEST_CHECK(messageEditingModeFromTextFormat(Qt::AutoText)==MessageEditingMode::Markdown);
    UISE_TEST_CHECK(messageEditingModeFromTextFormat(Qt::MarkdownText)==MessageEditingMode::Markdown);

    UISE_TEST_CHECK(textFormatFromMessageEditingMode(MessageEditingMode::Plaintext)==Qt::PlainText);
    UISE_TEST_CHECK(textFormatFromMessageEditingMode(MessageEditingMode::Wysiwyg)==Qt::RichText);
    UISE_TEST_CHECK(textFormatFromMessageEditingMode(MessageEditingMode::Markdown)==Qt::AutoText);

    // Documented lossy leg: Qt::MarkdownText round-trips to Qt::AutoText, not back to itself.
    auto roundTripped=textFormatFromMessageEditingMode(messageEditingModeFromTextFormat(Qt::MarkdownText));
    UISE_TEST_CHECK(roundTripped==Qt::AutoText);
}

BOOST_AUTO_TEST_CASE(TestDefaultModeMatchesLegacyRichText)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            UISE_TEST_CHECK(editor.messageEditingMode()==MessageEditingMode::Wysiwyg);
            UISE_TEST_CHECK(editor.editingMode()==Qt::RichText);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestLegacyPlainTextModeUnchanged)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setEditingMode(Qt::PlainText);
            UISE_TEST_CHECK(editor.messageEditingMode()==MessageEditingMode::Plaintext);
            UISE_TEST_CHECK(editor.editingMode()==Qt::PlainText);

            editor.loadText(QStringLiteral("**a**"),TextFormat::Plain);
            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Plain),QStringLiteral("**a**"));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestDeprecatedUpdateEditingModeHookStillCalled)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            StubMessageEditor editor;
            UISE_TEST_CHECK(!editor.legacyHookCalled);
            editor.setMessageEditingMode(MessageEditingMode::Plaintext);
            UISE_TEST_CHECK(editor.legacyHookCalled);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestMessageEditorDoesNotRunLegacyHook)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            SubMessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Plaintext);
            UISE_TEST_CHECK(!editor.legacyHookCalled);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestMarkdownModeDoesNotEscapeSource)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Markdown);

            auto source=QStringLiteral("**bold** and `x`");
            editor.loadText(source,TextFormat::Markdown);

            // The headline Stage 5a fix: the document IS the markdown source in this mode, so
            // reading it back as Markdown must return it VERBATIM -- toMarkdown() on a plain
            // document would backslash-escape it instead.
            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Markdown),source);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestPlaintextModeStillEscapesMarkdown)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Plaintext);

            editor.loadText(QStringLiteral("**bold**"),TextFormat::Markdown);
            auto md=editor.text(TextFormat::Markdown);

            // Deliberate asymmetry: literal text typed in Plaintext mode must still escape when
            // read as markdown, so it round-trips as the same literal characters elsewhere.
            UISE_TEST_CHECK(md.contains(QStringLiteral("\\")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestWysiwygMarkdownRoundTrip)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.loadText(QStringLiteral("**bold**"),TextFormat::Markdown);
            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Plain),QStringLiteral("bold"));
            UISE_TEST_CHECK(editor.text(TextFormat::Markdown).contains(QStringLiteral("**bold**")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestModeSwitchWysiwygToMarkdownShowsSource)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.loadText(QStringLiteral("**bold**"),TextFormat::Markdown);
            editor.setMessageEditingMode(MessageEditingMode::Markdown);
            UISE_TEST_CHECK(editor.text(TextFormat::Plain).contains(QStringLiteral("**bold**")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestModeSwitchMarkdownToWysiwygParsesSource)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Markdown);
            editor.loadText(QStringLiteral("**bold**"),TextFormat::Markdown);
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Plain),QStringLiteral("bold"));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestModeSwitchWysiwygToPlaintextKeepsSource)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.loadText(QStringLiteral("**bold**"),TextFormat::Markdown);
            editor.setMessageEditingMode(MessageEditingMode::Plaintext);

            // Fixed by Stage 5a: today's code lost the markup entirely (toPlainText()). The
            // markdown source is now kept as literal text instead.
            UISE_TEST_CHECK(editor.text(TextFormat::Plain).contains(QStringLiteral("**bold**")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestModeSwitchIsIdempotent)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.loadText(QStringLiteral("**bold**"),TextFormat::Markdown);
            editor.setMessageEditingMode(MessageEditingMode::Markdown);
            auto once=editor.text(TextFormat::Plain);
            // Setting the SAME mode again must not re-round-trip the document.
            editor.setMessageEditingMode(MessageEditingMode::Markdown);
            auto twice=editor.text(TextFormat::Plain);
            UISE_TEST_CHECK_EQUAL_QSTR(once,twice);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestBoldItalicUnderlineStrikeToggle)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.loadText(QStringLiteral("word"),TextFormat::Plain);

            auto* textEdit=editor.textEdit();
            auto cursor=textEdit->textCursor();
            cursor.select(QTextCursor::Document);
            textEdit->setTextCursor(cursor);

            editor.toolbar()->button(MessageEditorToolbarButton::Bold)->click();
            UISE_TEST_CHECK(textEdit->textCursor().charFormat().fontWeight()>=QFont::DemiBold);

            editor.toolbar()->button(MessageEditorToolbarButton::Italic)->click();
            UISE_TEST_CHECK(textEdit->currentCharFormat().fontItalic());

            editor.toolbar()->button(MessageEditorToolbarButton::Underline)->click();
            UISE_TEST_CHECK(textEdit->currentCharFormat().fontUnderline());

            editor.toolbar()->button(MessageEditorToolbarButton::Strikethrough)->click();
            UISE_TEST_CHECK(textEdit->currentCharFormat().fontStrikeOut());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestInlineCodeToggle)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            // Toggling off relies on the SECOND click reading a freshly-pushed
            // MessageEditorFormatState -- syncToolbarState() only pushes one while the toolbar
            // is actually shown (cheap early-out otherwise), so expand it first.
            editor.setExpanded(true);
            editor.loadText(QStringLiteral("word"),TextFormat::Plain);
            auto* textEdit=editor.textEdit();
            auto cursor=textEdit->textCursor();
            cursor.select(QTextCursor::Document);
            textEdit->setTextCursor(cursor);

            editor.toolbar()->button(MessageEditorToolbarButton::InlineCode)->click();
            UISE_TEST_CHECK(textEdit->currentCharFormat().fontFixedPitch());

            editor.toolbar()->button(MessageEditorToolbarButton::InlineCode)->click();
            UISE_TEST_CHECK(!textEdit->currentCharFormat().fontFixedPitch());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestHeadingSetsLevelAndSizeAdjustment)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.loadText(QStringLiteral("word"),TextFormat::Plain);

            editor.toolbar()->headingMenu()->setItemChecked(2,true);
            emit editor.toolbar()->headingMenu()->itemToggled(2,true);

            auto bf=editor.textEdit()->textCursor().blockFormat();
            UISE_TEST_CHECK_EQUAL(bf.headingLevel(),2);
            UISE_TEST_CHECK_EQUAL(editor.textEdit()->textCursor().charFormat().intProperty(QTextFormat::FontSizeAdjustment),2);

            emit editor.toolbar()->headingMenu()->itemToggled(0,true);
            bf=editor.textEdit()->textCursor().blockFormat();
            UISE_TEST_CHECK(!bf.hasProperty(QTextFormat::HeadingLevel));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestBulletAndNumberedList)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            // See TestInlineCodeToggle's comment -- toggling off needs the toolbar expanded so
            // syncToolbarState() actually pushes the refreshed state back after the first click.
            editor.setExpanded(true);
            editor.loadText(QStringLiteral("item"),TextFormat::Plain);

            editor.toolbar()->button(MessageEditorToolbarButton::BulletList)->click();
            auto* list=editor.textEdit()->textCursor().currentList();
            UISE_TEST_CHECK(list!=nullptr);
            UISE_TEST_CHECK(list->format().style()==QTextListFormat::ListDisc);

            editor.toolbar()->button(MessageEditorToolbarButton::BulletList)->click();
            UISE_TEST_CHECK(editor.textEdit()->textCursor().currentList()==nullptr);
            UISE_TEST_CHECK_EQUAL(editor.textEdit()->textCursor().blockFormat().indent(),0);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestListLevelCanBeSteppedUpAndDown)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setExpanded(true);
            editor.loadText(QStringLiteral("item"),TextFormat::Plain);

            editor.toolbar()->button(MessageEditorToolbarButton::BulletList)->click();
            auto* first=editor.textEdit()->textCursor().currentList();
            UISE_TEST_REQUIRE(first!=nullptr);
            UISE_TEST_CHECK_EQUAL(first->format().indent(),1);

            emit editor.toolbar()->indentIncreaseRequested();
            auto* deeper=editor.textEdit()->textCursor().currentList();
            UISE_TEST_REQUIRE(deeper!=nullptr);
            UISE_TEST_CHECK_EQUAL(deeper->format().indent(),2);
            // A NEW list object, not the same one re-levelled: editing the shared format in
            // place would drag every sibling item down a level too.
            UISE_TEST_CHECK(deeper!=first);

            emit editor.toolbar()->indentDecreaseRequested();
            UISE_TEST_REQUIRE(editor.textEdit()->textCursor().currentList()!=nullptr);
            UISE_TEST_CHECK_EQUAL(editor.textEdit()->textCursor().currentList()->format().indent(),1);

            // Stepping out of the last level leaves the list entirely rather than leaving an
            // indent-0 list Qt draws without a marker.
            emit editor.toolbar()->indentDecreaseRequested();
            UISE_TEST_CHECK(editor.textEdit()->textCursor().currentList()==nullptr);
            UISE_TEST_CHECK_EQUAL(editor.textEdit()->textCursor().blockFormat().indent(),0);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestListIndentWidthMatchesViewer)
{
    // A message composed in the editor and the same message rendered in a bubble must indent
    // their lists identically -- side by side in the demo, a mismatch is the first thing the eye
    // catches. The two constants are duplicated so neither header includes the other; this is
    // what stops them drifting.
    UISE_TEST_CHECK_EQUAL(static_cast<int>(EnhancedTextEdit::DefaultListIndentWidth),
                          static_cast<int>(ChatMessageTextBrowser::DefaultListIndentWidth));
}

BOOST_AUTO_TEST_CASE(TestListIndentWidthIsAdjustable)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            // Half Qt's own 40px default, which is what makes a one-level list sit so far from
            // the margin. A document property, not anything CSS can reach.
            UISE_TEST_CHECK_EQUAL(static_cast<int>(editor.listIndentWidth()),
                                  static_cast<int>(EnhancedTextEdit::DefaultListIndentWidth));
            UISE_TEST_CHECK(EnhancedTextEdit::DefaultListIndentWidth<40.0);

            // Survives a content reload -- setMarkdown/setHtml/setPlainText all leave
            // QTextDocument::indentWidth() alone, so it never needs re-applying.
            editor.loadText(QStringLiteral("- a\n- b"),TextFormat::Markdown);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(editor.listIndentWidth()),
                                  static_cast<int>(EnhancedTextEdit::DefaultListIndentWidth));
            editor.setListIndentWidth(18);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(editor.listIndentWidth()),18);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestBlockquoteUsesClearPropertyOnRemoval)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            // See TestInlineCodeToggle's comment -- toggling off needs the toolbar expanded so
            // syncToolbarState() actually pushes the refreshed state back after the first click.
            editor.setExpanded(true);
            editor.loadText(QStringLiteral("quoted"),TextFormat::Plain);

            editor.toolbar()->button(MessageEditorToolbarButton::Blockquote)->click();
            UISE_TEST_CHECK(editor.textEdit()->textCursor().blockFormat().hasProperty(QTextFormat::BlockQuoteLevel));

            editor.toolbar()->button(MessageEditorToolbarButton::Blockquote)->click();
            // Must be a real property REMOVAL, not set-to-0 -- the markdown writer tests
            // hasProperty(), not the value.
            UISE_TEST_CHECK(!editor.textEdit()->textCursor().blockFormat().hasProperty(QTextFormat::BlockQuoteLevel));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestBlockquoteRoundTripsThroughToMarkdown)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.loadText(QStringLiteral("quoted line"),TextFormat::Plain);
            editor.toolbar()->button(MessageEditorToolbarButton::Blockquote)->click();
            UISE_TEST_CHECK(editor.text(TextFormat::Markdown).contains(QStringLiteral(">")));
        }
    );
}

namespace {

//! U+00A0, the character a paragraph indent is made of -- see MessageEditor::applyIndentStep().
const QChar NoBreakSpace(0x00a0);

//! Put the caret in a MessageEditor's document without selecting anything.
void placeCaret(MessageEditor& editor, int blockIndex, int offset=0)
{
    auto* textEdit=editor.textEdit();
    const auto block=textEdit->document()->findBlockByNumber(blockIndex);
    QTextCursor cursor(block);
    cursor.setPosition(block.position()+offset);
    textEdit->setTextCursor(cursor);
}

//! itemText() of one list item, e.g. "2." -- what the EDITOR actually draws in front of it, as
//! opposed to what the exported markdown claims.
QString itemMarker(MessageEditor& editor, int blockIndex)
{
    const auto block=editor.textEdit()->document()->findBlockByNumber(blockIndex);
    auto* list=block.textList();
    return list==nullptr ? QString{} : list->itemText(block);
}

QTextList* listOf(MessageEditor& editor, int blockIndex)
{
    return editor.textEdit()->document()->findBlockByNumber(blockIndex).textList();
}

//! Select from the start of block `from` to the end of block `to`, inclusive.
void selectBlocks(MessageEditor& editor, int from, int to)
{
    auto* textEdit=editor.textEdit();
    auto* document=textEdit->document();
    QTextCursor cursor(document->findBlockByNumber(from));
    const auto last=document->findBlockByNumber(to);
    cursor.setPosition(last.position()+last.length()-1,QTextCursor::KeepAnchor);
    textEdit->setTextCursor(cursor);
}

void pressTab(MessageEditor& editor, bool shift=false)
{
    QKeyEvent event(QEvent::KeyPress,
                    shift ? Qt::Key_Backtab : Qt::Key_Tab,
                    shift ? Qt::ShiftModifier : Qt::NoModifier);
    QApplication::sendEvent(editor.textEdit(),&event);
}

}

namespace {

//! Text of a code fence delimiter line, or empty. Mirrors the editor's own fenceRun().
bool isFenceLine(const QString& line)
{
    const auto trimmed=line.trimmed();
    return trimmed.startsWith(QStringLiteral("```")) || trimmed.startsWith(QStringLiteral("~~~"));
}

bool styledAsCode(MessageEditor& editor, int blockIndex)
{
    const auto formats=editor.textEdit()->document()
                           ->findBlockByNumber(blockIndex).layout()->formats();
    return !formats.isEmpty() && formats.first().format.fontFixedPitch();
}

}

BOOST_AUTO_TEST_CASE(TestCodeBlockButtonInsertsThreeVisibleLines)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.toolbar()->button(MessageEditorToolbarButton::CodeBlock)->click();

            // Three ordinary lines of TEXT and no block properties at all. The fences being in
            // the document is the whole point: they are visible, and a language tag can be typed
            // onto the opening one without leaving WYSIWYG.
            auto* document=editor.textEdit()->document();
            UISE_TEST_REQUIRE(document->blockCount()==3);
            UISE_TEST_CHECK(isFenceLine(document->findBlockByNumber(0).text()));
            UISE_TEST_CHECK(!document->findBlockByNumber(1).text().isEmpty());
            UISE_TEST_CHECK(isFenceLine(document->findBlockByNumber(2).text()));

            UISE_TEST_CHECK(!document->findBlockByNumber(0).blockFormat()
                                 .hasProperty(QTextFormat::BlockCodeFence));
            UISE_TEST_CHECK(!document->findBlockByNumber(1).blockFormat().nonBreakableLines());

            // Placeholder selected, so the first keystroke replaces it.
            UISE_TEST_CHECK(editor.textEdit()->textCursor().hasSelection());

            UISE_TEST_CHECK(editor.text(TextFormat::Markdown).contains(QStringLiteral("```")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestCodeFenceWithLanguageSurvivesExport)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(
                QStringLiteral("```cpp\nauto x = *p;\nif (a[i] < b) { }\n```"),TextFormat::Plain);

            // The headline case. Left to itself qtextmarkdownwriter turns these three paragraphs
            // into "\\```cpp", "auto x = \\*p;" and "if (a\\[i] < b) { }" -- and an ESCAPED fence
            // is not a fence, so the block silently stops being code at all. restoreCodeFences()
            // undoes exactly that, and only between fences.
            const auto markdown=editor.text(TextFormat::Markdown);
            UISE_TEST_CHECK(markdown.contains(QStringLiteral("```cpp")));
            UISE_TEST_CHECK(markdown.contains(QStringLiteral("auto x = *p;")));
            UISE_TEST_CHECK(markdown.contains(QStringLiteral("if (a[i] < b) { }")));
            UISE_TEST_CHECK(!markdown.contains(QLatin1Char('\\')));

            // Qt also writes a blank line between every pair of paragraphs, which inside a fence
            // would be content -- the code would arrive double-spaced.
            UISE_TEST_CHECK(!markdown.contains(QStringLiteral("```cpp\n\n")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestCodeFenceKeepsRealBackslashes)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(QStringLiteral("```\nprintf(\"\\n\");\n```"),TextFormat::Plain);

            // The un-escape is an exact inverse rather than a heuristic: the writer DOUBLES every
            // backslash already present, so one round of un-escaping restores it rather than
            // eating it.
            UISE_TEST_CHECK(editor.text(TextFormat::Markdown)
                                .contains(QStringLiteral("printf(\"\\n\");")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestProseOutsideFenceStaysEscaped)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(QStringLiteral("a [x] outside\n```\nc * d inside\n```"),
                            TextFormat::Plain);

            // Only text BETWEEN fences is touched. Ordinary prose keeps Qt's escaping, which is
            // what stops it being read as markup by the far end.
            const auto markdown=editor.text(TextFormat::Markdown);
            UISE_TEST_CHECK(markdown.contains(QStringLiteral("c * d inside")));
            UISE_TEST_CHECK(markdown.contains(QStringLiteral("\\[")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestMarkdownImportRestoresVisibleFences)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.textEdit()->setCodeBlockColor(QColor(0x44,0x44,0x44));

            // Qt's markdown importer CONSUMES the fences into block properties, so a code block
            // loaded into WYSIWYG used to have no "```" anywhere in the document and its language
            // tag vanished from view entirely. convertCodeBlocksToText() puts both back as text.
            editor.loadText(QStringLiteral("before\n\n```cpp\nauto x = *p;\n```\n\nafter\n"),
                            TextFormat::Markdown);

            const auto plain=editor.text(TextFormat::Plain);
            UISE_TEST_CHECK(plain.contains(QStringLiteral("```cpp")));
            UISE_TEST_CHECK(plain.contains(QStringLiteral("auto x = *p;")));

            auto* document=editor.textEdit()->document();
            UISE_TEST_REQUIRE(document->blockCount()==5);
            UISE_TEST_CHECK(!styledAsCode(editor,0));
            UISE_TEST_CHECK(styledAsCode(editor,1));
            UISE_TEST_CHECK(styledAsCode(editor,2));
            UISE_TEST_CHECK(styledAsCode(editor,3));
            UISE_TEST_CHECK(!styledAsCode(editor,4));

            // No property-based code block is left anywhere -- restoreCodeFences() is only safe
            // because of that: Qt writes its OWN fences with the content unescaped, so unescaping
            // those as well would eat real backslashes.
            for (auto block=document->begin(); block!=document->end(); block=block.next())
            {
                UISE_TEST_CHECK(!block.blockFormat().hasProperty(QTextFormat::BlockCodeFence));
                UISE_TEST_CHECK(!block.blockFormat().nonBreakableLines());
            }

            // ...and it exports back to what came in.
            const auto markdown=editor.text(TextFormat::Markdown);
            UISE_TEST_CHECK(markdown.contains(QStringLiteral("```cpp")));
            UISE_TEST_CHECK(markdown.contains(QStringLiteral("auto x = *p;")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestCodeFenceHighlightIsDisplayOnly)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.textEdit()->setCodeBlockColor(QColor(0x44,0x44,0x44));
            editor.loadText(QStringLiteral("```\nint x = 1;\n```"),TextFormat::Plain);

            UISE_TEST_CHECK(styledAsCode(editor,1));

            // Same terms as the blockquote colour: nothing is written into the document, so
            // nothing is baked in, nothing leaks into an export, and a theme switch is free.
            const auto fragment=editor.textEdit()->document()
                                    ->findBlockByNumber(1).begin().fragment();
            UISE_TEST_CHECK(!fragment.charFormat().fontFixedPitch());
            UISE_TEST_CHECK(!fragment.charFormat().hasProperty(QTextFormat::ForegroundBrush));
            UISE_TEST_CHECK(!editor.text(TextFormat::Html).contains(QStringLiteral("#444444")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestCodeBlockRoundTripsThroughToMarkdown)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.loadText(QStringLiteral("int x = 1;"),TextFormat::Plain);
            editor.toolbar()->button(MessageEditorToolbarButton::CodeBlock)->click();
            UISE_TEST_CHECK(editor.text(TextFormat::Markdown).contains(QStringLiteral("`")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestTableInsertion)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            emit editor.toolbar()->tableRequested(2,3);

            // Read through the WIDGET's own cursor, not a fresh one: insertTable() repositions
            // only the cursor copy it was handed, so this also pins down that applyTable()
            // assigns that copy back and the visible caret really does land in the first cell.
            auto* table=editor.textEdit()->textCursor().currentTable();
            UISE_TEST_CHECK(table!=nullptr);
            if (table!=nullptr)
            {
                UISE_TEST_CHECK_EQUAL(table->rows(),2);
                UISE_TEST_CHECK_EQUAL(table->columns(),3);

                // An empty table with the default format has no border and no width -- i.e. it is
                // invisible, which is exactly what a user reported. Both must be set.
                auto format=table->format();
                UISE_TEST_CHECK(format.border()>0);
                UISE_TEST_CHECK(format.borderStyle()==QTextFrameFormat::BorderStyle_Solid);
                UISE_TEST_CHECK(format.width().type()==QTextLength::PercentageLength);
                UISE_TEST_CHECK_EQUAL(static_cast<int>(format.width().rawValue()),100);

                // Regression guard, and NOT a stylistic preference: with border-collapse on, Qt
                // paints the grid only from per-CELL border properties (drawTableCell() gates
                // drawTableCellBorder() on those), while the path that draws every cell from the
                // TABLE's own border is itself gated on !borderCollapse -- so a collapsed table
                // carrying only a table-level border draws nothing whatsoever (measured: 0 border
                // pixels vs 2720 with it off).
                //
                // Note this must be set EXPLICITLY in applyTable(): QTextTableFormat's
                // constructor turns border collapse ON, so merely not asking for it leaves it
                // enabled and the table invisible. Deleting the setBorderCollapse(false) call is
                // exactly the regression this line exists to catch.
                UISE_TEST_CHECK(!format.borderCollapse());

                // The gridline must actually be paintable -- a fully transparent brush would be
                // just as invisible as no border at all.
                UISE_TEST_CHECK(format.borderBrush().style()!=Qt::NoBrush);
                UISE_TEST_CHECK_EQUAL(format.borderBrush().color().alpha(),255);

                // Theme-NEUTRAL, not palette-derived: the brush is baked into the document at
                // insert time and never re-read, so a colour picked for the current background
                // leaves every existing table invisible after a theme switch (measured: 2120
                // border pixels before, 0 after). Equal channels are what makes one colour work
                // against both the light (#FFFFFF) and dark (#000000) editor backgrounds.
                auto grid=format.borderBrush().color();
                UISE_TEST_CHECK_EQUAL(grid.red(),grid.green());
                UISE_TEST_CHECK_EQUAL(grid.green(),grid.blue());
                UISE_TEST_CHECK(grid.red()>0 && grid.red()<255);
            }
        }
    );
}

BOOST_AUTO_TEST_CASE(TestEmptyTableColumnsStillExportAsATable)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            //! Find the one table in a document, or nullptr.
            auto onlyTable=[](const QTextDocument& document) -> QTextTable*
            {
                QTextTable* found=nullptr;
                for (auto it=document.rootFrame()->begin(); !it.atEnd(); ++it)
                {
                    auto* candidate=qobject_cast<QTextTable*>(it.currentFrame());
                    if (candidate!=nullptr)
                    {
                        found=candidate;
                    }
                }
                return found;
            };

            MessageEditor editor;
            emit editor.toolbar()->tableRequested(2,3);

            auto* table=editor.textEdit()->textCursor().currentTable();
            UISE_TEST_REQUIRE(table!=nullptr);
            table->cellAt(0,0).firstCursorPosition().insertText(QStringLiteral("1"));
            table->cellAt(1,1).firstCursorPosition().insertText(QStringLiteral("3"));

            // Qt's markdown writer sizes each column to its widest cell and writes exactly that
            // many characters -- ZERO for a column nothing was typed into. An empty CONTENT cell
            // is legal markdown, but an empty DELIMITER cell is not: the export came out as
            // "|1| ||" / "|-|-||" / "| |3||", which no parser reads as a table at all, so the
            // whole thing rendered in the bubble as a paragraph of literal pipes.
            //
            // Parsing the export back is therefore the assertion that matters, not the text: this
            // found zero tables before the fix.
            QTextDocument parsed;
            parsed.setMarkdown(editor.text(TextFormat::Markdown));

            auto* exported=onlyTable(parsed);
            UISE_TEST_REQUIRE(exported!=nullptr);
            UISE_TEST_CHECK_EQUAL(exported->rows(),2);
            UISE_TEST_CHECK_EQUAL(exported->columns(),3);

            auto cellText=[](QTextTable* from, int row, int column)
            {
                return from->cellAt(row,column).firstCursorPosition().block().text();
            };

            // What makes a space the right padding: markdown strips a cell's surrounding
            // whitespace, so the empty columns come back EMPTY rather than carrying a character
            // the author never typed, and the next export is identical to this one.
            UISE_TEST_CHECK_EQUAL_QSTR(cellText(exported,0,0),QStringLiteral("1"));
            UISE_TEST_CHECK(cellText(exported,0,1).isEmpty());
            UISE_TEST_CHECK(cellText(exported,0,2).isEmpty());
            UISE_TEST_CHECK(cellText(exported,1,0).isEmpty());
            UISE_TEST_CHECK_EQUAL_QSTR(cellText(exported,1,1),QStringLiteral("3"));
            UISE_TEST_CHECK(cellText(exported,1,2).isEmpty());

            // The padding goes on the export CLONE. The document the user is editing must not
            // gain characters, and their undo stack must not gain a step.
            UISE_TEST_CHECK(cellText(table,0,1).isEmpty());
            UISE_TEST_CHECK(cellText(table,0,2).isEmpty());

            // A table with nothing in it at all is the same bug at its worst -- every one of its
            // rows is written as "|||", delimiter included -- and it is one keystroke away: insert
            // a table, switch to Markdown mode.
            MessageEditor blank;
            emit blank.toolbar()->tableRequested(2,2);

            QTextDocument parsedBlank;
            parsedBlank.setMarkdown(blank.text(TextFormat::Markdown));

            auto* exportedBlank=onlyTable(parsedBlank);
            UISE_TEST_REQUIRE(exportedBlank!=nullptr);
            UISE_TEST_CHECK_EQUAL(exportedBlank->rows(),2);
            UISE_TEST_CHECK_EQUAL(exportedBlank->columns(),2);

            // A table with no empty column is untouched: its export is byte-identical with and
            // without the padding pass.
            MessageEditor full;
            emit full.toolbar()->tableRequested(2,2);
            auto* fullTable=full.textEdit()->textCursor().currentTable();
            UISE_TEST_REQUIRE(fullTable!=nullptr);
            fullTable->cellAt(0,0).firstCursorPosition().insertText(QStringLiteral("a"));
            fullTable->cellAt(1,1).firstCursorPosition().insertText(QStringLiteral("d"));
            UISE_TEST_CHECK(full.text(TextFormat::Markdown)
                                .contains(QStringLiteral("|a| |\n|-|-|\n| |d|")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestInsertedTableWidthIsConfigurable)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            // 95, not 100: a 100% table lands at exactly the viewport width with no slack, so
            // any later pixel (a vertical scrollbar appearing, host border/padding) raises a
            // horizontal scrollbar under it.
            UISE_TEST_CHECK_EQUAL(editor.insertedTableWidthPercent(),
                                  AbstractMessageEditor::DefaultInsertedTableWidthPercent);
            UISE_TEST_CHECK(AbstractMessageEditor::DefaultInsertedTableWidthPercent<100);

            editor.setInsertedTableWidthPercent(60);
            emit editor.toolbar()->tableRequested(2,3);
            auto* narrow=editor.textEdit()->textCursor().currentTable();
            UISE_TEST_REQUIRE(narrow!=nullptr);
            UISE_TEST_CHECK(narrow->format().width().type()==QTextLength::PercentageLength);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(narrow->format().width().rawValue()),60);

            // Above 100 the table would overflow the text area with no way to scroll to it.
            editor.clear();
            editor.setInsertedTableWidthPercent(150);
            emit editor.toolbar()->tableRequested(2,2);
            auto* clamped=editor.textEdit()->textCursor().currentTable();
            UISE_TEST_REQUIRE(clamped!=nullptr);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(clamped->format().width().rawValue()),100);

            // 0 opts into Qt's own content sizing -- no width property set at all.
            editor.clear();
            editor.setInsertedTableWidthPercent(0);
            emit editor.toolbar()->tableRequested(2,2);
            auto* sized=editor.textEdit()->textCursor().currentTable();
            UISE_TEST_REQUIRE(sized!=nullptr);
            UISE_TEST_CHECK(sized->format().width().type()!=QTextLength::PercentageLength);

            // The border format is still applied whatever the width -- an unbordered table is
            // invisible regardless of how wide it is.
            UISE_TEST_CHECK(sized->format().border()>0);
            UISE_TEST_CHECK(!sized->format().borderCollapse());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestTableRowsAndColumnsEditableInPlace)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            emit editor.toolbar()->tableRequested(2,2);

            auto* textEdit=editor.textEdit();
            auto* table=textEdit->textCursor().currentTable();
            UISE_TEST_REQUIRE(table!=nullptr);
            UISE_TEST_CHECK_EQUAL(table->rows(),2);
            UISE_TEST_CHECK_EQUAL(table->columns(),2);

            // Growing in place is what covers every size the fixed presets do not -- the
            // dimensions never have to be known before the table is created.
            emit editor.toolbar()->tableActionRequested(MessageEditorTableAction::InsertRowBelow);
            emit editor.toolbar()->tableActionRequested(MessageEditorTableAction::InsertColumnRight);
            UISE_TEST_CHECK_EQUAL(table->rows(),3);
            UISE_TEST_CHECK_EQUAL(table->columns(),3);

            emit editor.toolbar()->tableActionRequested(MessageEditorTableAction::InsertRowAbove);
            emit editor.toolbar()->tableActionRequested(MessageEditorTableAction::InsertColumnLeft);
            UISE_TEST_CHECK_EQUAL(table->rows(),4);
            UISE_TEST_CHECK_EQUAL(table->columns(),4);

            emit editor.toolbar()->tableActionRequested(MessageEditorTableAction::RemoveRow);
            UISE_TEST_CHECK_EQUAL(table->rows(),3);
            emit editor.toolbar()->tableActionRequested(MessageEditorTableAction::RemoveColumn);
            UISE_TEST_CHECK_EQUAL(table->columns(),3);

            // ...and the whole table goes in one action, without whittling it down row by row.
            emit editor.toolbar()->tableActionRequested(MessageEditorTableAction::RemoveTable);
            UISE_TEST_CHECK(textEdit->textCursor().currentTable()==nullptr);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestTableActionsGatedOnBeingInsideATable)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setExpanded(true);
            editor.loadText(QStringLiteral("plain text"),TextFormat::Plain);

            // Outside a table the edits are meaningless: the state says so, and applying one
            // anyway must be a no-op rather than a crash -- a host may wire the signal itself.
            UISE_TEST_CHECK(!editor.toolbar()->formatState().insideTable);
            emit editor.toolbar()->tableActionRequested(MessageEditorTableAction::RemoveRow);
            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Plain),QStringLiteral("plain text"));

            editor.clear();
            emit editor.toolbar()->tableRequested(2,2);
            UISE_TEST_CHECK(editor.toolbar()->formatState().insideTable);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestRemovingLastRowRemovesTheWholeTable)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            emit editor.toolbar()->tableRequested(1,2);

            auto* textEdit=editor.textEdit();
            UISE_TEST_REQUIRE(textEdit->textCursor().currentTable()!=nullptr);

            // Qt keeps a 0-row table in the document as an invisible husk the caret can still
            // enter, so deleting the only row has to take the table with it.
            emit editor.toolbar()->tableActionRequested(MessageEditorTableAction::RemoveRow);
            UISE_TEST_CHECK(textEdit->textCursor().currentTable()==nullptr);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestTabNavigatesTableCells)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            emit editor.toolbar()->tableRequested(2,2);

            auto* textEdit=editor.textEdit();
            auto* table=textEdit->textCursor().currentTable();
            UISE_TEST_REQUIRE(table!=nullptr);

            auto cellAt=[table,textEdit]()
            {
                auto cell=table->cellAt(textEdit->textCursor());
                return std::make_pair(cell.row(),cell.column());
            };

            UISE_TEST_CHECK(cellAt()==std::make_pair(0,0));

            // Qt itself has no NextCell/PreviousCell key handling anywhere (verified against
            // QWidgetTextControl and QTextEdit), so without EnhancedTextEdit's own handler Tab
            // would just insert a tab character and leave the caret in cell (0,0).
            QKeyEvent tab(QEvent::KeyPress,Qt::Key_Tab,Qt::NoModifier);
            QApplication::sendEvent(textEdit,&tab);
            UISE_TEST_CHECK(cellAt()==std::make_pair(0,1));

            QKeyEvent backtab(QEvent::KeyPress,Qt::Key_Backtab,Qt::ShiftModifier);
            QApplication::sendEvent(textEdit,&backtab);
            UISE_TEST_CHECK(cellAt()==std::make_pair(0,0));

            // At the first cell there is nowhere back to go: movePosition() fails and the caret
            // stays put rather than wrapping or leaving the table.
            QApplication::sendEvent(textEdit,&backtab);
            UISE_TEST_CHECK(cellAt()==std::make_pair(0,0));
        }
    );
}


BOOST_AUTO_TEST_CASE(TestTabNeverInsertsTabCharacter)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.loadText(QStringLiteral("plain"),TextFormat::Plain);

            auto* textEdit=editor.textEdit();
            UISE_TEST_REQUIRE(textEdit->textCursor().currentTable()==nullptr);

            // The whole point of the change. A literal tab reaching the document is what made a
            // leading Tab render as an indented CODE block in the chat bubble, and a mid-line one
            // collapse to a single space -- so Tab is consumed outside a table too, never
            // falling through to QTextEdit's own "insert a tab character" handling.
            pressTab(editor);
            UISE_TEST_CHECK(!editor.text(TextFormat::Plain).contains(QChar('\t')));

            pressTab(editor,true);
            UISE_TEST_CHECK(!editor.text(TextFormat::Plain).contains(QChar('\t')));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestTabIndentsParagraphWithNoBreakSpaces)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(QStringLiteral("hello"),TextFormat::Plain);
            placeCaret(editor,0);

            pressTab(editor);
            UISE_TEST_CHECK(editor.text(TextFormat::Plain).startsWith(
                QString(MessageEditor::DefaultParagraphIndentSpaces,NoBreakSpace)));

            // Steps stack.
            pressTab(editor);
            UISE_TEST_CHECK(editor.text(TextFormat::Plain).startsWith(
                QString(2*MessageEditor::DefaultParagraphIndentSpaces,NoBreakSpace)));

            pressTab(editor,true);
            UISE_TEST_CHECK(editor.text(TextFormat::Plain).startsWith(
                QString(MessageEditor::DefaultParagraphIndentSpaces,NoBreakSpace)));

            // An outdent past zero removes only indentation it could have added, never the
            // line's own first characters.
            pressTab(editor,true);
            pressTab(editor,true);
            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Plain),QStringLiteral("hello"));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestTabInsertsAtCaretNotLineStart)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(QStringLiteral("alpha beta"),TextFormat::Plain);
            placeCaret(editor,0,5);

            // Tab is a "widen the gap here" gesture as much as a "shift this line right" one, so
            // pressed mid-sentence it has to act mid-sentence.
            pressTab(editor);

            // Local, not inline in the macro: UISE_TEST_CHECK_EQUAL_QSTR appends ".toStdString()"
            // to its argument, which would bind to the last operand of a "+" expression alone.
            const auto expected=QStringLiteral("alpha")
                +QString(MessageEditor::DefaultParagraphIndentSpaces,NoBreakSpace)
                +QStringLiteral(" beta");
            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Plain),expected);
            UISE_TEST_CHECK(!editor.text(TextFormat::Plain).startsWith(NoBreakSpace));

            // The caret follows what it inserted, so typing continues after the gap.
            UISE_TEST_CHECK_EQUAL(editor.textEdit()->textCursor().position(),
                                  5+MessageEditor::DefaultParagraphIndentSpaces);

            // Outdent is the exact inverse: it takes back only no-break spaces sitting directly
            // behind the caret, never the line's real characters.
            pressTab(editor,true);
            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Plain),
                                       QStringLiteral("alpha beta"));
            pressTab(editor,true);
            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Plain),
                                       QStringLiteral("alpha beta"));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestPlainTextExportPreservesNoBreakSpaceIndent)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Plaintext);
            editor.loadText(QStringLiteral("alpha"),TextFormat::Plain);
            placeCaret(editor,0,0);
            pressTab(editor);

            // The export boundary, and a trap worth a test of its own: QTextDocument::toPlainText()
            // is DOCUMENTED to replace U+00A0 with an ordinary space. Reaching for it here would
            // hand the caller four leading ORDINARY spaces -- markdown's indented-code-block
            // syntax, i.e. the very bug the no-break space exists to avoid -- silently, and in
            // Plaintext mode, which is the mode a real composer runs in.
            UISE_TEST_CHECK(editor.text(TextFormat::Plain).startsWith(
                QString(MessageEditor::DefaultParagraphIndentSpaces,NoBreakSpace)));
            UISE_TEST_CHECK(!editor.text(TextFormat::Plain).startsWith(QStringLiteral("    ")));

            // ...while still normalizing block separators, which is the whole reason toRawText()
            // is not a drop-in replacement for toPlainText().
            editor.loadText(QStringLiteral("one\ntwo"),TextFormat::Plain);
            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Plain),
                                       QStringLiteral("one\ntwo"));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestParagraphIndentSurvivesMarkdownAsOrdinaryText)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(QStringLiteral("hello"),TextFormat::Plain);
            placeCaret(editor,0);
            pressTab(editor);

            // The reason for U+00A0 rather than ordinary spaces, asserted rather than described:
            // four leading ORDINARY spaces are markdown's indented-code-block syntax, so the
            // exported markdown must carry no such run. A no-break space has no markdown meaning
            // and is not whitespace for CommonMark's indentation rules.
            const auto markdown=editor.text(TextFormat::Markdown);
            UISE_TEST_CHECK(markdown.contains(NoBreakSpace));
            UISE_TEST_CHECK(!markdown.startsWith(QStringLiteral("    ")));
            UISE_TEST_CHECK(!markdown.startsWith(QChar('\t')));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestTabOnSelectionBlockquotesParagraphs)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(QStringLiteral("one\n\ntwo"),TextFormat::Markdown);

            auto* document=editor.textEdit()->document();
            UISE_TEST_REQUIRE(document->blockCount()>=2);

            selectBlocks(editor,0,document->blockCount()-1);
            pressTab(editor);

            // Markdown has exactly one portable way to indent a run of paragraphs, and
            // BlockQuoteLevel is what round-trips as it.
            for (auto block=document->begin(); block!=document->end(); block=block.next())
            {
                UISE_TEST_CHECK_EQUAL(
                    block.blockFormat().intProperty(QTextFormat::BlockQuoteLevel),1);
            }
            UISE_TEST_CHECK(editor.text(TextFormat::Markdown).contains(QStringLiteral("> one")));

            selectBlocks(editor,0,document->blockCount()-1);
            pressTab(editor,true);

            // clearProperty(), not a zero value: qtextmarkdownwriter tests hasProperty(), so a
            // present-but-zero level would still export a "> " prefix.
            for (auto block=document->begin(); block!=document->end(); block=block.next())
            {
                UISE_TEST_CHECK(
                    !block.blockFormat().hasProperty(QTextFormat::BlockQuoteLevel));
            }
            UISE_TEST_CHECK(!editor.text(TextFormat::Markdown).contains(QChar('>')));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestBlockquoteIsVisiblyIndented)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(QStringLiteral("quote me"),TextFormat::Plain);

            auto blockFormat=[&editor]()
            {
                return editor.textEdit()->document()->firstBlock().blockFormat();
            };
            UISE_TEST_CHECK_EQUAL(static_cast<int>(blockFormat().leftMargin()),0);

            emit editor.toolbar()->blockquoteRequested(true);

            // QTextFormat::BlockQuoteLevel on its own draws NOTHING -- it is export metadata that
            // qtextmarkdownwriter reads, and Qt's layout ignores it. Setting only the property is
            // why a blockquote applied here used to have no visual effect at all, while the same
            // document reloaded from markdown came back indented.
            UISE_TEST_CHECK_EQUAL(static_cast<int>(blockFormat().leftMargin()),
                                  static_cast<int>(MessageEditor::DefaultBlockquoteIndent));
            UISE_TEST_CHECK_EQUAL(blockFormat().intProperty(QTextFormat::BlockQuoteLevel),1);
            UISE_TEST_CHECK(editor.text(TextFormat::Markdown).contains(QStringLiteral("> quote me")));

            emit editor.toolbar()->blockquoteRequested(false);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(blockFormat().leftMargin()),0);
            UISE_TEST_CHECK(!blockFormat().hasProperty(QTextFormat::BlockQuoteLevel));
        }
    );
}

namespace {

int ruleCount(const QTextDocument* document)
{
    int count=0;
    for (auto block=document->begin(); block!=document->end(); block=block.next())
    {
        if (block.blockFormat().hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth))
        {
            ++count;
        }
    }
    return count;
}

}

BOOST_AUTO_TEST_CASE(TestHorizontalRuleInsertsOwnBlockAndExports)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(QStringLiteral("above"),TextFormat::Plain);

            auto* document=editor.textEdit()->document();
            emit editor.toolbar()->horizontalRuleRequested();

            UISE_TEST_CHECK_EQUAL(ruleCount(document),1);
            UISE_TEST_CHECK_EQUAL(document->blockCount(),3);

            // The rule's own block is left EMPTY on purpose: Qt draws the ruler along the bottom
            // edge of its block, except in an empty one where it is centred
            // (qtextdocumentlayout.cpp) -- the centred form is the one that reads as a rule
            // rather than as an underline beneath the previous line's text.
            UISE_TEST_CHECK(document->findBlockByNumber(1).text().isEmpty());

            // Caret lands BELOW the rule on a block that did not inherit the property, so the
            // next thing typed is ordinary text and not a second rule.
            UISE_TEST_CHECK_EQUAL(editor.textEdit()->textCursor().blockNumber(),2);
            UISE_TEST_CHECK(!editor.textEdit()->textCursor().blockFormat()
                                 .hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth));

            editor.textEdit()->textCursor().insertText(QStringLiteral("below"));
            UISE_TEST_CHECK_EQUAL(ruleCount(document),1);
            UISE_TEST_CHECK(editor.text(TextFormat::Markdown).contains(QStringLiteral("- - -")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestHorizontalRuleIsOneUndoableAction)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(QStringLiteral("above"),TextFormat::Plain);

            const auto markdownBefore=editor.text(TextFormat::Markdown);
            emit editor.toolbar()->horizontalRuleRequested();
            UISE_TEST_REQUIRE(ruleCount(editor.textEdit()->document())==1);

            // Deliberately asserted as behaviour rather than as a step count:
            // QTextDocument::availableUndoSteps() counts RAW undo items, not user-visible
            // actions (measured -- this grouped three-edit action reports four of them). What
            // begin/endEditBlock actually buys is that ONE undo takes the whole rule back out,
            // and that is what a user would notice if it broke.
            editor.textEdit()->undo();
            UISE_TEST_CHECK_EQUAL(ruleCount(editor.textEdit()->document()),0);
            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Markdown),markdownBefore);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestHorizontalRuleDoesNotInheritSurroundingBlock)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(QStringLiteral("- alpha\n- beta\n"),TextFormat::Markdown);

            placeCaret(editor,1);
            auto cursor=editor.textEdit()->textCursor();
            cursor.movePosition(QTextCursor::EndOfBlock);
            editor.textEdit()->setTextCursor(cursor);

            emit editor.toolbar()->horizontalRuleRequested();

            // A fresh QTextBlockFormat, never a copy of the caret's own: inherited, the rule
            // would arrive as a list ITEM, complete with bullet and indent. List membership
            // travels in the block format's object index, so starting clean drops it too.
            const auto rule=editor.textEdit()->document()->findBlockByNumber(2);
            UISE_TEST_REQUIRE(rule.blockFormat()
                                  .hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth));
            UISE_TEST_CHECK(rule.textList()==nullptr);
            UISE_TEST_CHECK_EQUAL(rule.blockFormat().indent(),0);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(rule.blockFormat().leftMargin()),0);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestHorizontalRuleButtonIsGreyedOutsideWysiwyg)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            auto* toolbar=editor.toolbar();

            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            UISE_TEST_CHECK(toolbar->isButtonEnabled(MessageEditorToolbarButton::HorizontalRule));

            // Formatting is WYSIWYG-only (Stage 5a), and the rule button has to be in that set
            // like every other block insert -- the enum-ordered FormattingButtons table is easy
            // to extend the enum without.
            editor.setMessageEditingMode(MessageEditingMode::Markdown);
            UISE_TEST_CHECK(!toolbar->isButtonEnabled(MessageEditorToolbarButton::HorizontalRule));

            editor.setMessageEditingMode(MessageEditingMode::Plaintext);
            UISE_TEST_CHECK(!toolbar->isButtonEnabled(MessageEditorToolbarButton::HorizontalRule));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestBlockquoteColorPaintsWithoutTouchingDocument)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(QStringLiteral("plain\n\nquote me"),TextFormat::Markdown);

            auto* textEdit=editor.textEdit();
            auto* document=textEdit->document();
            UISE_TEST_REQUIRE(document->blockCount()>=2);

            const auto quotedIndex=document->blockCount()-1;
            placeCaret(editor,quotedIndex);

            const auto markdownBefore=editor.text(TextFormat::Markdown);
            const auto undoStepsBefore=document->availableUndoSteps();

            textEdit->setBlockquoteColor(QColor(0x66,0x66,0x66));
            emit editor.toolbar()->blockquoteRequested(true);

            auto painted=[document](int index)
            {
                const auto formats=document->findBlockByNumber(index).layout()->formats();
                return formats.isEmpty() ? QColor() : formats.first().format.foreground().color();
            };

            // The colour lands as a LAYOUT format...
            UISE_TEST_CHECK(painted(quotedIndex)==QColor(0x66,0x66,0x66));
            UISE_TEST_CHECK(!painted(0).isValid());

            // ...and nowhere else. All three measured, because all three are what make colouring
            // safe here at all: the document's own char formats stay clean, so nothing is baked
            // in and frozen at the theme it was typed in; the exported message carries no colour;
            // and the user's undo history gains no invisible entries.
            const auto quoted=document->findBlockByNumber(quotedIndex);
            UISE_TEST_CHECK(!quoted.begin().fragment().charFormat()
                                .hasProperty(QTextFormat::ForegroundBrush));
            UISE_TEST_CHECK(!editor.text(TextFormat::Html).contains(QStringLiteral("#666666")));

            // One undo step for the blockquote itself -- the real edit -- and not one more.
            UISE_TEST_CHECK_EQUAL(document->availableUndoSteps(),undoStepsBefore+1);

            // A theme switch recolours with no document edit whatsoever.
            const auto markdownQuoted=editor.text(TextFormat::Markdown);
            const auto undoStepsQuoted=document->availableUndoSteps();
            textEdit->setBlockquoteColor(QColor(0xBB,0xBB,0xBB));
            UISE_TEST_CHECK(painted(quotedIndex)==QColor(0xBB,0xBB,0xBB));
            UISE_TEST_CHECK_EQUAL(document->availableUndoSteps(),undoStepsQuoted);
            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Markdown),markdownQuoted);

            // Unquoting drops the colour with it.
            emit editor.toolbar()->blockquoteRequested(false);
            UISE_TEST_CHECK(!painted(quotedIndex).isValid());
            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Markdown),markdownBefore);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestBlockquoteColorIsInertUntilStyled)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(QStringLiteral("quote me"),TextFormat::Plain);

            // The default is an INVALID colour, which is what a host shipping no stylesheet gets:
            // quoted text stays in the ordinary text colour and the indent alone marks the quote.
            // Anything else would hardcode a colour that may be invisible on the host's own
            // background.
            UISE_TEST_CHECK(!editor.textEdit()->blockquoteColor().isValid());

            emit editor.toolbar()->blockquoteRequested(true);
            const auto formats=editor.textEdit()->document()->firstBlock().layout()->formats();
            UISE_TEST_CHECK(formats.isEmpty());

            // ...but the indent is still applied, so the quote is never invisible.
            UISE_TEST_CHECK_EQUAL(
                static_cast<int>(editor.textEdit()->document()->firstBlock()
                                     .blockFormat().leftMargin()),
                static_cast<int>(MessageEditor::DefaultBlockquoteIndent));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestBlockquoteIndentSurvivesMarkdownRoundTrip)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);

            // Qt's markdown importer hardcodes a flat 40px per level plus a 40px RIGHT margin
            // (qtextmarkdownimporter.cpp). Left alone that is a second, wider rendering of the
            // same construct, which is what "switch to markdown and back and the indent changes"
            // actually was. normalizeBlockquoteIndent() re-indents on the way in.
            editor.loadText(QStringLiteral("> quoted line\n"),TextFormat::Markdown);

            const auto blockFormat=editor.textEdit()->document()->firstBlock().blockFormat();
            UISE_TEST_CHECK_EQUAL(blockFormat.intProperty(QTextFormat::BlockQuoteLevel),1);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(blockFormat.leftMargin()),
                                  static_cast<int>(MessageEditor::DefaultBlockquoteIndent));

            // The right margin only narrows a composer, so it is dropped rather than mirrored.
            UISE_TEST_CHECK_EQUAL(static_cast<int>(blockFormat.rightMargin()),0);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestBlockquoteIndentMatchesListIndentWidth)
{
    // A quote and a list at the same depth must line up, in the composer and in the bubble alike.
    // Three constants say so independently -- these two, and the "margin-left" in the blockquote
    // rule of resources/style/messagetext.css, which no test can reach. Keep all three in step.
    UISE_TEST_CHECK_EQUAL(static_cast<int>(MessageEditor::DefaultBlockquoteIndent),
                          static_cast<int>(EnhancedTextEdit::DefaultListIndentWidth));
}

BOOST_AUTO_TEST_CASE(TestTabStepsBlockquoteIndentPerLevel)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(QStringLiteral("one\n\ntwo"),TextFormat::Markdown);

            auto* document=editor.textEdit()->document();
            selectBlocks(editor,0,document->blockCount()-1);
            pressTab(editor);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(document->firstBlock().blockFormat().leftMargin()),
                                  static_cast<int>(MessageEditor::DefaultBlockquoteIndent));

            selectBlocks(editor,0,document->blockCount()-1);
            pressTab(editor);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(document->firstBlock().blockFormat().leftMargin()),
                                  static_cast<int>(2*MessageEditor::DefaultBlockquoteIndent));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestTabStepsListLevel)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(QStringLiteral("- alpha\n- beta\n"),TextFormat::Markdown);
            placeCaret(editor,1);

            auto* list=editor.textEdit()->textCursor().currentList();
            UISE_TEST_REQUIRE(list!=nullptr);
            UISE_TEST_CHECK_EQUAL(list->format().indent(),1);

            pressTab(editor);
            UISE_TEST_CHECK_EQUAL(
                editor.textEdit()->textCursor().currentList()->format().indent(),2);

            pressTab(editor,true);
            UISE_TEST_CHECK_EQUAL(
                editor.textEdit()->textCursor().currentList()->format().indent(),1);

            // Stepping out of level 1 leaves the list entirely rather than leaving an indent-0
            // list, which Qt renders with no marker and which nothing can step back into.
            pressTab(editor,true);
            UISE_TEST_CHECK(editor.textEdit()->textCursor().currentList()==nullptr);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestOrderedListNumberingSurvivesMultiItemIndent)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(QStringLiteral("1. alpha\n2. beta\n3. gamma\n"),TextFormat::Markdown);

            auto* document=editor.textEdit()->document();
            selectBlocks(editor,0,document->blockCount()-1);
            pressTab(editor);

            // Measured, and the reason stepListLevel() groups a contiguous run of items into ONE
            // new QTextList: QTextCursor::createList() builds a fresh list object per call and an
            // ordered list restarts at 1 for each, so re-leveling item by item exports
            // "1. 1. 1.". A bullet list hides this completely.
            const auto markdown=editor.text(TextFormat::Markdown);
            UISE_TEST_CHECK(markdown.contains(QStringLiteral("2.")));
            UISE_TEST_CHECK(markdown.contains(QStringLiteral("3.")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestOrderedListNumberingRestoredOnOutdent)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(QStringLiteral("1. alpha\n2. beta\n3. gamma\n"),TextFormat::Markdown);

            placeCaret(editor,1);
            pressTab(editor);            // nest "beta"
            placeCaret(editor,1);
            pressTab(editor,true);       // and put it straight back

            // The reported bug: the outdent used to drop "beta" into a list of ITS OWN at level 1,
            // so the editor drew "1. alpha / 1. beta / 2. gamma" while the bubble showed 1/2/3
            // (md4c merges adjacent same-level items on render, which is what hid it). Joining the
            // neighbouring list instead renumbers positionally -- measured.
            UISE_TEST_CHECK(listOf(editor,0)==listOf(editor,1));
            UISE_TEST_CHECK(listOf(editor,1)==listOf(editor,2));
            UISE_TEST_CHECK(itemMarker(editor,1).startsWith(QStringLiteral("2")));
            UISE_TEST_CHECK(itemMarker(editor,2).startsWith(QStringLiteral("3")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestIndentingSiblingsOneAtATimeJoinsOneList)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(QStringLiteral("1. alpha\n2. beta\n3. gamma\n"),TextFormat::Markdown);

            // Same defect a level down: nesting two siblings in separate keystrokes used to build
            // two one-item lists, both numbered "1.".
            placeCaret(editor,2);
            pressTab(editor);
            placeCaret(editor,1);
            pressTab(editor);

            UISE_TEST_CHECK(listOf(editor,1)==listOf(editor,2));
            UISE_TEST_CHECK(itemMarker(editor,2).startsWith(QStringLiteral("2")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestListLevelStepJoinsOnlyMatchingStyle)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(QStringLiteral("- alpha\n- beta\n"),TextFormat::Markdown);

            placeCaret(editor,1);
            pressTab(editor);

            // A neighbour is only joined when its style matches too, so a bullet item can never be
            // absorbed into an ordered list (or the reverse) by a level step.
            UISE_TEST_REQUIRE(listOf(editor,1)!=nullptr);
            UISE_TEST_CHECK_EQUAL(static_cast<int>(listOf(editor,1)->format().style()),
                                  static_cast<int>(QTextListFormat::ListDisc));
            UISE_TEST_CHECK_EQUAL(listOf(editor,1)->format().indent(),2);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestMarkdownModeTabIndentsListSource)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Markdown);
            editor.loadText(QStringLiteral("- alpha\n- beta"),TextFormat::Markdown);
            placeCaret(editor,1);

            // In Markdown mode the document holds SOURCE, so a nesting step is a source edit:
            // markdown spells nesting with leading spaces, and those are real spaces on purpose.
            pressTab(editor);
            UISE_TEST_CHECK(editor.text(TextFormat::Markdown).contains(
                QStringLiteral("\n  - beta")));

            pressTab(editor,true);
            UISE_TEST_CHECK(editor.text(TextFormat::Markdown).contains(
                QStringLiteral("\n- beta")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestMarkdownModeTabOnSelectionAddsQuotePrefix)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Markdown);
            editor.loadText(QStringLiteral("one\ntwo"),TextFormat::Markdown);

            selectBlocks(editor,0,1);
            pressTab(editor);
            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Markdown),
                                       QStringLiteral("> one\n> two"));

            selectBlocks(editor,0,1);
            pressTab(editor,true);
            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Markdown),
                                       QStringLiteral("one\ntwo"));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestPlaintextModeTabNeverAddsMarkup)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Plaintext);
            editor.loadText(QStringLiteral("- alpha\n- beta"),TextFormat::Plain);

            // Plaintext mode's whole definition is that nothing in the document carries markup
            // meaning: a leading "- " is a hyphen, not a list. So neither the list branch nor the
            // blockquote branch may fire here -- only literal indent characters.
            placeCaret(editor,0);
            pressTab(editor);
            UISE_TEST_CHECK(editor.text(TextFormat::Plain).startsWith(
                QString(MessageEditor::DefaultParagraphIndentSpaces,NoBreakSpace)));

            selectBlocks(editor,0,1);
            pressTab(editor);
            UISE_TEST_CHECK(!editor.text(TextFormat::Plain).contains(QChar('>')));

            // With no blockquote to fall back on, a selected run of lines has to be indented
            // line by line here or not at all.
            const auto lines=editor.text(TextFormat::Plain).split(QChar('\n'));
            UISE_TEST_REQUIRE(lines.size()==2);
            UISE_TEST_CHECK(lines.at(1).startsWith(NoBreakSpace));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestParagraphIndentSpacesConfigurable)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            UISE_TEST_CHECK_EQUAL(editor.paragraphIndentSpaces(),
                                  MessageEditor::DefaultParagraphIndentSpaces);

            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.setParagraphIndentSpaces(2);
            editor.loadText(QStringLiteral("hello"),TextFormat::Plain);
            placeCaret(editor,0);
            pressTab(editor);

            UISE_TEST_CHECK(editor.text(TextFormat::Plain).startsWith(QString(2,NoBreakSpace)));
            UISE_TEST_CHECK(!editor.text(TextFormat::Plain).startsWith(QString(3,NoBreakSpace)));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestTabStopDistanceIsFontRelative)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            EnhancedTextEdit textEdit;

            // Qt's own default is a flat 80px that ignores the font entirely -- at this widget's
            // space width that is over twenty spaces per tab, which is what made one pasted tab
            // push the rest of the line out of view.
            const auto spaceWidth=QFontMetricsF(textEdit.font()).horizontalAdvance(QChar(' '));
            UISE_TEST_CHECK(qAbs(textEdit.tabStopDistance()
                                 -EnhancedTextEdit::DefaultTabStopSpaces*spaceWidth)<0.001);

            // QSS drives fonts here, so the ctor's one-time computation has to be redone on
            // every font change or it is stale from the first theme switch onward.
            auto font=textEdit.font();
            font.setPixelSize(40);
            textEdit.setFont(font);

            const auto grownSpace=QFontMetricsF(textEdit.font()).horizontalAdvance(QChar(' '));

            // Guards the guard: if the font had not actually changed, the check below would pass
            // against a stale tab stop and prove nothing.
            UISE_TEST_CHECK(grownSpace>spaceWidth);
            UISE_TEST_CHECK(qAbs(textEdit.tabStopDistance()
                                 -EnhancedTextEdit::DefaultTabStopSpaces*grownSpace)<0.001);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestClearFormattingStripsCharBlockAndList)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.loadText(QStringLiteral("word"),TextFormat::Plain);

            auto cursor=editor.textEdit()->textCursor();
            cursor.select(QTextCursor::Document);
            editor.textEdit()->setTextCursor(cursor);

            editor.toolbar()->button(MessageEditorToolbarButton::Bold)->click();
            editor.toolbar()->button(MessageEditorToolbarButton::BulletList)->click();

            editor.toolbar()->button(MessageEditorToolbarButton::ClearFormatting)->click();

            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Plain),QStringLiteral("word"));
            UISE_TEST_CHECK(editor.textEdit()->textCursor().currentList()==nullptr);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestChromeHiddenByDefaultCostsZeroHeight)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            UISE_TEST_CHECK(editor.expandButton()->isHidden());
            UISE_TEST_CHECK(editor.toolbar()->isHidden());
            UISE_TEST_CHECK(!editor.isExpandButtonVisible());
            UISE_TEST_CHECK(!editor.isExpanded());

            // The real guarantee: with both pieces of chrome hidden, the whole editor is exactly
            // as tall as its bare text edit -- a hidden widget is isEmpty() to a box layout, and
            // Layout::vertical()/Layout::clear() zero every margin and spacing, including the
            // nested row the expand button lives in. Comparing against the embedded text edit,
            // not against a second MessageEditor (which would be the same structure and so prove
            // nothing).
            UISE_TEST_CHECK_EQUAL(editor.sizeHint().height(),editor.textEdit()->sizeHint().height());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestExpandTogglesToolbarAndScrollbar)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setExpandButtonVisible(true);
            UISE_TEST_CHECK(!editor.expandButton()->isHidden());

            editor.setExpanded(true);
            UISE_TEST_CHECK(!editor.toolbar()->isHidden());
            UISE_TEST_CHECK(editor.textEdit()->verticalScrollBarPolicy()==Qt::ScrollBarAsNeeded);
            UISE_TEST_CHECK(!editor.textEdit()->isAutoResizingEnabled());

            editor.setExpanded(false);
            UISE_TEST_CHECK(editor.toolbar()->isHidden());
            UISE_TEST_CHECK(editor.textEdit()->verticalScrollBarPolicy()==Qt::ScrollBarAlwaysOff);
            UISE_TEST_CHECK(editor.textEdit()->isAutoResizingEnabled());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestExpandedPinsHeightAtEffectiveMax)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMaxHeight(220);
            // Enough content that the collapsed, content-driven height is nowhere near the
            // ceiling -- otherwise the two states could coincide by accident.
            editor.loadText(QStringLiteral("one line"),TextFormat::Plain);

            auto* textEdit=editor.textEdit();
            UISE_TEST_CHECK(textEdit->sizePolicy().verticalPolicy()==QSizePolicy::Expanding);

            editor.setExpanded(true);

            // Expanded mode must report the ceiling as a FIXED height. sizeHint() alone would not
            // pin it -- QAbstractScrollArea installs QSizePolicy::Expanding vertically
            // (qabstractscrollarea.cpp:275), which lets a layout stretch the widget past its
            // sizeHint whenever there is leftover space, so the policy has to change too.
            UISE_TEST_CHECK_EQUAL(textEdit->sizeHint().height(),220);
            UISE_TEST_CHECK_EQUAL(textEdit->effectiveMaxHeight(),220);
            UISE_TEST_CHECK(textEdit->sizePolicy().verticalPolicy()==QSizePolicy::Fixed);

            editor.setExpanded(false);

            // Collapsing restores content-driven auto-resize and the policy QAbstractScrollArea
            // itself installed.
            UISE_TEST_CHECK(textEdit->sizePolicy().verticalPolicy()==QSizePolicy::Expanding);
            UISE_TEST_CHECK(textEdit->sizeHint().height()!=220);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestLeadingTrailingWidgetsSitInsideTheEditor)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            auto* attach=new QPushButton();
            auto* send=new QPushButton();
            attach->setFixedSize(24,24);
            send->setFixedSize(24,24);
            editor.addLeadingWidget(attach);
            editor.addTrailingWidget(send);

            // Reparented into the editor -- that is the whole point: a host's composer buttons
            // have to live under the same toolbar as the text, not beside the editor.
            UISE_TEST_CHECK(attach->parentWidget()==&editor);
            UISE_TEST_CHECK(send->parentWidget()==&editor);

            editor.resize(400,200);
            Layout::activateUpward(&editor);

            // Inline: leading | expand | text | trailing, all on one row.
            auto editorRect=editor.textEdit()->geometry();
            UISE_TEST_CHECK_LE(attach->geometry().right(),editorRect.left());
            UISE_TEST_CHECK_LE(attach->geometry().right(),editor.expandButton()->geometry().left());
            UISE_TEST_CHECK_LE(editorRect.right(),send->geometry().left());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestArrangementStacksOnMultilineAndReturnsOnlyWhenEmpty)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            auto* attach=new QPushButton();
            attach->setFixedSize(24,24);
            editor.addLeadingWidget(attach);

            UISE_TEST_CHECK(!editor.isStackedArrangement());

            editor.loadText(QStringLiteral("one line"),TextFormat::Plain);
            UISE_TEST_CHECK(!editor.isStackedArrangement());

            editor.loadText(QStringLiteral("a\nb\nc"),TextFormat::Plain);
            UISE_TEST_CHECK(editor.isStackedArrangement());

            // The asymmetry is the point: dropping back to a single line must NOT un-stack, or a
            // composer edited around the boundary would flip its own layout under the user.
            editor.loadText(QStringLiteral("short"),TextFormat::Plain);
            UISE_TEST_CHECK(editor.isStackedArrangement());

            // Only emptying it returns to inline.
            editor.loadText(QString(),TextFormat::Plain);
            UISE_TEST_CHECK(!editor.isStackedArrangement());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestExpandedAlwaysStacksSideWidgets)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            auto* attach=new QPushButton();
            attach->setFixedSize(24,24);
            editor.addLeadingWidget(attach);

            UISE_TEST_CHECK(!editor.isStackedArrangement());

            // Expanded pins the text area at its maximum height whatever it holds, so even an
            // EMPTY expanded editor must move its side widgets underneath -- otherwise they are
            // stranded beside a several-hundred-pixel-tall box.
            editor.setExpanded(true);
            UISE_TEST_CHECK(editor.isEmpty());
            UISE_TEST_CHECK(editor.isStackedArrangement());

            // Collapsing re-runs the ordinary content rule: empty editor -> back beside.
            editor.setExpanded(false);
            UISE_TEST_CHECK(!editor.isStackedArrangement());

            // With real multi-line text still present, collapsing leaves them below -- the same
            // hysteresis as typing, not a special case for the toggle.
            editor.loadText(QStringLiteral("a\nb\nc"),TextFormat::Plain);
            UISE_TEST_CHECK(editor.isStackedArrangement());
            editor.setExpanded(true);
            UISE_TEST_CHECK(editor.isStackedArrangement());
            editor.setExpanded(false);
            UISE_TEST_CHECK(editor.isStackedArrangement());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestStackedArrangementTurnsSideGroupsIntoColumns)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            auto* attach=new QPushButton();
            auto* send=new QPushButton();
            attach->setFixedSize(24,24);
            send->setFixedSize(24,24);
            editor.addLeadingWidget(attach);
            editor.addTrailingWidget(send);

            auto* leadingLayout=qobject_cast<QBoxLayout*>(editor.leadingWidgetsFrame()->layout());
            auto* trailingLayout=qobject_cast<QBoxLayout*>(editor.trailingWidgetsFrame()->layout());
            UISE_TEST_REQUIRE(leadingLayout!=nullptr);
            UISE_TEST_REQUIRE(trailingLayout!=nullptr);

            UISE_TEST_CHECK(leadingLayout->direction()==QBoxLayout::LeftToRight);
            UISE_TEST_CHECK(trailingLayout->direction()==QBoxLayout::LeftToRight);

            editor.loadText(QStringLiteral("a\nb\nc\nd"),TextFormat::Plain);
            editor.resize(400,300);
            Layout::activateUpward(&editor);
            UISE_TEST_REQUIRE(editor.isStackedArrangement());

            // ONLY the two side groups change axis. BottomToTop, not TopToBottom: "the left
            // widget becomes the bottom widget".
            UISE_TEST_CHECK(leadingLayout->direction()==QBoxLayout::BottomToTop);
            UISE_TEST_CHECK(trailingLayout->direction()==QBoxLayout::BottomToTop);

            // The row itself never changes -- the frames stay either side of the text area, they
            // do not move underneath it.
            auto editorRect=editor.textEdit()->geometry();
            UISE_TEST_CHECK_LE(editor.leadingWidgetsFrame()->geometry().right(),editorRect.left());
            UISE_TEST_CHECK_GE(editor.trailingWidgetsFrame()->geometry().left(),editorRect.right());

            // Within the leading column the first-added widget is lowest, and the expand button
            // -- the group's last member -- sits above it.
            UISE_TEST_CHECK_GE(attach->geometry().top(),editor.expandButton()->geometry().top());
            UISE_TEST_CHECK_EQUAL(attach->geometry().left(),editor.expandButton()->geometry().left());

            // Back to rows once emptied.
            editor.loadText(QString(),TextFormat::Plain);
            UISE_TEST_CHECK(leadingLayout->direction()==QBoxLayout::LeftToRight);
            UISE_TEST_CHECK(trailingLayout->direction()==QBoxLayout::LeftToRight);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestExpandButtonSitsLeftOfEditor)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setExpandButtonVisible(true);
            editor.resize(400,200);
            Layout::activateUpward(&editor);

            // The button shares a row with the text edit rather than occupying one of its own:
            // it must end up to the LEFT of the editor, and vertically overlap it rather than
            // sitting entirely below it.
            auto buttonRect=editor.expandButton()->geometry();
            auto editorRect=editor.textEdit()->geometry();
            UISE_TEST_CHECK_LE(buttonRect.right(),editorRect.left());
            UISE_TEST_CHECK(buttonRect.top()<editorRect.bottom());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestAutoResizeMinimumTracksContentHeight)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            EnhancedTextEdit textEdit;
            textEdit.setAutoResizingEnabled(true);
            textEdit.setPlainText(QStringLiteral("one line"));

            const auto qtFloor=textEdit.QTextEdit::minimumSizeHint().height();

            // Qt's own floor (~90px, QAbstractScrollArea reserving room for a couple of lines
            // plus scrollbars) is far taller than one line of text, and a layout can never size a
            // widget below its minimumSizeHint -- so without this override an auto-resizing
            // editor stays frozen for its first ~5 lines. Measured before the fix: document
            // heights 23/38/53/68/83px ALL rendered at 90px.
            UISE_TEST_CHECK_EQUAL(textEdit.minimumSizeHint().height(),textEdit.sizeHint().height());
            UISE_TEST_CHECK(textEdit.minimumSizeHint().height()<qtFloor);

            // Only the HEIGHT is overridden: sizeHint() reports the widget's CURRENT width, so
            // tracking it for the width too would pin the minimum width to whatever the widget
            // happens to be and ratchet it wider forever.
            UISE_TEST_CHECK_EQUAL(textEdit.minimumSizeHint().width(),
                                  textEdit.QTextEdit::minimumSizeHint().width());

            // Growing the document must move the minimum with it.
            auto before=textEdit.minimumSizeHint().height();
            textEdit.setPlainText(QStringLiteral("a\nb\nc\nd\ne\nf\ng\nh"));
            UISE_TEST_CHECK(textEdit.minimumSizeHint().height()>before);

            // Expanded mode pins the height at effectiveMaxHeight() instead and keeps Qt's floor,
            // so a cramped host can still shrink the editor.
            textEdit.setExpandedEnabled(true);
            UISE_TEST_CHECK_EQUAL(textEdit.minimumSizeHint().height(),qtFloor);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestAutoResizingDisabledRestoresScrollbar)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            EnhancedTextEdit textEdit;
            textEdit.setAutoResizingEnabled(false);
            UISE_TEST_CHECK(textEdit.verticalScrollBarPolicy()==Qt::ScrollBarAsNeeded);
            textEdit.setAutoResizingEnabled(true);
            UISE_TEST_CHECK(textEdit.verticalScrollBarPolicy()==Qt::ScrollBarAlwaysOff);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestMaxHeightHonoursQssMaximumHeight)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            EnhancedTextEdit textEdit;
            UISE_TEST_CHECK_EQUAL(textEdit.effectiveMaxHeight(),EnhancedTextEdit::DefaultMaxHeight);

            textEdit.setMaximumHeight(200);
            UISE_TEST_CHECK_EQUAL(textEdit.effectiveMaxHeight(),200);

            textEdit.setMaximumHeight(QWIDGETSIZE_MAX);
            UISE_TEST_CHECK_EQUAL(textEdit.effectiveMaxHeight(),EnhancedTextEdit::DefaultMaxHeight);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestAutoResizeStopsAtMaxHeightAndScrolls)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            EnhancedTextEdit textEdit;
            textEdit.setAutoResizingEnabled(true);
            textEdit.setMaxHeight(120);

            // Below the ceiling the widget is sized exactly to its content and suppresses the
            // scrollbar, since there would be nothing to scroll.
            textEdit.setPlainText(QStringLiteral("one"));
            UISE_TEST_CHECK_LE(textEdit.sizeHint().height(),120);
            UISE_TEST_CHECK(textEdit.verticalScrollBarPolicy()==Qt::ScrollBarAlwaysOff);

            // Past it the height stops growing -- an auto-resizing composer that grew without
            // limit would eventually swallow its window -- and the scrollbar comes back, or
            // everything beyond the cap would be unreachable.
            QString many=QStringLiteral("x");
            for (int i=0;i<40;++i)
            {
                many+=QStringLiteral("\nx");
            }
            textEdit.setPlainText(many);
            UISE_TEST_CHECK_EQUAL(textEdit.sizeHint().height(),120);
            UISE_TEST_CHECK(textEdit.verticalScrollBarPolicy()==Qt::ScrollBarAsNeeded);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestMaxHeightPercentFollowsReferenceWidget)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            QFrame reference;
            EnhancedTextEdit textEdit;
            textEdit.setMaxHeight(180);
            textEdit.setMaxHeightPercent(40);
            textEdit.setMaxHeightReferenceWidget(&reference);

            // A tall reference raises the ceiling...
            reference.resize(600,800);
            UISE_TEST_CHECK_EQUAL(textEdit.effectiveMaxHeight(),320);

            // ...but the floor is what a short one gets: 40% of 400 is 160, below maxHeight, so
            // maxHeight wins. The percentage may only ever RAISE the ceiling, never squeeze the
            // editor down to a couple of lines on a small window.
            reference.resize(600,400);
            UISE_TEST_CHECK_EQUAL(textEdit.effectiveMaxHeight(),180);

            // A host stylesheet's own max-height still outranks the percentage entirely.
            textEdit.setMaximumHeight(120);
            UISE_TEST_CHECK_EQUAL(textEdit.effectiveMaxHeight(),120);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestToolbarButtonVisibilityDefaults)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditorToolbar toolbar;
            UISE_TEST_CHECK(!toolbar.isButtonVisible(MessageEditorToolbarButton::Link));
            UISE_TEST_CHECK(!toolbar.isButtonVisible(MessageEditorToolbarButton::RemoveLink));
            UISE_TEST_CHECK(!toolbar.isButtonVisible(MessageEditorToolbarButton::Mention));
            UISE_TEST_CHECK(toolbar.button(MessageEditorToolbarButton::Close)!=nullptr);

            toolbar.setButtonVisible(MessageEditorToolbarButton::Link,true);
            UISE_TEST_CHECK(toolbar.isButtonVisible(MessageEditorToolbarButton::Link));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestDropdownTriggersWearSelectedOptionIcon)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            MessageEditorToolbar toolbar;

            // Asserted on the icon's NAME, not on shared_ptr identity: every icon that fails to
            // resolve comes back as the one shared m_fallbackIcon (svgiconlocator.cpp), so a
            // pointer comparison would quietly pass or fail on whether the style JSON happened to
            // load rather than on the wiring under test. The name checks the whole chain --
            // button -> alias -> messageeditor.json -> resolved icon.
            auto iconName=[](IconTextButton* btn)
            {
                auto icon=btn->svgIcon();
                return icon ? icon->name() : QString{};
            };

            auto* modeButton=toolbar.button(MessageEditorToolbarButton::Mode);
            // The trigger wears the SELECTED option's own glyph, not a static one.
            UISE_TEST_CHECK(iconName(modeButton).endsWith(QStringLiteral("wysiwyg")));
            toolbar.setMode(MessageEditingMode::Markdown);
            UISE_TEST_CHECK(iconName(modeButton).endsWith(QStringLiteral("markdown")));
            toolbar.setMode(MessageEditingMode::Plaintext);
            UISE_TEST_CHECK(iconName(modeButton).endsWith(QStringLiteral("plaintext")));

            auto* headingButton=toolbar.button(MessageEditorToolbarButton::Heading);
            UISE_TEST_CHECK(iconName(headingButton).endsWith(QStringLiteral("normalText")));
            MessageEditorFormatState state;
            state.headingLevel=2;
            toolbar.setFormatState(state);
            UISE_TEST_CHECK(iconName(headingButton).endsWith(QStringLiteral("heading2")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestSelectableDropdownsCloseOnActivation)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditorToolbar toolbar;
            // Mode and heading are one-shot choices, so their menus close on activation like a
            // plain clickable row. The table menu's rows are already non-checkable, so
            // DropdownMenu closes it for free.
            UISE_TEST_CHECK(toolbar.modeMenu()->isCloseOnCheckableActivation());
            UISE_TEST_CHECK(toolbar.headingMenu()->isCloseOnCheckableActivation());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestToolbarStateSyncEmitsNothing)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditorToolbar toolbar;
            QSignalSpy spy(&toolbar,&MessageEditorToolbar::boldRequested);

            MessageEditorFormatState state;
            state.bold=true;
            toolbar.setFormatState(state);

            UISE_TEST_CHECK_EQUAL(spy.count(),0);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestToolbarClickedUsesStateNotButton)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditorToolbar toolbar;
            QSignalSpy spy(&toolbar,&MessageEditorToolbar::boldRequested);

            MessageEditorFormatState state;
            state.bold=true;
            toolbar.setFormatState(state);

            // The button's own checked state, not what was last requested, drives the emitted
            // value -- and IconTextButton::click()'s unconditional toggle() must not be allowed
            // to become the source of truth.
            toolbar.button(MessageEditorToolbarButton::Bold)->click();

            UISE_TEST_REQUIRE_EQUAL(spy.count(),1);
            UISE_TEST_CHECK(!spy.at(0).at(0).toBool());
            UISE_TEST_CHECK(toolbar.button(MessageEditorToolbarButton::Bold)->isChecked());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestUndoRedoButtons)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            auto* toolbar=editor.toolbar();

            // An untouched editor has an empty undo stack, so both start unavailable.
            UISE_TEST_CHECK(!toolbar->isButtonEnabled(MessageEditorToolbarButton::Undo));
            UISE_TEST_CHECK(!toolbar->isButtonEnabled(MessageEditorToolbarButton::Redo));

            // Typed through the cursor rather than loadText(): setPlainText() and friends reset
            // the document's undo stack, which is exactly what must NOT happen here.
            auto cursor=editor.textEdit()->textCursor();
            cursor.insertText(QStringLiteral("hello"));
            UISE_TEST_CHECK(toolbar->isButtonEnabled(MessageEditorToolbarButton::Undo));

            toolbar->button(MessageEditorToolbarButton::Undo)->click();
            UISE_TEST_CHECK(!editor.text(TextFormat::Plain).contains(QStringLiteral("hello")));
            UISE_TEST_CHECK(toolbar->isButtonEnabled(MessageEditorToolbarButton::Redo));

            toolbar->button(MessageEditorToolbarButton::Redo)->click();
            UISE_TEST_CHECK(editor.text(TextFormat::Plain).contains(QStringLiteral("hello")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestUndoRedoStayEnabledOutsideWysiwyg)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setExpanded(true);

            auto cursor=editor.textEdit()->textCursor();
            cursor.insertText(QStringLiteral("hello"));

            editor.setMessageEditingMode(MessageEditingMode::Plaintext);

            // Undo is an EDIT, not formatting -- setFormattingEnabled(false) greys the formatting
            // half in Markdown/Plaintext mode, but must leave undo/redo alone.
            UISE_TEST_CHECK(!editor.toolbar()->isButtonEnabled(MessageEditorToolbarButton::Bold));
            UISE_TEST_CHECK(editor.toolbar()->isButtonEnabled(MessageEditorToolbarButton::Undo));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestFormattingDisabledOutsideWysiwyg)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setExpandButtonVisible(true);
            editor.setExpanded(true);

            editor.setMessageEditingMode(MessageEditingMode::Markdown);
            UISE_TEST_CHECK(!editor.toolbar()->isButtonEnabled(MessageEditorToolbarButton::Bold));
            UISE_TEST_CHECK(editor.toolbar()->isButtonEnabled(MessageEditorToolbarButton::Mode));

            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            UISE_TEST_CHECK(editor.toolbar()->isButtonEnabled(MessageEditorToolbarButton::Bold));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestContextMenuFormattingRowsOnlyInWysiwyg)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            MessageEditor editor;
            editor.loadText(QStringLiteral("word"),TextFormat::Plain);

            std::vector<MenuItem> captured;
            editor.setContextMenuHandler(
                [&](std::vector<MenuItem>& items)
                {
                    captured=items;
                }
            );

            QMetaObject::invokeMethod(&editor,"showContextMenu",Q_ARG(QPoint,QPoint(0,0)));

            auto hasFormatting=[](const std::vector<MenuItem>& items)
            {
                for (const auto& item : items)
                {
                    if (item.id==static_cast<int>(MessageEditorMenuAction::Formatting) && !item.children.empty())
                    {
                        return true;
                    }
                }
                return false;
            };
            UISE_TEST_CHECK(hasFormatting(captured));

            captured.clear();
            editor.setMessageEditingMode(MessageEditingMode::Plaintext);
            QMetaObject::invokeMethod(&editor,"showContextMenu",Q_ARG(QPoint,QPoint(0,0)));
            UISE_TEST_CHECK(!hasFormatting(captured));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestCloseButtonLeadsOnEveryPlatform)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditorToolbar toolbar;
            auto* layout=toolbar.layout();
            UISE_TEST_REQUIRE(layout!=nullptr);

            // Close is the FIRST item on every platform -- deliberately not the brief's original
            // left-on-macOS/right-elsewhere split, so it always sits above the editor's own
            // bottom-left expand button. Asserted unconditionally: a #ifdef here would let the
            // non-macOS layout regress untested on the machine this suite usually runs on.
            UISE_TEST_CHECK_EQUAL(layout->indexOf(toolbar.button(MessageEditorToolbarButton::Close)),0);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestMessageEditorShowsLinkButtonByDefault)
{
    // Deliberately NOT a change to TestToolbarButtonVisibilityDefaults above: that test builds a
    // BARE MessageEditorToolbar, whose own ctor-default for Link is still hidden (Stage 6's
    // Mention keeps that shape). Stage 5b makes Link visible from MessageEditor's OWN ctor
    // instead -- a shipped feature, not a hidden placeholder -- so the assertion belongs on a
    // real MessageEditor, not on the toolbar in isolation.
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            UISE_TEST_CHECK(editor.toolbar()->isButtonVisible(MessageEditorToolbarButton::Link));
            // RemoveLink stays hidden until the caret is inside a link -- see
            // TestRemoveLinkButtonVisibilityTracksCaret.
            UISE_TEST_CHECK(!editor.toolbar()->isButtonVisible(MessageEditorToolbarButton::RemoveLink));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestLinkEnabledInMarkdownDisabledInPlaintext)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setExpandButtonVisible(true);
            editor.setExpanded(true);

            // Unlike every other row in FormattingButtons, Link stays enabled in Markdown mode
            // too -- inserting a literal "[title](url)" is exactly the kind of thing that mode
            // should still help with.
            editor.setMessageEditingMode(MessageEditingMode::Markdown);
            UISE_TEST_CHECK(editor.toolbar()->isButtonEnabled(MessageEditorToolbarButton::Link));
            UISE_TEST_CHECK(!editor.toolbar()->isButtonEnabled(MessageEditorToolbarButton::Bold));

            editor.setMessageEditingMode(MessageEditingMode::Plaintext);
            UISE_TEST_CHECK(!editor.toolbar()->isButtonEnabled(MessageEditorToolbarButton::Link));

            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            UISE_TEST_CHECK(editor.toolbar()->isButtonEnabled(MessageEditorToolbarButton::Link));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestRemoveLinkButtonVisibilityTracksCaret)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setExpandButtonVisible(true);
            // syncToolbarState() early-returns while the toolbar is hidden (see its own doc
            // comment), so RemoveLink's visibility is only ever pushed while expanded.
            editor.setExpanded(true);

            editor.insertLink(QStringLiteral("https://example.com"),QStringLiteral("Example"));

            // insertLink() deliberately leaves the caret OUTSIDE the link it just inserted (so
            // typing continues in plain text), so RemoveLink is hidden at that moment -- moving
            // the caret back INTO the link is what reveals it.
            UISE_TEST_CHECK(!editor.toolbar()->isButtonVisible(MessageEditorToolbarButton::RemoveLink));

            auto inside=editor.textEdit()->textCursor();
            inside.setPosition(3);
            editor.textEdit()->setTextCursor(inside);
            UISE_TEST_CHECK(editor.toolbar()->isButtonVisible(MessageEditorToolbarButton::RemoveLink));

            auto outside=editor.textEdit()->textCursor();
            outside.movePosition(QTextCursor::End);
            outside.insertText(QStringLiteral(" plain"));
            editor.textEdit()->setTextCursor(outside);
            UISE_TEST_CHECK(!editor.toolbar()->isButtonVisible(MessageEditorToolbarButton::RemoveLink));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestInsertLinkWysiwygRoundTrip)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.insertLink(QStringLiteral("https://example.com"),QStringLiteral("Example"));

            // Measured: a real QTextCharFormat anchor (not literal text) exports via
            // toMarkdown() as "[title](url)" bit-identical, with no baked colour of its own --
            // that is the viewer's job, never this editor's.
            UISE_TEST_CHECK_EQUAL_QSTR(
                editor.text(TextFormat::Markdown).trimmed(),
                QStringLiteral("[Example](https://example.com)")
            );

            // The anchor is a real char format on the document, and carries NO baked colour of
            // its own (Qt's own markdown importer bakes #0000ff; this editor must not).
            bool foundAnchor=false;
            for (auto block=editor.textEdit()->document()->begin(); block.isValid(); block=block.next())
            {
                for (auto it=block.begin(); !it.atEnd(); ++it)
                {
                    const auto format=it.fragment().charFormat();
                    if (format.isAnchor()
                        && format.anchorHref()==QStringLiteral("https://example.com"))
                    {
                        foundAnchor=true;
                        UISE_TEST_CHECK(!format.hasProperty(QTextFormat::ForegroundBrush));
                    }
                }
            }
            UISE_TEST_CHECK(foundAnchor);

            // The caret is deliberately left OUTSIDE the link, so the next thing typed is not
            // swallowed into it -- measured, without this "Example" + " plain" exported as
            // "[Example plain](url)".
            UISE_TEST_CHECK(!editor.textEdit()->currentCharFormat().isAnchor());

            auto typing=editor.textEdit()->textCursor();
            typing.insertText(QStringLiteral(" plain"));
            editor.textEdit()->setTextCursor(typing);
            UISE_TEST_CHECK_EQUAL_QSTR(
                editor.text(TextFormat::Markdown).trimmed(),
                QStringLiteral("[Example](https://example.com) plain")
            );
        }
    );
}

BOOST_AUTO_TEST_CASE(TestInsertLinkEmptyTitleUsesUrl)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.insertLink(QStringLiteral("https://example.com"),QString{});

            UISE_TEST_CHECK_EQUAL_QSTR(
                editor.text(TextFormat::Markdown).trimmed(),
                QStringLiteral("[https://example.com](https://example.com)")
            );
        }
    );
}

BOOST_AUTO_TEST_CASE(TestInsertLinkMarkdownModeIsLiteralText)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Markdown);

            editor.insertLink(QStringLiteral("https://example.com"),QStringLiteral("Example"));

            // Markdown mode's document IS source text -- a literal insert, not an anchor char
            // format, and text(Markdown) must return it byte-for-byte with no escaping.
            UISE_TEST_CHECK_EQUAL_QSTR(
                editor.text(TextFormat::Markdown),
                QStringLiteral("[Example](https://example.com)")
            );
        }
    );
}

BOOST_AUTO_TEST_CASE(TestRemoveLinkClearsWholeRun)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            auto* textEdit=editor.textEdit();

            // Two same-href fragments with different weight -- the shape an anchor applied over
            // an already-formatted selection splits into (measured), and the case
            // selectLinkRunAtCursor()/removeLink() must walk across as ONE link.
            auto cursor=textEdit->textCursor();
            QTextCharFormat boldAnchor;
            boldAnchor.setAnchor(true);
            boldAnchor.setAnchorHref(QStringLiteral("https://example.com"));
            boldAnchor.setFontWeight(QFont::Bold);
            cursor.insertText(QStringLiteral("bo"),boldAnchor);

            QTextCharFormat plainAnchor;
            plainAnchor.setAnchor(true);
            plainAnchor.setAnchorHref(QStringLiteral("https://example.com"));
            cursor.insertText(QStringLiteral("ld"),plainAnchor);
            textEdit->setTextCursor(cursor);

            // Caret in the middle of the run, no selection -- removeLink() must extend to the
            // whole run itself.
            auto caret=textEdit->textCursor();
            caret.setPosition(2);
            textEdit->setTextCursor(caret);

            emit editor.toolbar()->removeLinkRequested();

            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Plain),QStringLiteral("bold"));

            bool anyAnchor=false;
            bool firstFragmentStillBold=false;
            for (auto block=textEdit->document()->begin(); block.isValid(); block=block.next())
            {
                for (auto it=block.begin(); !it.atEnd(); ++it)
                {
                    const auto fragment=it.fragment();
                    const auto format=fragment.charFormat();
                    if (format.isAnchor())
                    {
                        anyAnchor=true;
                    }
                    if (fragment.text()==QStringLiteral("bo") && format.fontWeight()>=QFont::Bold)
                    {
                        firstFragmentStillBold=true;
                    }
                }
            }

            UISE_TEST_CHECK(!anyAnchor);
            UISE_TEST_CHECK(firstFragmentStillBold);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestLinkDisabledInsideCodeFence)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);

            emit editor.toolbar()->codeBlockRequested();
            // The placeholder is left selected -- type over it to land the caret on real fenced
            // content, the same as a user would.
            auto cursor=editor.textEdit()->textCursor();
            cursor.insertText(QStringLiteral("code"));
            editor.textEdit()->setTextCursor(cursor);

            QSignalSpy spy(&editor,&AbstractMessageEditor::linkRequested);
            emit editor.toolbar()->linkRequested();
            UISE_TEST_CHECK_EQUAL(spy.count(),0);

            editor.insertLink(QStringLiteral("https://example.com"),QStringLiteral("Example"));
            UISE_TEST_CHECK(!editor.text(TextFormat::Markdown).contains(QStringLiteral("[Example]")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestContextMenuRemoveLinkOnlyWhenInsideLink)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            MessageEditor editor;
            editor.insertLink(QStringLiteral("https://example.com"),QStringLiteral("Example"));

            auto findFormatting=[](const std::vector<MenuItem>& items) -> const std::vector<MenuItem>*
            {
                for (const auto& item : items)
                {
                    if (item.id==static_cast<int>(MessageEditorMenuAction::Formatting))
                    {
                        return &item.children;
                    }
                }
                return nullptr;
            };
            auto hasRow=[](const std::vector<MenuItem>& items, MessageEditorMenuAction action)
            {
                for (const auto& item : items)
                {
                    if (item.id==static_cast<int>(action))
                    {
                        return true;
                    }
                }
                return false;
            };

            std::vector<MenuItem> captured;
            editor.setContextMenuHandler([&](std::vector<MenuItem>& items) { captured=items; });

            // Caret moved INTO the link -- insertLink() itself leaves it just outside, so the
            // next thing typed is not swallowed into the link.
            auto inside=editor.textEdit()->textCursor();
            inside.setPosition(3);
            editor.textEdit()->setTextCursor(inside);

            QMetaObject::invokeMethod(&editor,"showContextMenu",Q_ARG(QPoint,QPoint(0,0)));
            auto* formatting=findFormatting(captured);
            UISE_TEST_REQUIRE(formatting!=nullptr);
            UISE_TEST_CHECK(hasRow(*formatting,MessageEditorMenuAction::Link));
            UISE_TEST_CHECK(hasRow(*formatting,MessageEditorMenuAction::RemoveLink));

            captured.clear();
            auto cursor=editor.textEdit()->textCursor();
            cursor.movePosition(QTextCursor::End);
            cursor.insertText(QStringLiteral(" plain"));
            editor.textEdit()->setTextCursor(cursor);

            QMetaObject::invokeMethod(&editor,"showContextMenu",Q_ARG(QPoint,QPoint(0,0)));
            formatting=findFormatting(captured);
            UISE_TEST_REQUIRE(formatting!=nullptr);
            UISE_TEST_CHECK(hasRow(*formatting,MessageEditorMenuAction::Link));
            UISE_TEST_CHECK(!hasRow(*formatting,MessageEditorMenuAction::RemoveLink));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestPasteStripsBakedColorsAndFonts)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;

            auto* mime=new QMimeData();
            mime->setHtml(QStringLiteral(
                "<html><body><p style=\"color:#1f2937;font-family:Calibri;font-size:14pt\">"
                "Hello <b>world</b></p></body></html>"
            ));
            mime->setText(QStringLiteral("Hello world"));
            QApplication::clipboard()->setMimeData(mime);

            editor.textEdit()->pasteFromClipboard();

            bool foundBakedFormat=false;
            bool foundBold=false;
            for (auto block=editor.textEdit()->document()->begin(); block.isValid(); block=block.next())
            {
                for (auto it=block.begin(); !it.atEnd(); ++it)
                {
                    const auto format=it.fragment().charFormat();
                    if (format.hasProperty(QTextFormat::ForegroundBrush)
                        || format.hasProperty(QTextFormat::FontFamilies)
                        || format.hasProperty(QTextFormat::FontPointSize))
                    {
                        foundBakedFormat=true;
                    }
                    if (format.fontWeight()>=QFont::Bold)
                    {
                        foundBold=true;
                    }
                }
            }

            // Non-lossy: the strip removes cosmetic paint only -- bold survives.
            UISE_TEST_CHECK(!foundBakedFormat);
            UISE_TEST_CHECK(foundBold);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestPasteFixesInvisibleTable)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;

            auto* mime=new QMimeData();
            mime->setHtml(QStringLiteral(
                "<html><body><table border=\"0\"><tr><td>A</td><td>B</td></tr></table></body></html>"
            ));
            mime->setText(QStringLiteral("A\tB"));
            QApplication::clipboard()->setMimeData(mime);

            editor.textEdit()->pasteFromClipboard();

            QTextTable* table=nullptr;
            auto* root=editor.textEdit()->document()->rootFrame();
            for (auto it=root->begin(); !it.atEnd(); ++it)
            {
                if (auto* candidate=qobject_cast<QTextTable*>(it.currentFrame()))
                {
                    table=candidate;
                    break;
                }
            }

            UISE_TEST_REQUIRE(table!=nullptr);
            UISE_TEST_CHECK_EQUAL(table->format().border(),1.0);
            UISE_TEST_CHECK(!table->format().borderCollapse());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestPasteNormalizesATableInAnEmptyDocumentToo)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            // Qt rebuilds the frame hierarchy only in QTextDocumentPrivate::finishEdit(), which
            // does nothing while an edit block is open -- so while insertFromMimeData() held one,
            // a table created by the paste was not yet among rootFrame()'s children and every
            // table fix walked straight past it. An ALREADY PRESENT table left the hierarchy
            // populated and hid the bug completely, which is how it was reported: insert a table
            // with the toolbar first and a pasted table is cleaned; paste into a fresh editor and
            // its cells keep their original fills.
            //
            // Measured with the block still open -- empty document: 0 tables found; document with
            // only text: 0; document with a table already in it: 2. So the state of the document
            // BEFORE the paste is the axis this has to be checked on, and an empty one is the case
            // that fails.
            auto pasteTableInto=[](MessageEditor& editor)
            {
                auto* mime=new QMimeData();
                mime->setHtml(QStringLiteral(
                    "<html><body><table border=\"0\">"
                    "<tr><td style=\"background-color:#b0b3b2;color:#1f2937\">A</td>"
                        "<td style=\"background-color:#d4d4d4\">B</td></tr>"
                    "<tr><td style=\"background-color:#f2f2f2\">C</td>"
                        "<td style=\"background-color:#ffffff\">D</td></tr>"
                    "</table></body></html>"
                ));
                mime->setText(QStringLiteral("A\tB\nC\tD"));
                QApplication::clipboard()->setMimeData(mime);

                editor.textEdit()->pasteFromClipboard();

                QTextTable* pasted=nullptr;
                auto* root=editor.textEdit()->document()->rootFrame();
                for (auto it=root->begin(); !it.atEnd(); ++it)
                {
                    if (auto* candidate=qobject_cast<QTextTable*>(it.currentFrame()))
                    {
                        pasted=candidate;
                    }
                }
                return pasted;
            };

            auto checkNormalized=[](QTextTable* pasted)
            {
                UISE_TEST_REQUIRE(pasted!=nullptr);
                UISE_TEST_CHECK_EQUAL(pasted->format().border(),1.0);
                UISE_TEST_CHECK(!pasted->format().borderCollapse());

                // The fills are the visible half: kept, they pair a light cell with palette-
                // coloured text, i.e. white on near-white in a dark theme.
                for (int row=0; row<pasted->rows(); ++row)
                {
                    for (int column=0; column<pasted->columns(); ++column)
                    {
                        auto cellFormat=pasted->cellAt(row,column).format();
                        UISE_TEST_CHECK(!cellFormat.hasProperty(QTextFormat::BackgroundBrush));
                    }
                }
            };

            // The case that regressed: nothing in the document at all.
            MessageEditor empty;
            checkNormalized(pasteTableInto(empty));

            // Text but no table -- also failed, so "a table exists" was never the real condition.
            MessageEditor withText;
            withText.loadText(QStringLiteral("hello"),TextFormat::Plain);
            auto textEnd=withText.textEdit()->textCursor();
            textEnd.movePosition(QTextCursor::End);
            withText.textEdit()->setTextCursor(textEnd);
            checkNormalized(pasteTableInto(withText));

            // The case that always worked, kept so a fix cannot trade one for the other.
            MessageEditor withTable;
            emit withTable.toolbar()->tableRequested(2,2);
            auto tableEnd=withTable.textEdit()->textCursor();
            tableEnd.movePosition(QTextCursor::End);
            withTable.textEdit()->setTextCursor(tableEnd);
            checkNormalized(pasteTableInto(withTable));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestPasteIsASingleUndoStep)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            // The normalization runs AFTER the insert's edit block closes (it has to -- see
            // TestPasteNormalizesATableInAnEmptyDocumentToo), so it must rejoin that block's undo
            // action rather than open one of its own. Otherwise the first Ctrl+Z after a paste
            // puts the baked colours back instead of taking the paste away.
            MessageEditor editor;
            editor.textEdit()->textCursor().insertText(QStringLiteral("typed"));
            const auto before=editor.text(TextFormat::Plain);

            auto* mime=new QMimeData();
            mime->setHtml(QStringLiteral(
                "<html><body><table border=\"0\">"
                "<tr><td style=\"background-color:#b0b3b2\">A</td></tr>"
                "</table></body></html>"
            ));
            mime->setText(QStringLiteral("A"));
            QApplication::clipboard()->setMimeData(mime);

            auto atEnd=editor.textEdit()->textCursor();
            atEnd.movePosition(QTextCursor::End);
            editor.textEdit()->setTextCursor(atEnd);
            editor.textEdit()->pasteFromClipboard();

            UISE_TEST_CHECK(editor.textEdit()->document()->availableUndoSteps()>0);

            editor.textEdit()->undo();
            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Plain),before);

            // And what redo brings back is the NORMALIZED table, not the raw pasted one.
            editor.textEdit()->redo();

            QTextTable* table=nullptr;
            auto* root=editor.textEdit()->document()->rootFrame();
            for (auto it=root->begin(); !it.atEnd(); ++it)
            {
                if (auto* candidate=qobject_cast<QTextTable*>(it.currentFrame()))
                {
                    table=candidate;
                }
            }
            UISE_TEST_REQUIRE(table!=nullptr);
            UISE_TEST_CHECK(
                !table->cellAt(0,0).format().hasProperty(QTextFormat::BackgroundBrush));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestPasteConvertsPreToLiteralFence)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;

            auto* mime=new QMimeData();
            mime->setHtml(QStringLiteral(
                "<html><body><p>before</p><pre><code>int x = 1;</code></pre><p>after</p></body></html>"
            ));
            mime->setText(QStringLiteral("before\nint x = 1;\nafter"));
            QApplication::clipboard()->setMimeData(mime);

            editor.textEdit()->pasteFromClipboard();

            bool foundLiteralFence=false;
            bool foundPropertyBasedBlock=false;
            for (auto block=editor.textEdit()->document()->begin(); block.isValid(); block=block.next())
            {
                if (isFenceLine(block.text()))
                {
                    foundLiteralFence=true;
                }

                const auto bf=block.blockFormat();
                if (bf.hasProperty(QTextFormat::BlockCodeFence)
                    || !bf.stringProperty(QTextFormat::BlockCodeLanguage).isEmpty()
                    || bf.nonBreakableLines())
                {
                    foundPropertyBasedBlock=true;
                }
            }

            UISE_TEST_CHECK(foundLiteralFence);
            UISE_TEST_CHECK(!foundPropertyBasedBlock);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestPasteKeepsUndoStack)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;

            auto typed=editor.textEdit()->textCursor();
            typed.insertText(QStringLiteral("typed before paste"));
            editor.textEdit()->setTextCursor(typed);
            UISE_TEST_REQUIRE(editor.textEdit()->document()->isUndoAvailable());

            auto* mime=new QMimeData();
            mime->setHtml(QStringLiteral("<html><body><p>pasted</p></body></html>"));
            mime->setText(QStringLiteral("pasted"));
            QApplication::clipboard()->setMimeData(mime);

            editor.textEdit()->pasteFromClipboard();
            UISE_TEST_CHECK(editor.text(TextFormat::Plain).contains(QStringLiteral("pasted")));

            // The paste must NOT have wiped the history. QTextDocument::setUndoRedoEnabled(false)
            // CLEARS the undo stack outright (measured: one step before, zero after), so running
            // the normalization passes with undo suppressed -- which is right for a whole-document
            // load -- made Ctrl+Z after ANY paste a no-op, throwing away everything typed before.
            UISE_TEST_REQUIRE(editor.textEdit()->document()->isUndoAvailable());

            // And one undo takes the whole paste back out, normalization included, because it all
            // shares insertFromMimeData()'s own edit block.
            editor.textEdit()->undo();
            UISE_TEST_CHECK(!editor.text(TextFormat::Plain).contains(QStringLiteral("pasted")));
            UISE_TEST_CHECK(editor.text(TextFormat::Plain).contains(QStringLiteral("typed before paste")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestHyperlinkDialogFitsItsHeightCeiling)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            Style::instance().applyStyleSheet();

            ModalHyperlinkDialog frame;
            frame.resize(700,600);

            UISE_TEST_REQUIRE(frame.openDialog(true,false));
            // QPointer, never auto* -- see the accessor's own declaration.
            auto dialog=frame.dialog();
            UISE_TEST_REQUIRE(!dialog.isNull());

            // ModalPopup::updateWidgetGeometry() caps the popup at maxHeightPercent() of the HOST
            // FRAME's rect even with auto-height on, so a low percentage does not make the box
            // tidier -- it clips it. Measured at the original 40%: a 220px host gave an 88px
            // popup and the two fields were squeezed into nothing.
            const auto ceiling=frame.height()*frame.maxHeightPercent()/100;
            UISE_TEST_CHECK(dialog->sizeHint().height()<=ceiling);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestInsertedLinkIsPaintedWithoutTouchingDocument)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            auto* textEdit=editor.textEdit();
            textEdit->setLinkColor(QColor(0x1A,0x6F,0xD4));

            editor.insertLink(QStringLiteral("https://example.com"),QStringLiteral("Example"));

            auto* document=textEdit->document();

            // The colour lands as a LAYOUT format, which is the whole point: an anchor otherwise
            // renders exactly like ordinary text (measured), and the blue an imported link showed
            // was baked into the DOCUMENT by Qt's importer -- frozen at its theme and leaking into
            // toHtml(). This is what makes a freshly inserted link visible as a link.
            const auto formats=document->firstBlock().layout()->formats();
            bool paintedLinkColor=false;
            for (const auto& range : formats)
            {
                if (range.format.foreground().color()==QColor(0x1A,0x6F,0xD4))
                {
                    paintedLinkColor=true;
                }
            }
            UISE_TEST_CHECK(paintedLinkColor);

            // ...and nowhere else: the document's own char formats stay clean, so nothing is
            // frozen at the current theme and no colour reaches the sent message.
            for (auto it=document->firstBlock().begin(); !it.atEnd(); ++it)
            {
                UISE_TEST_CHECK(!it.fragment().charFormat()
                                    .hasProperty(QTextFormat::ForegroundBrush));
            }
            UISE_TEST_CHECK(!editor.text(TextFormat::Html).contains(QStringLiteral("1a6fd4"),
                                                                    Qt::CaseInsensitive));
            UISE_TEST_CHECK_EQUAL_QSTR(
                editor.text(TextFormat::Markdown).trimmed(),
                QStringLiteral("[Example](https://example.com)")
            );
        }
    );
}

BOOST_AUTO_TEST_CASE(TestImportedLinkColorIsStripped)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(QStringLiteral("see [Example](https://example.com) here"),
                            TextFormat::Markdown);

            // Qt's markdown importer bakes foreground=#0000ff onto every anchor it reads. Left
            // alone that is the only reason an imported link looked like one -- and it is frozen
            // at import time, leaks into toHtml(), and overrides whatever linkColor a host set.
            bool anchorSeen=false;
            for (auto block=editor.textEdit()->document()->begin(); block.isValid();
                 block=block.next())
            {
                for (auto it=block.begin(); !it.atEnd(); ++it)
                {
                    const auto format=it.fragment().charFormat();
                    if (!format.isAnchor())
                    {
                        continue;
                    }
                    anchorSeen=true;
                    UISE_TEST_CHECK(!format.hasProperty(QTextFormat::ForegroundBrush));
                    // The anchor itself must survive the strip untouched.
                    UISE_TEST_CHECK_EQUAL_QSTR(format.anchorHref(),
                                               QStringLiteral("https://example.com"));
                }
            }
            UISE_TEST_CHECK(anchorSeen);
            UISE_TEST_CHECK(!editor.text(TextFormat::Html).contains(QStringLiteral("0000ff"),
                                                                    Qt::CaseInsensitive));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestNumbersStyleTablePastesAsTable)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            QSignalSpy attachments(&editor,&AbstractMessageEditor::attachmentsPasted);

            // A faithful replica of what macOS Numbers/Pages actually put on the pasteboard,
            // dumped from a real copy: the true table as text/html, a tab-separated text/plain,
            // and an image rendition whose data is ZERO BYTES -- which Qt still reports through
            // hasImage(). Judging on the image alone (mimeDataHasAttachments()) sent the whole
            // payload to the attachment flow and inserted nothing, which is how a copied
            // spreadsheet table used to reach this editor.
            // Markup copied VERBATIM from a real macOS Numbers copy (dumped off the pasteboard),
            // not a reconstruction -- the fill lives on the <td>, the text colour on a <font>
            // that repeats it in its own style attribute, and every cell carries a black border.
            auto* mime=new QMimeData();
            mime->setHtml(QStringLiteral(
                "<table cellspacing=\"0\" cellpadding=\"0\" style=\"border-collapse: collapse\">\n"
                "<tbody>\n<tr>\n"
                "<td valign=\"top\" style=\"width: 108.0px; height: 14.0px; "
                "background-color: #b0b3b2; border-style: solid; "
                "border-width: 1.0px 1.0px 1.0px 1.0px; "
                "border-color: #000000 #000000 #000000 #000000; "
                "padding: 4.0px 4.0px 4.0px 4.0px\">\n"
                "<p style=\"margin: 0.0px 0.0px 0.0px 0.0px\">"
                "<font face=\"Helvetica Neue\" size=\"2\" color=\"#000000\" "
                "style=\"font: 10.0px 'Helvetica Neue'; font-variant-ligatures: common-ligatures; "
                "color: #000000\"><b>11</b><b></b></font></p>\n</td>\n"
                "<td valign=\"top\" style=\"width: 108.0px; height: 14.0px; "
                "background-color: #b0b3b2; border-style: solid; "
                "border-width: 1.0px 1.0px 1.0px 1.0px; "
                "border-color: #000000 #000000 #000000 #000000; "
                "padding: 4.0px 4.0px 4.0px 4.0px\">\n"
                "<p style=\"margin: 0.0px 0.0px 0.0px 0.0px\">"
                "<font face=\"Helvetica Neue\" size=\"2\" color=\"#000000\" "
                "style=\"font: 10.0px 'Helvetica Neue'; color: #000000\">"
                "<b>12</b></font></p>\n</td>\n</tr>\n<tr>\n"
                "<td valign=\"top\" style=\"width: 108.0px; height: 13.0px; "
                "background-color: #d4d4d4; border-style: solid; "
                "border-width: 1.0px 1.0px 1.0px 1.0px; "
                "border-color: #000000 #000000 #000000 #000000; "
                "padding: 4.0px 4.0px 4.0px 4.0px\">\n"
                "<p style=\"margin: 0.0px 0.0px 0.0px 0.0px\">"
                "<font face=\"Helvetica Neue\" size=\"2\" color=\"#000000\" "
                "style=\"font: 10.0px 'Helvetica Neue'; color: #000000\">A</font></p>\n</td>\n"
                "<td valign=\"top\" style=\"width: 108.0px; height: 13.0px; "
                "border-style: solid; border-width: 1.0px 1.0px 1.0px 1.0px; "
                "border-color: #000000 #000000 #000000 #000000; "
                "padding: 4.0px 4.0px 4.0px 4.0px\">\n"
                "<p style=\"margin: 0.0px 0.0px 0.0px 0.0px\">"
                "<font face=\"Helvetica Neue\" size=\"2\" color=\"#000000\">Aaaa</font></p>\n"
                "</td>\n</tr>\n</tbody>\n</table>"
            ));
            mime->setText(QStringLiteral("11\t12\nA\tAaaa"));
            mime->setData(QStringLiteral("application/x-qt-image"),QByteArray());
            QApplication::clipboard()->setMimeData(mime);

            editor.textEdit()->pasteFromClipboard();

            UISE_TEST_CHECK_EQUAL(attachments.count(),0);

            QTextTable* table=nullptr;
            auto* root=editor.textEdit()->document()->rootFrame();
            for (auto it=root->begin(); !it.atEnd(); ++it)
            {
                if (auto* candidate=qobject_cast<QTextTable*>(it.currentFrame()))
                {
                    table=candidate;
                    break;
                }
            }
            UISE_TEST_REQUIRE(table!=nullptr);
            UISE_TEST_CHECK_EQUAL(table->rows(),2);
            UISE_TEST_CHECK_EQUAL(table->columns(),2);
            // ...and it went through the same normalization every other pasted table does.
            UISE_TEST_CHECK_EQUAL(table->format().border(),1.0);
            UISE_TEST_CHECK(!table->format().borderCollapse());

            // Including the PER-CELL borders and FILLS Numbers bakes in. Measured on the real
            // payload: every cell carries an explicit #000000 border, and 14 of 20 carry a
            // light fill (#b0b3b2/#d4d4d4/#f2f2f2) on the cell format AND on the block format
            // inside it. Leaving the fills while stripping the char foregrounds was the worst
            // of both worlds -- the text fell back to the palette, giving WHITE text on a
            // near-white cell in a dark theme. Nothing baked may survive, in either carrier.
            for (int row=0; row<table->rows(); ++row)
            {
                for (int column=0; column<table->columns(); ++column)
                {
                    const auto cell=table->cellAt(row,column);
                    const auto cellFormat=cell.format().toTableCellFormat();
                    UISE_TEST_CHECK(!cellFormat.hasProperty(QTextFormat::TableCellLeftBorderBrush));
                    UISE_TEST_CHECK(!cellFormat.hasProperty(QTextFormat::TableCellTopBorderBrush));
                    UISE_TEST_CHECK(!cellFormat.hasProperty(QTextFormat::TableCellLeftBorder));
                    UISE_TEST_CHECK(!cellFormat.hasProperty(QTextFormat::BackgroundBrush));

                    for (auto it=cell.begin(); !it.atEnd(); ++it)
                    {
                        const auto block=it.currentBlock();
                        if (!block.isValid())
                        {
                            continue;
                        }
                        UISE_TEST_CHECK(!block.blockFormat()
                                            .hasProperty(QTextFormat::BackgroundBrush));
                        for (auto fragmentIt=block.begin(); !fragmentIt.atEnd(); ++fragmentIt)
                        {
                            const auto charFormat=fragmentIt.fragment().charFormat();
                            UISE_TEST_CHECK(!charFormat
                                                .hasProperty(QTextFormat::ForegroundBrush));
                            UISE_TEST_CHECK(!charFormat
                                                .hasProperty(QTextFormat::BackgroundBrush));
                        }
                    }
                }
            }
        }
    );
}

namespace {

void pressReturn(MessageEditor& editor)
{
    QKeyEvent press(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);
    QApplication::sendEvent(editor.textEdit(),&press);
}

}

BOOST_AUTO_TEST_CASE(TestReturnKeepsOneBlockPerLine)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.setFinishOnEnter(false);

            auto cursor=editor.textEdit()->textCursor();
            cursor.insertText(QStringLiteral("aa"));
            editor.textEdit()->setTextCursor(cursor);
            pressReturn(editor);
            cursor=editor.textEdit()->textCursor();
            cursor.insertText(QStringLiteral("bb"));
            editor.textEdit()->setTextCursor(cursor);

            // ONE BLOCK PER LINE, and that is load-bearing: every block-level format (list,
            // heading, blockquote, code block, horizontal rule, indent) acts on a block, so a
            // message whose lines shared one block would apply a bullet to all of them at once.
            UISE_TEST_CHECK_EQUAL(editor.textEdit()->document()->blockCount(),2);

            // The export still spells that as ONE newline, not the blank line a block boundary
            // normally becomes -- mergeProseBlocksForExport() joins prose blocks on the export
            // clone, so markdownToHtml() renders "<p>aa<br/>bb</p>" and the composer and bubble
            // agree line for line.
            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Markdown).trimmed(),
                                       QStringLiteral("aa\nbb"));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestBlockFormatAppliesToOneLineOnly)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.setFinishOnEnter(false);

            auto cursor=editor.textEdit()->textCursor();
            cursor.insertText(QStringLiteral("aa"));
            editor.textEdit()->setTextCursor(cursor);
            pressReturn(editor);
            cursor=editor.textEdit()->textCursor();
            cursor.insertText(QStringLiteral("list item"));
            editor.textEdit()->setTextCursor(cursor);

            emit editor.toolbar()->bulletListRequested(true);

            // Only the caret's own line becomes a list item. Regression test for exactly the
            // opposite: while Return inserted a soft break instead of a block, every line shared
            // one block and the bullet swallowed all of them.
            auto* document=editor.textEdit()->document();
            UISE_TEST_REQUIRE_EQUAL(document->blockCount(),2);
            UISE_TEST_CHECK(document->firstBlock().textList()==nullptr);
            UISE_TEST_CHECK(document->lastBlock().textList()!=nullptr);

            const auto markdown=editor.text(TextFormat::Markdown);
            UISE_TEST_CHECK(markdown.contains(QStringLiteral("- list item")));
            UISE_TEST_CHECK(!markdown.contains(QStringLiteral("- aa")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestTwoReturnsGiveABlankLineThatSurvives)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.setFinishOnEnter(false);

            auto cursor=editor.textEdit()->textCursor();
            cursor.insertText(QStringLiteral("aa"));
            editor.textEdit()->setTextCursor(cursor);
            pressReturn(editor);
            pressReturn(editor);
            cursor=editor.textEdit()->textCursor();
            cursor.insertText(QStringLiteral("bb"));
            editor.textEdit()->setTextCursor(cursor);

            const auto markdown=editor.text(TextFormat::Markdown);
            UISE_TEST_CHECK_EQUAL_QSTR(markdown.trimmed(),
                                       QStringLiteral("aa\n%1\nbb").arg(QChar(0x200b)));

            // ...and it survives the Markdown round trip, which is what it exists for.
            editor.setMessageEditingMode(MessageEditingMode::Markdown);
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Markdown).trimmed(),
                                       markdown.trimmed());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestLineBreaksSurviveTheMarkdownRoundTrip)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.loadText(QStringLiteral("aa\nbb\ncc"),TextFormat::Markdown);

            // setMarkdown() follows CommonMark, where a single newline inside a paragraph is a
            // SPACE -- measured, "aa\nbb" comes back as one block reading "aa bb", so a typed
            // line break was destroyed on re-import. markdownWithParagraphPerLine() gives each
            // line its own block again.
            UISE_TEST_CHECK_EQUAL(editor.textEdit()->document()->blockCount(),3);
            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Markdown).trimmed(),
                                       QStringLiteral("aa\nbb\ncc"));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestBlankLinesSurviveMarkdownRoundTrip)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);

            auto cursor=editor.textEdit()->textCursor();
            cursor.insertText(QStringLiteral("Hello"));
            cursor.insertBlock();
            cursor.insertBlock();
            cursor.insertText(QStringLiteral("World"));
            editor.textEdit()->setTextCursor(cursor);

            const auto blocksBefore=editor.textEdit()->document()->blockCount();
            UISE_TEST_REQUIRE_EQUAL(blocksBefore,3);

            // qtextmarkdownwriter writes NOTHING for an empty block, so without help the blank
            // line simply ceases to exist -- measured, 3 blocks in and 2 out, which is what made
            // two tables weld themselves together after a Markdown round trip. Exported as a
            // NO-BREAK SPACE paragraph it survives, the same trick the paragraph indent uses.
            const auto markdown=editor.text(TextFormat::Markdown);
            UISE_TEST_CHECK(markdown.contains(QChar(0x200b)));

            editor.setMessageEditingMode(MessageEditingMode::Markdown);
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            UISE_TEST_CHECK_EQUAL(editor.textEdit()->document()->blockCount(),blocksBefore);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestBlankLineExportLeavesTheLiveDocumentAlone)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);

            auto cursor=editor.textEdit()->textCursor();
            cursor.insertText(QStringLiteral("Hello"));
            cursor.insertBlock();
            cursor.insertBlock();
            cursor.insertText(QStringLiteral("World"));
            editor.textEdit()->setTextCursor(cursor);

            const auto undoBefore=editor.textEdit()->document()->availableUndoSteps();

            UISE_TEST_CHECK(editor.text(TextFormat::Markdown).contains(QChar(0x200b)));

            // The markers exist only in the exported COPY: an export must never edit what the user
            // is typing, nor push anything onto their undo stack.
            UISE_TEST_CHECK(!editor.textEdit()->document()->toRawText().contains(QChar(0x200b)));
            UISE_TEST_CHECK_EQUAL(editor.textEdit()->document()->availableUndoSteps(),undoBefore);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestTableOnlyMessageGainsNoLeadingBlankLine)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);

            emit editor.toolbar()->tableRequested(1,2);

            // A QTextDocument always carries an empty block before a leading table, and the user
            // cannot delete it -- so it is structure, not a blank line anybody typed. Filling it
            // would put a blank line above every message that merely starts with a table.
            const auto markdown=editor.text(TextFormat::Markdown);
            UISE_TEST_CHECK(!markdown.trimmed().startsWith(QChar(0x200b)));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestBlankLineInsideCodeFenceIsNotFilled)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            editor.textEdit()->setPlainText(QStringLiteral("```\ncode\n\nmore\n```"));

            // A marker inside a fence would be a character injected into the user's code.
            const auto markdown=editor.text(TextFormat::Markdown);
            UISE_TEST_CHECK(!markdown.contains(QChar(0x200b)));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestNoBlankLineMarkerNextToATable)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);

            auto cursor=editor.textEdit()->textCursor();
            cursor.insertText(QStringLiteral("cc"));
            cursor.insertBlock();
            editor.textEdit()->setTextCursor(cursor);
            emit editor.toolbar()->tableRequested(2,2);

            // A Return pressed to get OUT of the paragraph above a table is not a blank line the
            // author wanted, and a table does not need one to stand apart -- messagetext.css gives
            // it its own margins. Marking it produced a doubled gap: the margin AND a blank line.
            const auto markdown=editor.text(TextFormat::Markdown);
            UISE_TEST_CHECK(!markdown.contains(QChar(0x200b)));

            // ...while a blank line between two ordinary lines is still preserved.
            MessageEditor plain;
            plain.setMessageEditingMode(MessageEditingMode::Wysiwyg);
            auto plainCursor=plain.textEdit()->textCursor();
            plainCursor.insertText(QStringLiteral("aa"));
            plainCursor.insertBlock();
            plainCursor.insertBlock();
            plainCursor.insertText(QStringLiteral("bb"));
            plain.textEdit()->setTextCursor(plainCursor);
            UISE_TEST_CHECK(plain.text(TextFormat::Markdown).contains(QChar(0x200b)));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestExportCollapsesRedundantBlankLines)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);

            emit editor.toolbar()->tableRequested(2,2);
            auto cursor=editor.textEdit()->textCursor();
            cursor.movePosition(QTextCursor::End);
            editor.textEdit()->setTextCursor(cursor);
            emit editor.toolbar()->tableRequested(2,2);

            // Qt's own writer puts a bare extra newline in front of every table, so two adjacent
            // tables showed TWO blank source lines between them before anything was authored --
            // which is most of why a single authored blank line looked so large in Markdown mode.
            // One blank line separates any two block constructs; more mean the same thing.
            const auto markdown=editor.text(TextFormat::Markdown);
            UISE_TEST_CHECK(!markdown.contains(QStringLiteral("\n\n\n")));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestTableStaysVisibleAcrossModeRoundTrip)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);

            emit editor.toolbar()->tableRequested(2,2);

            auto findTable=[&]() -> QTextTable*
            {
                auto* root=editor.textEdit()->document()->rootFrame();
                for (auto it=root->begin(); !it.atEnd(); ++it)
                {
                    if (auto* candidate=qobject_cast<QTextTable*>(it.currentFrame()))
                    {
                        return candidate;
                    }
                }
                return nullptr;
            };

            auto* table=findTable();
            UISE_TEST_REQUIRE(table!=nullptr);
            UISE_TEST_CHECK_EQUAL(table->format().border(),1.0);

            // A mode round trip rebuilds the document through setMarkdown(), and Qt's importer
            // gives the re-created table border=0 with borderCollapse ON -- which paints nothing
            // at all. Normalizing only on the paste path is what made a table lose its grid the
            // moment the editor was switched to Markdown and back.
            editor.setMessageEditingMode(MessageEditingMode::Markdown);
            editor.setMessageEditingMode(MessageEditingMode::Wysiwyg);

            table=findTable();
            UISE_TEST_REQUIRE(table!=nullptr);
            UISE_TEST_CHECK_EQUAL(table->format().border(),1.0);
            UISE_TEST_CHECK(!table->format().borderCollapse());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestImageOnlyPasteStillGoesToAttachments)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            QSignalSpy attachments(&editor,&AbstractMessageEditor::attachmentsPasted);

            // The other side of the same rule: image bits with no renderable text (a screenshot,
            // or a browser's "copy image", whose HTML is a lone <img> that renders to nothing but
            // a U+FFFC object replacement character) must still reach the attachment flow.
            auto* mime=new QMimeData();
            mime->setHtml(QStringLiteral("<meta charset='utf-8'><img src=\"https://ex.com/cat.png\">"));
            QImage image(4,4,QImage::Format_ARGB32);
            image.fill(Qt::red);
            mime->setImageData(image);
            QApplication::clipboard()->setMimeData(mime);

            editor.textEdit()->pasteFromClipboard();

            UISE_TEST_CHECK_EQUAL(attachments.count(),1);
            UISE_TEST_CHECK(editor.isEmpty());
        }
    );
}

BOOST_AUTO_TEST_SUITE_END()
