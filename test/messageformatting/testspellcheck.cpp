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

/** @file uise/test/messageformatting/testspellcheck.cpp
*
*  Tests the spellcheck seam added to the message editor (task-spellcheck.md): the highlighter's
*  misspelling pass, the tokenizer's word rules, the checker-attach/detach wiring, the async
*  dictionaryChanged() -> rehighlight() path, and the context menu's suggestion rows -- all
*  against a stub AbstractSpellChecker, with no real dictionary present anywhere in this suite.
*
*  Same GUI-thread marshalling as testmessageeditor.cpp: almost every case constructs a
*  MessageEditor, so almost every case runs via TestThread::execGuiThread(). Widgets are
*  stack-allocated inside the lambda and never shown -- isHidden() is used instead of isVisible().
*
*/

/****************************************************************************/

#include <boost/test/unit_test.hpp>

#include <QTest>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextLayout>
#include <QTextFormat>
#include <QColor>
#include <QSet>

#include <uise/test/uise-testthread.hpp>

#include <uise/desktop/messageeditor.hpp>
#include <uise/desktop/messageeditortoolbar.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/abstractspellchecker.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

namespace {

/** @brief A checker with no dictionary and no I/O -- every word not explicitly marked bad is
 *  accepted, so the whole seam is testable with nothing resembling hunspell present.
 *
 * Every check() call is recorded in asked(), which is what most cases here assert against
 * instead of (or alongside) the painted squiggle -- it is the most direct way to prove a link,
 * mention, code fence or inline-code run was never even OFFERED to the checker, not merely that
 * it was offered and painted nothing.
 */
class StubSpellChecker : public AbstractSpellChecker
{
    public:

        bool isReady() const override
        {
            return m_ready;
        }

        void setReady(bool ready)
        {
            m_ready=ready;
        }

        void markBad(const QString& word)
        {
            m_bad.insert(word);
        }

        void unmarkBad(const QString& word)
        {
            m_bad.remove(word);
        }

        //! When set, every check() answers this regardless of m_bad -- for the Unknown case.
        void forceVerdict(SpellCheckVerdict verdict)
        {
            m_forced=verdict;
            m_forcing=true;
        }

        SpellCheckVerdict check(const QString& word) const override
        {
            m_asked.push_back(word);
            if (m_forcing)
            {
                return m_forced;
            }
            return m_bad.contains(word) ? SpellCheckVerdict::Misspelled : SpellCheckVerdict::Correct;
        }

        QStringList suggestions(const QString& word, int maxCount) const override
        {
            Q_UNUSED(word)
            QStringList out=m_suggestions;
            while (out.size()>maxCount)
            {
                out.removeLast();
            }
            return out;
        }

        void setSuggestions(QStringList suggestions)
        {
            m_suggestions=std::move(suggestions);
        }

        bool canAddToDictionary() const override
        {
            return m_canAddToDictionary;
        }

        void setCanAddToDictionary(bool enable)
        {
            m_canAddToDictionary=enable;
        }

        void addToDictionary(const QString& word) override
        {
            m_addedToDictionary.push_back(word);
            m_bad.remove(word);
            emit dictionaryChanged();
        }

        void ignoreWord(const QString& word) override
        {
            m_ignored.push_back(word);
        }

        const QStringList& asked() const
        {
            return m_asked;
        }

        const QStringList& addedToDictionary() const
        {
            return m_addedToDictionary;
        }

        const QStringList& ignored() const
        {
            return m_ignored;
        }

    private:

        bool m_ready=true;
        QSet<QString> m_bad;
        QStringList m_suggestions;
        bool m_canAddToDictionary=false;
        bool m_forcing=false;
        SpellCheckVerdict m_forced=SpellCheckVerdict::Correct;

