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

/** @file uise/test/chatobjectbrowser/testlineeditcompletion.cpp
*
*  Test of LineEditCompletion (the word under the caret, the question to the source, the list and its keys) and of its use
*  in the search line of the chat object browsers (setCompletionCb()).
*
*/

/****************************************************************************/

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QStringList>
#include <QWidget>

#include <boost/test/unit_test.hpp>

#include <uise/test/uise-testthread.hpp>
#include <uise/desktop/chatobjectlistbrowser.hpp>
#include <uise/desktop/lineeditcompletion.hpp>
#include <uise/desktop/style.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

//--------------------------------------------------------------------------
// Strict inequalities are written as UISE_TEST_CHECK(a<b): UISE_TEST_CHECK_GT is wired to
// BOOST_CHECK_GE and there is no UISE_TEST_CHECK_LT at all.
//--------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(TestLineEditCompletion)

namespace {

//! Typed the way a user does: one key event per character, which the line turns into textEdited().
void typeText(QLineEdit* edit, const QString& text)
{
    for (const auto ch : text)
    {
        QKeyEvent press(QEvent::KeyPress,0,Qt::NoModifier,QString(ch));
        QApplication::sendEvent(edit,&press);
    }
}

void pressKey(QLineEdit* edit, int key)
{
    QKeyEvent press(QEvent::KeyPress,key,Qt::NoModifier);
    QApplication::sendEvent(edit,&press);
}

//! A source that knows some tags and answers at once with those that begin with the word.
LineEditCompletion::SourceCb tagsSource(QStringList tags, std::vector<QString>* asked=nullptr)
{
    return [tags,asked](const QString& word, LineEditCompletion::AnswerCb answer)
    {
        if (asked!=nullptr)
        {
            asked->push_back(word);
        }
        QStringList found;
        for (const auto& tag : tags)
        {
            if (tag.startsWith(word,Qt::CaseInsensitive))
            {
                found.push_back(tag);
            }
        }
        answer(found);
    };
}

//! The line under test, the widget the list is placed in, and the completion, with no pause before a question.
struct Fixture
{
    Fixture()
        : host(new QWidget()),
          edit(new QLineEdit(host.get())),
          completion(new LineEditCompletion(edit,host.get()))
    {
        completion->setDelayMs(0);
    }

    //! The line with the text and the caret at the end.
    void setTextAtEnd(const QString& text)
    {
        edit->setText(text);
        edit->setCursorPosition(text.size());
    }

    std::unique_ptr<QWidget> host;
    QLineEdit* edit;
    LineEditCompletion* completion;
};

} // anonymous namespace

//--------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestWordAtCaret)
{
    runInGui(
        []()
        {
            Fixture f;

            f.setTextAtEnd(QStringLiteral("see #tra"));
            auto word=f.completion->wordAtCaret();
            UISE_TEST_CHECK_EQUAL(word.start,4);
            UISE_TEST_CHECK_EQUAL(word.end,8);
            UISE_TEST_CHECK_EQUAL_QSTR(word.text,QStringLiteral("#tra"));

            // a bare trigger is a word: the first tags of the chat
            f.setTextAtEnd(QStringLiteral("#"));
            word=f.completion->wordAtCaret();
            UISE_TEST_CHECK_EQUAL(word.start,0);
            UISE_TEST_CHECK_EQUAL_QSTR(word.text,QStringLiteral("#"));

            // the caret in the middle of a word: what is typed of it is the text, the whole word is replaced by a pick
            f.edit->setText(QStringLiteral("#travel and more"));
            f.edit->setCursorPosition(4);
            word=f.completion->wordAtCaret();
            UISE_TEST_CHECK_EQUAL(word.start,0);
            UISE_TEST_CHECK_EQUAL(word.end,7);
            UISE_TEST_CHECK_EQUAL_QSTR(word.text,QStringLiteral("#tra"));

            // an underscore and non-ASCII letters belong to a word
            f.setTextAtEnd(QStringLiteral("#hello_мир"));
            UISE_TEST_CHECK_EQUAL_QSTR(f.completion->wordAtCaret().text,QStringLiteral("#hello_мир"));

            // a trigger after another one, and after a bracket, starts a word
            f.setTextAtEnd(QStringLiteral("##tr"));
            UISE_TEST_CHECK_EQUAL(f.completion->wordAtCaret().start,1);
            f.setTextAtEnd(QStringLiteral("(#tr"));
            UISE_TEST_CHECK_EQUAL(f.completion->wordAtCaret().start,1);

            // glued to a letter or a digit before it, or no trigger at all, or a space in between: no word
            f.setTextAtEnd(QStringLiteral("abc#tr"));
            UISE_TEST_CHECK_EQUAL(f.completion->wordAtCaret().start,-1);
            f.setTextAtEnd(QStringLiteral("5#tr"));
            UISE_TEST_CHECK_EQUAL(f.completion->wordAtCaret().start,-1);
            f.setTextAtEnd(QStringLiteral("no tag here"));
            UISE_TEST_CHECK_EQUAL(f.completion->wordAtCaret().start,-1);
            f.setTextAtEnd(QStringLiteral("# tr"));
            UISE_TEST_CHECK_EQUAL(f.completion->wordAtCaret().start,-1);
        }
    );
}

