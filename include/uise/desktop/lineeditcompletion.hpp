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

/** @file uise/desktop/lineeditcompletion.hpp
*
*  Declares LineEditCompletion: suggestions for the word under the caret of a QLineEdit that starts with a trigger
*  character ("#" for a hashtag), picked from a list that opens under the line.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_LINEEDITCOMPLETION_HPP
#define UISE_DESKTOP_LINEEDITCOMPLETION_HPP

#include <functional>

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <uise/desktop/uisedesktop.hpp>

class QLineEdit;
class QListWidget;
class QTimer;

namespace uise {

/**
 * @brief Completion of the word being typed in a line, from a source the owner provides.
 *
 * The WORD is the trigger character and the letters, digits and underscores that follow it up to the caret ("#tr"), where the
 * trigger is not glued to a letter or a digit before it ("abc#tr" has none). After a short pause in typing the source is asked for
 * the candidates of the word; its answer (possibly later, possibly never) fills a list that opens under the line. The list never
 * takes the keyboard focus: the line keeps it, and while the list is open the line's keys are taken -- Up and Down move the
 * selection, Enter and Tab pick the selected candidate, Escape closes the list and is NOT seen by anything else (so a page that
 * closes on Escape stays open), every other key is the line's own. A click on a row picks it.
 *
 * A pick replaces the whole word with the candidate, puts the caret after it (and a space, when the word was the end of the text),
 * closes the list and emits accepted(). The text is changed with setText(), which emits no textEdited(): it never asks again.
 *
 * Nothing is asked and nothing is shown when no source is set. An answer that is no longer wanted (the word changed, the list was
 * closed) is dropped. A single candidate that is the word itself is not worth a list and is not shown, so that Enter keeps its
 * own meaning in a finished word.
 *
 * Style (QSS): the list is a QListWidget named "completionList", a child of the widget given to the constructor.
 */
class UISE_DESKTOP_EXPORT LineEditCompletion : public QObject
{
    Q_OBJECT

    public:

        //! Gives the candidates of a word.
        using AnswerCb=std::function<void (const QStringList& candidates)>;

        //! Asks for the candidates of \a word (the trigger included, as typed up to the caret: "#tr", or "#" alone); calls \a answer
        //! with them, at once or later. Candidates are whole words, the trigger included ("#travel").
        using SourceCb=std::function<void (const QString& word, AnswerCb answer)>;

        //! The most rows the list shows; a source may give more, the rest is not offered.
        constexpr static const int MaxRows=10;

        //! Pause after the last keystroke before the source is asked.
        constexpr static const int DefaultDelayMs=150;

        /**
         * @param edit The line. Must outlive this object or be its parent.
         * @param listParent The widget the list is placed in, under \a edit, as wide as it. Must be an ancestor of \a edit.
         * @param trigger The character the word starts with.
         */
        LineEditCompletion(QLineEdit* edit, QWidget* listParent, QChar trigger=QChar('#'));

        ~LineEditCompletion() override;

        LineEditCompletion(const LineEditCompletion&)=delete;
        LineEditCompletion(LineEditCompletion&&)=delete;
        LineEditCompletion& operator=(const LineEditCompletion&)=delete;
        LineEditCompletion& operator=(LineEditCompletion&&)=delete;

        //! Where the candidates come from; null stops the completion and closes the list.
        void setSource(SourceCb source);

        //! Pause before the source is asked; 0 asks at once, in the call of the edit's signal.
        void setDelayMs(int delay) noexcept;

        //! Whether the list is open.
        bool isOpen() const;

        //! The candidates in the list, in its order; empty when it is closed.
        QStringList candidates() const;

        //! The row selected in the list; -1 when it is closed.
        int currentRow() const;

        //! Closes the list and drops what is on its way.
        void close();

        //! Picks the selected candidate as a key would. False when the list is closed.
        bool acceptCurrent();

        //! The word under the caret: where it starts in the text (-1: none) and what is typed of it (the trigger included).
        struct Word
        {
            int start=-1;
            QString text;
            int end=-1;     //!< where the word ends in the text, the part after the caret included
        };

        Word wordAtCaret() const;

    signals:

        //! A candidate was picked and is in the line.
        void accepted(const QString& candidate);

    protected:

        bool eventFilter(QObject* watched, QEvent* event) override;

    private:

        void onTextEdited();
        void onCursorPositionChanged();
        void ask();
        void show(const QStringList& candidates, const QString& forWord);
        void place();
        void moveSelection(int delta);
        void accept(int row);

        QPointer<QLineEdit> m_edit;
        QPointer<QWidget> m_listParent;
        QChar m_trigger;
        SourceCb m_source;
        int m_delayMs;

        QTimer* m_timer;
        QPointer<QListWidget> m_list;

        //! Counts the questions and closings: an answer of an older one is dropped.
        quint64 m_generation=0;
        bool m_open=false;
        //! Where the word the open list was made for starts: typing goes on in the same word, a caret that leaves it closes the list.
        int m_wordStart=-1;
};

}

#endif // UISE_DESKTOP_LINEEDITCOMPLETION_HPP