        mutable QStringList m_asked;
        QStringList m_addedToDictionary;
        QStringList m_ignored;
};

//! Whether any layout FormatRange in `block` carries the spellcheck squiggle -- the idiom
//! testmessageeditor.cpp's link-colour cases already use for reading a highlighter's OWN
//! (display-only) formats, at testmessageeditor.cpp:3825 and friends.
bool hasSpellUnderline(const QTextBlock& block)
{
    const auto formats=block.layout()->formats();
    for (const auto& range : formats)
    {
        if (range.format.underlineStyle()==QTextCharFormat::SpellCheckUnderline)
        {
            return true;
        }
    }
    return false;
}

//! The colour of the FIRST range carrying the squiggle, or an invalid QColor if none does.
QColor spellUnderlineColor(const QTextBlock& block)
{
    const auto formats=block.layout()->formats();
    for (const auto& range : formats)
    {
        if (range.format.underlineStyle()==QTextCharFormat::SpellCheckUnderline)
        {
            return range.format.underlineColor();
        }
    }
    return {};
}

//! Whether a range carrying BOTH `color` as foreground AND the squiggle exists -- the regression
//! guard for highlightMisspellings() seeding its format from format(pos) rather than a blank one.
bool hasColorAndSpellUnderline(const QTextBlock& block, const QColor& color)
{
    const auto formats=block.layout()->formats();
    for (const auto& range : formats)
    {
        if (range.format.underlineStyle()==QTextCharFormat::SpellCheckUnderline
            && range.format.foreground().color()==color)
        {
            return true;
        }
    }
    return false;
}

MenuItem* findItem(std::vector<MenuItem>& items, int id)
{
    for (auto& item : items)
    {
        if (item.id==id)
        {
            return &item;
        }
    }
    return nullptr;
}

}

BOOST_AUTO_TEST_SUITE(TestMessageEditorSpellCheck)

BOOST_AUTO_TEST_CASE(TestNoCheckerPaintsNothing)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.textEdit()->setSpellCheckUnderlineColor(QColor(0xFF,0x3B,0x30));
            editor.loadText(QStringLiteral("xyzzy"),TextFormat::Plain);

            UISE_TEST_CHECK(!hasSpellUnderline(editor.textEdit()->document()->firstBlock()));
        }
    );
}