//--------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestTypingOpensTheListAndTheKeysDriveIt)
{
    runInGui(
        []()
        {
            Fixture f;
            std::vector<QString> asked;
            f.completion->setSource(tagsSource({"#idea","#travel","#travelblog","#travels"},&asked));
            QStringList picked;
            QObject::connect(f.completion,&LineEditCompletion::accepted,f.host.get(),
                [&picked](const QString& candidate)
                {
                    picked.push_back(candidate);
                }
            );

            typeText(f.edit,QStringLiteral("#tr"));
            UISE_TEST_REQUIRE(!asked.empty());
            UISE_TEST_CHECK_EQUAL_QSTR(asked.back(),QStringLiteral("#tr"));
            UISE_TEST_CHECK(f.completion->isOpen());
            UISE_TEST_CHECK_EQUAL(f.completion->candidates().size(),3);
            UISE_TEST_CHECK_EQUAL_QSTR(f.completion->candidates().front(),QStringLiteral("#travel"));
            UISE_TEST_CHECK_EQUAL(f.completion->currentRow(),0);

            // typing goes on in the same word: the list stays and follows the text
            typeText(f.edit,QStringLiteral("avel"));
            UISE_TEST_CHECK(f.completion->isOpen());
            UISE_TEST_CHECK_EQUAL(f.completion->candidates().size(),3);
            // what is typed is the one candidate: the word is finished, no list
            typeText(f.edit,QStringLiteral("s"));
            UISE_TEST_CHECK(!f.completion->isOpen());

            // Down and Up move the selection and wrap around
            f.edit->clear();
            typeText(f.edit,QStringLiteral("#tr"));
            UISE_TEST_REQUIRE(f.completion->isOpen());
            pressKey(f.edit,Qt::Key_Down);
            UISE_TEST_CHECK_EQUAL(f.completion->currentRow(),1);
            pressKey(f.edit,Qt::Key_Down);
            pressKey(f.edit,Qt::Key_Down);
            UISE_TEST_CHECK_EQUAL(f.completion->currentRow(),0);
            pressKey(f.edit,Qt::Key_Up);
            UISE_TEST_CHECK_EQUAL(f.completion->currentRow(),2);

            // Enter picks it: the word is replaced, with a space after it at the end of the text, and the list is closed
            pressKey(f.edit,Qt::Key_Return);
            UISE_TEST_CHECK_EQUAL_QSTR(f.edit->text(),QStringLiteral("#travels "));
            UISE_TEST_CHECK_EQUAL(f.edit->cursorPosition(),9);
            UISE_TEST_CHECK(!f.completion->isOpen());
            UISE_TEST_REQUIRE_EQUAL(picked.size(),1);
            UISE_TEST_CHECK_EQUAL_QSTR(picked.front(),QStringLiteral("#travels"));

            // the new text is not asked about: no list after a pick
            UISE_TEST_CHECK(!f.completion->isOpen());

            // Tab picks too
            f.edit->clear();
            typeText(f.edit,QStringLiteral("#tr"));
            UISE_TEST_REQUIRE(f.completion->isOpen());
            pressKey(f.edit,Qt::Key_Tab);
            UISE_TEST_CHECK_EQUAL_QSTR(f.edit->text(),QStringLiteral("#travel "));
        }
    );
}

