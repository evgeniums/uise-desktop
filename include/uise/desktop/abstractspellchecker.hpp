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

/** @file uise/desktop/abstractspellchecker.hpp
*
*  Declares AbstractSpellChecker.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTSPELLCHECKER_HPP
#define UISE_DESKTOP_ABSTRACTSPELLCHECKER_HPP

#include <QObject>
#include <QString>
#include <QStringList>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Answer AbstractSpellChecker::check() returns for one word.
 */
enum class SpellCheckVerdict
{
    Correct,      //!< Some loaded dictionary accepts the word.
    Misspelled,   //!< NO loaded dictionary accepts it -- the only state that draws a squiggle.
    Unknown       //!< No answer yet. Paints nothing -- see AbstractSpellChecker::dictionaryChanged().
};

/**
 * @brief The narrow seam task-spellcheck.md's "syntax highlight support" section asks for: a
 *  per-word verdict hook the editor's highlighter can call.
 *
 * uise ships no dictionary and never will -- same host-owns-the-data arrangement as
 * AbstractMessageEditor::mentionRequested()'s user directory. A concrete implementation (hatn
 * base's hunspell-backed one, per this task's own project split) is supplied by the host via
 * EnhancedTextEdit::setSpellChecker() / MessageEditor::setSpellChecker(), and one checker is
 * normally shared by every editor in the application, since a dictionary set is expensive to
 * build.
 *
 * check() is called once per word per rehighlighted block, i.e. on every keystroke, so it MUST be
 * cheap and synchronous. A backend that cannot answer without I/O or without a dictionary that has
 * not finished loading returns SpellCheckVerdict::Unknown and emits dictionaryChanged() once it
 * can -- the tri-state verdict exists specifically for a whitemdesktop-side adapter that answers
 * from a local cache on the GUI thread and enqueues a miss onto a worker thread holding the actual
 * hatn checker, which is a different layer entirely (Qt-facing here, Qt-free in hatn) and does not
 * exist in this codebase yet.
 */
class UISE_DESKTOP_EXPORT AbstractSpellChecker : public QObject
{
    Q_OBJECT

    public:

        using QObject::QObject;

        ~AbstractSpellChecker() override;

        AbstractSpellChecker(const AbstractSpellChecker&) =delete;
        AbstractSpellChecker(AbstractSpellChecker&&) =delete;
        AbstractSpellChecker& operator=(const AbstractSpellChecker&) =delete;
        AbstractSpellChecker& operator=(AbstractSpellChecker&&) =delete;

        //! Whether this checker has anything to check WITH right now. False means the editor
        //! skips the whole spell pass, so an editor wired to a checker whose dictionaries have
        //! not loaded yet behaves exactly like one with no checker at all.
        virtual bool isReady() const =0;

        //! MUST be cheap, synchronous and non-blocking -- see this class's own doc comment.
        virtual SpellCheckVerdict check(const QString& word) const =0;

        //! Replacements, best first, at most maxCount. Called ONCE per context-menu open (one
        //! deliberate user gesture), never from the highlighter -- so, unlike check(), this may
        //! take as long as it needs to.
        virtual QStringList suggestions(const QString& word, int maxCount) const =0;

        //! Whether the "Add to dictionary" context-menu row is offered at all. False by default:
        //! a checker with no writable personal dictionary should not advertise one.
        virtual bool canAddToDictionary() const
        {
            return false;
        }

        //! Add `word` to the checker's personal dictionary. A no-op base implementation, so a
        //! checker that returns false from canAddToDictionary() need not override this too.
        virtual void addToDictionary(const QString& word)
        {
            Q_UNUSED(word)
        }

        //! Session-only suppression of `word` -- "stop underlining this", offered whenever a
        //! checker is set, regardless of canAddToDictionary().
        virtual void ignoreWord(const QString& word)
        {
            Q_UNUSED(word)
        }

    signals:

        /**
         * @brief Anything that could change a previous check() verdict has changed -- a
         *  dictionary finished loading, the active language set changed, or a word was added or
         *  ignored.
         *
         * Every EnhancedTextEdit this checker is attached to drops its verdict cache and
         * re-highlights (debounced) in response. Declared here rather than on the highlighter:
         * MessageEditorHighlighter declares no signals and needs no moc pass of its own, so the
         * async leg of this seam lives on the checker instead, and EnhancedTextEdit -- already a
         * QObject -- is what connects it.
         */
        void dictionaryChanged();
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACTSPELLCHECKER_HPP