BOOST_AUTO_TEST_CASE(TestSpellUnderlinePainted)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            StubSpellChecker checker;
            checker.markBad(QStringLiteral("xyzzy"));

            editor.textEdit()->setSpellCheckUnderlineColor(QColor(0xFF,0x3B,0x30));
            editor.setSpellChecker(&checker);
            editor.loadText(QStringLiteral("xyzzy correct"),TextFormat::Plain);

            const auto block=editor.textEdit()->document()->firstBlock();
            UISE_TEST_CHECK(hasSpellUnderline(block));
            UISE_TEST_CHECK(spellUnderlineColor(block)==QColor(0xFF,0x3B,0x30));

            editor.setSpellChecker(nullptr);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestSpellHighlightIsDisplayOnly)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            StubSpellChecker checker;
            checker.markBad(QStringLiteral("xyzzy"));

            editor.textEdit()->setSpellCheckUnderlineColor(QColor(0xFF,0x3B,0x30));
            editor.setSpellChecker(&checker);
            editor.loadText(QStringLiteral("xyzzy"),TextFormat::Plain);

            UISE_TEST_CHECK(hasSpellUnderline(editor.textEdit()->document()->firstBlock()));

            // Same terms as TestCodeFenceHighlightIsDisplayOnly (testmessageeditor.cpp): nothing
            // is written into the document's own char formats, so nothing is baked in and
            // nothing leaks into an export.
            const auto fragment=editor.textEdit()->document()->firstBlock().begin().fragment();
            UISE_TEST_CHECK(fragment.charFormat().underlineStyle()==QTextCharFormat::NoUnderline);
            UISE_TEST_CHECK(!editor.text(TextFormat::Html).contains(QStringLiteral("#FF3B30"),Qt::CaseInsensitive));

            editor.setSpellChecker(nullptr);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestSpellSkipsLinksAndMentions)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.insertLink(QStringLiteral("https://example.com"),QStringLiteral("Example"));
            auto cursor=editor.textEdit()->textCursor();
            cursor.movePosition(QTextCursor::End);
            cursor.insertText(QStringLiteral(" checkme"));
            editor.textEdit()->setTextCursor(cursor);

            editor.insertMention(QStringLiteral("usr1"),QStringLiteral("Alice"));
            cursor=editor.textEdit()->textCursor();
            cursor.movePosition(QTextCursor::End);
            cursor.insertText(QStringLiteral(" checkme2"));
            editor.textEdit()->setTextCursor(cursor);

            StubSpellChecker checker;
            checker.markBad(QStringLiteral("Example"));
            checker.markBad(QStringLiteral("Alice"));
            // Attached AFTER the content exists, so the very first highlight pass already runs
            // with the anchors in place -- see nonProseRuns().
            editor.setSpellChecker(&checker);

            UISE_TEST_CHECK(!checker.asked().contains(QStringLiteral("Example")));
            UISE_TEST_CHECK(!checker.asked().contains(QStringLiteral("Alice")));
            UISE_TEST_CHECK(checker.asked().contains(QStringLiteral("checkme")));
            UISE_TEST_CHECK(checker.asked().contains(QStringLiteral("checkme2")));

            editor.setSpellChecker(nullptr);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestSpellSkipsCodeFence)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.loadText(QStringLiteral("checkme\n```\nxyzzy\n```"),TextFormat::Plain);

            StubSpellChecker checker;
            checker.markBad(QStringLiteral("xyzzy"));
            editor.setSpellChecker(&checker);

            UISE_TEST_CHECK(!checker.asked().contains(QStringLiteral("xyzzy")));
            UISE_TEST_CHECK(checker.asked().contains(QStringLiteral("checkme")));

            editor.setSpellChecker(nullptr);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestSpellSkipsInlineCode)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.loadText(QStringLiteral("checkme word"),TextFormat::Plain);

            auto cursor=editor.textEdit()->textCursor();
            cursor.movePosition(QTextCursor::End);
            cursor.movePosition(QTextCursor::PreviousWord,QTextCursor::KeepAnchor);
            editor.textEdit()->setTextCursor(cursor);
            editor.toolbar()->button(MessageEditorToolbarButton::InlineCode)->click();

            StubSpellChecker checker;
            checker.markBad(QStringLiteral("word"));
            // Attached after the inline-code run already exists in the document.
            editor.setSpellChecker(&checker);

            UISE_TEST_CHECK(!checker.asked().contains(QStringLiteral("word")));
            UISE_TEST_CHECK(checker.asked().contains(QStringLiteral("checkme")));

            editor.setSpellChecker(nullptr);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestSpellKeepsBlockquoteColor)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            editor.textEdit()->setBlockquoteColor(QColor(0x66,0x66,0x66));
            editor.loadText(QStringLiteral("xyzzy"),TextFormat::Plain);

            auto cursor=editor.textEdit()->textCursor();
            cursor.select(QTextCursor::Document);
            editor.textEdit()->setTextCursor(cursor);
            editor.toolbar()->button(MessageEditorToolbarButton::Blockquote)->click();

            StubSpellChecker checker;
            checker.markBad(QStringLiteral("xyzzy"));
            editor.setSpellChecker(&checker);

            // One and the same range carries BOTH the quote colour and the squiggle -- the
            // regression guard for highlightMisspellings() seeding from format(pos) instead of a
            // blank QTextCharFormat (QSyntaxHighlighter::setFormat() ASSIGNS, it does not merge
            // onto this highlighter's own earlier formats).
            UISE_TEST_CHECK(hasColorAndSpellUnderline(
                editor.textEdit()->document()->firstBlock(),QColor(0x66,0x66,0x66)
            ));

            editor.setSpellChecker(nullptr);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestSpellTokenizerRules)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            StubSpellChecker checker;
            editor.setSpellChecker(&checker);
            editor.loadText(
                QStringLiteral("don't well-known abc123 getFooBar user@example.com "
                                "https://x.example/teh @alice #tag NASA a"),
                TextFormat::Plain
            );

            const QSet<QString> expected{
                QStringLiteral("don't"),QStringLiteral("well-known"),QStringLiteral("NASA")
            };
            const QSet<QString> asked(checker.asked().begin(),checker.asked().end());
            UISE_TEST_CHECK(asked==expected);

            editor.setSpellChecker(nullptr);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestSpellUnknownPaintsNothing)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            StubSpellChecker checker;
            checker.forceVerdict(SpellCheckVerdict::Unknown);

            editor.setSpellChecker(&checker);
            editor.loadText(QStringLiteral("xyzzy"),TextFormat::Plain);

            UISE_TEST_CHECK(checker.asked().contains(QStringLiteral("xyzzy")));
            UISE_TEST_CHECK(!hasSpellUnderline(editor.textEdit()->document()->firstBlock()));

            editor.setSpellChecker(nullptr);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestDictionaryChangedRehighlights)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            StubSpellChecker checker;
            checker.forceVerdict(SpellCheckVerdict::Unknown);

            editor.setSpellChecker(&checker);
            editor.loadText(QStringLiteral("xyzzy"),TextFormat::Plain);
            UISE_TEST_CHECK(!hasSpellUnderline(editor.textEdit()->document()->firstBlock()));

            checker.forceVerdict(SpellCheckVerdict::Misspelled);
            emit checker.dictionaryChanged();

            // Past the 150ms debounce in EnhancedTextEdit::onSpellDictionaryChanged().
            QTest::qWait(300);

            UISE_TEST_CHECK(hasSpellUnderline(editor.textEdit()->document()->firstBlock()));

            editor.setSpellChecker(nullptr);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestSpellDisabledPaintsNothing)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            StubSpellChecker checker;
            checker.markBad(QStringLiteral("xyzzy"));

            editor.setSpellChecker(&checker);
            editor.setSpellCheckEnabled(false);
            editor.loadText(QStringLiteral("xyzzy"),TextFormat::Plain);

            UISE_TEST_CHECK(checker.asked().isEmpty());
            UISE_TEST_CHECK(!hasSpellUnderline(editor.textEdit()->document()->firstBlock()));

            editor.setSpellChecker(nullptr);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestSpellCheckButtonHiddenByDefault)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            UISE_TEST_CHECK(editor.toolbar()->button(MessageEditorToolbarButton::SpellCheck)->isHidden());

            editor.setSpellCheckButtonVisible(true);
            UISE_TEST_CHECK(!editor.toolbar()->button(MessageEditorToolbarButton::SpellCheck)->isHidden());
        }
    );
}