//--------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestPickInTheMiddleReplacesTheWholeWord)
{
    runInGui(
        []()
        {
            Fixture f;
            f.completion->setSource(tagsSource({"#travel"}));

            // the word is not at the end: no space is added, the part after the caret is gone with the word
            f.edit->setText(QStringLiteral("#trxyz and more"));
            f.edit->setCursorPosition(3);
            typeText(f.edit,QStringLiteral("a"));     // "#tra|xyz": the caret is in the word, the word is "#tra"
            UISE_TEST_REQUIRE(f.completion->isOpen());
            UISE_TEST_CHECK(f.completion->acceptCurrent());
            UISE_TEST_CHECK_EQUAL_QSTR(f.edit->text(),QStringLiteral("#travel and more"));
            UISE_TEST_CHECK_EQUAL(f.edit->cursorPosition(),7);
        }
    );
}

//--------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestEscapeClosesTheListAndNothingElseSeesIt)
{
    runInGui(
        []()
        {
            // something else that watches the line, installed BEFORE the list opens, like a page that closes on Escape
            struct Watcher : public QObject
            {
                int escapes=0;
                int overrides=0;

                bool eventFilter(QObject*, QEvent* event) override
                {
                    if (event->type()==QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key()==Qt::Key_Escape)
                    {
                        ++escapes;
                    }
                    if (event->type()==QEvent::ShortcutOverride && static_cast<QKeyEvent*>(event)->key()==Qt::Key_Escape)
                    {
                        ++overrides;
                    }
                    return false;
                }
            };

            Fixture f;
            Watcher watcher;
            f.edit->installEventFilter(&watcher);
            f.completion->setSource(tagsSource({"#travel","#trip"}));

            typeText(f.edit,QStringLiteral("#tr"));
            UISE_TEST_REQUIRE(f.completion->isOpen());

            // the key the list takes: a shortcut bound to it does not fire, and the watcher does not see it
            QKeyEvent shortcutOverride(QEvent::ShortcutOverride,Qt::Key_Escape,Qt::NoModifier);
            QApplication::sendEvent(f.edit,&shortcutOverride);
            UISE_TEST_CHECK(shortcutOverride.isAccepted());
            pressKey(f.edit,Qt::Key_Escape);
            UISE_TEST_CHECK(!f.completion->isOpen());
            UISE_TEST_CHECK_EQUAL(watcher.escapes,0);
            UISE_TEST_CHECK_EQUAL(watcher.overrides,0);
            UISE_TEST_CHECK_EQUAL_QSTR(f.edit->text(),QStringLiteral("#tr"));

            // the next one is the page's
            pressKey(f.edit,Qt::Key_Escape);
            UISE_TEST_CHECK_EQUAL(watcher.escapes,1);
        }
    );
}

//--------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestAnAnswerThatIsNotWantedAnyMoreIsDropped)
{
    runInGui(
        []()
        {
            Fixture f;
            // the answers are kept and given by the test, later, in the order it likes
            std::vector<std::pair<QString,LineEditCompletion::AnswerCb>> pending;
            f.completion->setSource(
                [&pending](const QString& word, LineEditCompletion::AnswerCb answer)
                {
                    pending.emplace_back(word,std::move(answer));
                }
            );

            typeText(f.edit,QStringLiteral("#tr"));
            UISE_TEST_REQUIRE_EQUAL(pending.size(),3);     // "#", "#t", "#tr"
            UISE_TEST_CHECK(!f.completion->isOpen());

            // the answer of an older question is dropped, the last one is taken
            pending[1].second(QStringList{"#trip"});
            UISE_TEST_CHECK(!f.completion->isOpen());
            pending[2].second(QStringList{"#travel","#trip"});
            UISE_TEST_CHECK(f.completion->isOpen());
            UISE_TEST_CHECK_EQUAL(f.completion->candidates().size(),2);

            // an answer after the list was closed is dropped
            f.completion->close();
            UISE_TEST_CHECK(!f.completion->isOpen());
            pending[2].second(QStringList{"#travel","#trip"});
            UISE_TEST_CHECK(!f.completion->isOpen());

            // an answer for a word that is not under the caret any more is dropped
            pending.clear();
            typeText(f.edit,QStringLiteral("a"));
            UISE_TEST_REQUIRE_EQUAL(pending.size(),1);
            f.edit->setCursorPosition(0);
            pending.back().second(QStringList{"#travel","#trap"});
            UISE_TEST_CHECK(!f.completion->isOpen());
        }
    );
}

//--------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestWhatDoesNotNeedAListShowsNone)
{
    runInGui(
        []()
        {
            Fixture f;

            // no source: nothing is asked and nothing is shown
            typeText(f.edit,QStringLiteral("#tr"));
            UISE_TEST_CHECK(!f.completion->isOpen());

            // nothing found
            f.completion->setSource(tagsSource({"#travel"}));
            f.edit->clear();
            typeText(f.edit,QStringLiteral("#x"));
            UISE_TEST_CHECK(!f.completion->isOpen());

            // the one candidate is what is typed: Enter keeps its own meaning
            f.edit->clear();
            typeText(f.edit,QStringLiteral("#travel"));
            UISE_TEST_CHECK(!f.completion->isOpen());

            // a text with no word to complete closes an open list
            f.edit->clear();
            f.completion->setSource(tagsSource({"#travel","#trip"}));
            typeText(f.edit,QStringLiteral("#tr"));
            UISE_TEST_REQUIRE(f.completion->isOpen());
            typeText(f.edit,QStringLiteral(" "));
            UISE_TEST_CHECK(!f.completion->isOpen());

            // the caret leaving the word closes it too, and a source taken away
            f.edit->clear();
            typeText(f.edit,QStringLiteral("#tr"));
            UISE_TEST_REQUIRE(f.completion->isOpen());
            f.edit->setCursorPosition(0);
            UISE_TEST_CHECK(!f.completion->isOpen());
            f.edit->clear();
            typeText(f.edit,QStringLiteral("#tr"));
            UISE_TEST_REQUIRE(f.completion->isOpen());
            f.completion->setSource(nullptr);
            UISE_TEST_CHECK(!f.completion->isOpen());

            // more candidates than rows: the first rows are offered
            QStringList many;
            for (int i=0;i<LineEditCompletion::MaxRows+5;++i)
            {
                many.push_back(QStringLiteral("#tag%1").arg(i,2,10,QLatin1Char('0')));
            }
            f.completion->setSource(tagsSource(many));
            f.edit->clear();
            typeText(f.edit,QStringLiteral("#ta"));
            UISE_TEST_CHECK_EQUAL(f.completion->candidates().size(),LineEditCompletion::MaxRows);
        }
    );
}

//--------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestAClickPicks)
{
    runInGui(
        []()
        {
            Fixture f;
            f.completion->setSource(tagsSource({"#travel","#trip"}));
            typeText(f.edit,QStringLiteral("#tr"));
            UISE_TEST_REQUIRE(f.completion->isOpen());

            auto* list=f.host->findChild<QListWidget*>("completionList");
            UISE_TEST_REQUIRE(list!=nullptr);
            UISE_TEST_CHECK_EQUAL(list->count(),2);
            list->itemClicked(list->item(1));
            UISE_TEST_CHECK_EQUAL_QSTR(f.edit->text(),QStringLiteral("#trip "));
            UISE_TEST_CHECK(!f.completion->isOpen());
        }
    );
}

//--------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(TestTheSearchLineOfABrowser)
{
    runInGui(
        []()
        {
            std::unique_ptr<uise::ChatObjectListBrowser> browser{uise::makeChatObjectSearchBrowser(nullptr)};
            UISE_TEST_REQUIRE(browser!=nullptr);
            UISE_TEST_CHECK(browser->completion()==nullptr);

            std::vector<QString> queries;
            browser->setRequestCb(
                [&queries](const ChatObjectsRequest& request)
                {
                    queries.push_back(request.query);
                }
            );
            browser->reload();

            auto* edit=browser->findChild<QLineEdit*>("searchEdit");
            UISE_TEST_REQUIRE(edit!=nullptr);

            browser->setCompletionCb(tagsSource({"#travel","#trip"}));
            UISE_TEST_REQUIRE(browser->completion()!=nullptr);
            browser->completion()->setDelayMs(0);

            typeText(edit,QStringLiteral("#tr"));
            UISE_TEST_CHECK(browser->completion()->isOpen());

            // a pick runs the search at once, not after the typing pause
            const auto before=queries.size();
            pressKey(edit,Qt::Key_Return);
            UISE_TEST_CHECK_EQUAL_QSTR(browser->searchText(),QStringLiteral("#travel"));
            UISE_TEST_REQUIRE(queries.size()>before);
            UISE_TEST_CHECK_EQUAL_QSTR(queries.back(),QStringLiteral("#travel"));
            UISE_TEST_CHECK(!browser->completion()->isOpen());

            // taken away: nothing is asked
            browser->setCompletionCb(nullptr);
            edit->clear();
            typeText(edit,QStringLiteral("#tr"));
            UISE_TEST_CHECK(!browser->completion()->isOpen());
        }
    );
}

//--------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE_END()