BOOST_AUTO_TEST_CASE(TestContextMenuOffersSuggestions)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            StubSpellChecker checker;
            checker.markBad(QStringLiteral("xyzzy"));
            checker.setSuggestions({QStringLiteral("one"),QStringLiteral("two"),QStringLiteral("three")});
            checker.setCanAddToDictionary(true);

            editor.setSpellChecker(&checker);
            editor.setSpellCheckMenuItemVisible(true);
            // "xyzzy" starts the document -- QPoint(0,0) hit-tests to document position 0 on an
            // unshown widget, which is what showContextMenu()'s cursorForPosition(pos) needs to
            // land on it.
            editor.loadText(QStringLiteral("xyzzy correct"),TextFormat::Plain);

            std::vector<MenuItem> captured;
            editor.setContextMenuHandler([&](std::vector<MenuItem>& items) { captured=items; });
            QMetaObject::invokeMethod(&editor,"showContextMenu",Q_ARG(QPoint,QPoint(0,0)));

            auto* s0=findItem(captured,static_cast<int>(MessageEditorMenuAction::SpellSuggestionFirst)+0);
            auto* s1=findItem(captured,static_cast<int>(MessageEditorMenuAction::SpellSuggestionFirst)+1);
            auto* s2=findItem(captured,static_cast<int>(MessageEditorMenuAction::SpellSuggestionFirst)+2);
            UISE_TEST_REQUIRE(s0!=nullptr);
            UISE_TEST_REQUIRE(s1!=nullptr);
            UISE_TEST_REQUIRE(s2!=nullptr);
            UISE_TEST_CHECK_EQUAL_QSTR(s0->text,QStringLiteral("one"));
            UISE_TEST_CHECK_EQUAL_QSTR(s1->text,QStringLiteral("two"));
            UISE_TEST_CHECK_EQUAL_QSTR(s2->text,QStringLiteral("three"));

            UISE_TEST_CHECK(findItem(captured,static_cast<int>(MessageEditorMenuAction::AddToDictionary))!=nullptr);
            UISE_TEST_CHECK(findItem(captured,static_cast<int>(MessageEditorMenuAction::IgnoreWord))!=nullptr);

            auto* toggle=findItem(captured,static_cast<int>(MessageEditorMenuAction::SpellCheckEnabled));
            UISE_TEST_REQUIRE(toggle!=nullptr);
            UISE_TEST_CHECK(toggle->isCheckable);
            UISE_TEST_CHECK(toggle->isChecked);

            editor.setSpellChecker(nullptr);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestContextMenuSuppressesWordActionsForMultiWordSelection)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            StubSpellChecker checker;
            checker.markBad(QStringLiteral("xyzzy"));
            checker.setSuggestions({QStringLiteral("one")});
            checker.setCanAddToDictionary(true);

            editor.setSpellChecker(&checker);
            editor.setSpellCheckMenuItemVisible(true);
            editor.loadText(QStringLiteral("xyzzy correct"),TextFormat::Plain);

            // Select the WHOLE document -- both words -- before right-clicking on "xyzzy".
            // Suggest/Add/Ignore act on a single word, which is ambiguous the moment more than
            // one word is selected (and there is no such thing as a multi-word dictionary entry
            // -- every check() call is tokenized on whitespace first).
            auto cursor=editor.textEdit()->textCursor();
            cursor.select(QTextCursor::Document);
            editor.textEdit()->setTextCursor(cursor);

            std::vector<MenuItem> captured;
            editor.setContextMenuHandler([&](std::vector<MenuItem>& items) { captured=items; });
            QMetaObject::invokeMethod(&editor,"showContextMenu",Q_ARG(QPoint,QPoint(0,0)));

            UISE_TEST_CHECK(findItem(captured,static_cast<int>(MessageEditorMenuAction::SpellSuggestionFirst))==nullptr);
            UISE_TEST_CHECK(findItem(captured,static_cast<int>(MessageEditorMenuAction::AddToDictionary))==nullptr);
            UISE_TEST_CHECK(findItem(captured,static_cast<int>(MessageEditorMenuAction::IgnoreWord))==nullptr);

            // The global "Check spelling" toggle is unrelated to word-specific actions and stays
            // offered regardless of selection.
            UISE_TEST_CHECK(findItem(captured,static_cast<int>(MessageEditorMenuAction::SpellCheckEnabled))!=nullptr);

            editor.setSpellChecker(nullptr);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestContextMenuAllowsWordActionsForExactWordSelection)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            StubSpellChecker checker;
            checker.markBad(QStringLiteral("xyzzy"));
            checker.setSuggestions({QStringLiteral("one")});

            editor.setSpellChecker(&checker);
            editor.setSpellCheckMenuItemVisible(true);
            editor.loadText(QStringLiteral("xyzzy correct"),TextFormat::Plain);

            // Select EXACTLY "xyzzy" -- e.g. what a double-click on the misspelling itself would
            // produce -- and confirm the actions still show: a single word selected is
            // unambiguous, unlike TestContextMenuSuppressesWordActionsForMultiWordSelection.
            auto cursor=editor.textEdit()->textCursor();
            cursor.setPosition(0);
            cursor.setPosition(5,QTextCursor::KeepAnchor);
            editor.textEdit()->setTextCursor(cursor);

            std::vector<MenuItem> captured;
            editor.setContextMenuHandler([&](std::vector<MenuItem>& items) { captured=items; });
            QMetaObject::invokeMethod(&editor,"showContextMenu",Q_ARG(QPoint,QPoint(0,0)));

            UISE_TEST_CHECK(findItem(captured,static_cast<int>(MessageEditorMenuAction::SpellSuggestionFirst))!=nullptr);
            UISE_TEST_CHECK(findItem(captured,static_cast<int>(MessageEditorMenuAction::IgnoreWord))!=nullptr);

            editor.setSpellChecker(nullptr);
        }
    );
}

BOOST_AUTO_TEST_CASE(TestSuggestionAppliesAndIsSingleUndo)
{
    TestThread::instance()->execGuiThread(
        [&]()
        {
            MessageEditor editor;
            StubSpellChecker checker;
            checker.markBad(QStringLiteral("xyzzy"));
            checker.setSuggestions({QStringLiteral("one"),QStringLiteral("two")});

            editor.setSpellChecker(&checker);
            editor.setSpellCheckMenuItemVisible(true);
            editor.loadText(QStringLiteral("xyzzy correct"),TextFormat::Plain);

            QMetaObject::invokeMethod(&editor,"showContextMenu",Q_ARG(QPoint,QPoint(0,0)));
            QMetaObject::invokeMethod(&editor,"onContextMenuItemTriggered",
                Q_ARG(int,static_cast<int>(MessageEditorMenuAction::SpellSuggestionFirst))
            );

            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Plain),QStringLiteral("one correct"));

            UISE_TEST_CHECK(editor.textEdit()->document()->isUndoAvailable());
            editor.textEdit()->undo();
            UISE_TEST_CHECK_EQUAL_QSTR(editor.text(TextFormat::Plain),QStringLiteral("xyzzy correct"));

            editor.setSpellChecker(nullptr);
        }
    );
}

BOOST_AUTO_TEST_SUITE_END()
