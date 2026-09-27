/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/autoresizingtextedit.hpp
*
*  Declares AutoResizingTextEdit.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_AUTORESIZINGTEXTEDIT_HPP
#define UISE_DESKTOP_AUTORESIZINGTEXTEDIT_HPP

#include <QTextEdit>

#include <uise/desktop/uisedesktop.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

/**
 * @brief Small plain-text editor that grows with its content, up to a configurable line cap.
 *
 * Meant for compact editable-value fields (see EditableLabelTextEdit) where a full-blown
 * message composer (EnhancedTextEdit, in messageeditor.hpp) is far more machinery than the field
 * needs -- no spellcheck, mentions, rich formatting or expanded mode, just a box that starts at
 * one line and grows as the user types, up to maxLines(), then scrolls.
 *
 * Return/Enter inserts a newline by default, like a plain QTextEdit. Set
 * setReturnInsertsNewLine(false) to make a plain Return/Enter emit returnPressed() instead (e.g.
 * to submit a form); Shift+Return/Enter always inserts a newline regardless of that setting.
 */
class UISE_DESKTOP_EXPORT AutoResizingTextEdit : public QTextEdit
{
    Q_OBJECT

    //! QSS: qproperty-maxLines: 3;
    Q_PROPERTY(int maxLines READ maxLines WRITE setMaxLines)

    public:

        //! Number of lines the editor grows to before it starts scrolling instead.
        constexpr static const int DefaultMaxLines=4;

        explicit AutoResizingTextEdit(QWidget* parent=nullptr);

        QSize sizeHint() const override;

        /**
         * @brief The minimum height tracks the content instead of Qt's own ~90px floor.
         *
         * QTextEdit::minimumSizeHint() reserves room for several lines plus scrollbars, so
         * without this override a one- or two-line editor would stay pinned at that floor and
         * never visibly shrink to fit its content (see EnhancedTextEdit::minimumSizeHint() in
         * messageeditor.hpp/cpp, which documents the same Qt behaviour in detail).
         *
         * Only the height is overridden -- the width stays Qt's own value, since sizeHint()
         * reports the CURRENT width and returning it wholesale would ratchet the minimum width up
         * and never let it shrink again.
         */
        QSize minimumSizeHint() const override;

        /**
         * @brief Set the number of lines the editor grows to before it starts scrolling.
         * @param lines Line cap, clamped to at least 1.
         */
        void setMaxLines(int lines);

        int maxLines() const noexcept
        {
            return m_maxLines;
        }

        /**
         * @brief Set whether a plain Return/Enter inserts a newline (the default) or emits
         *        returnPressed() instead.
         *
         * Shift+Return/Enter always inserts a newline, regardless of this setting -- so a host
         * that wants Return to submit still leaves a way to add a line break.
         */
        void setReturnInsertsNewLine(bool enable) noexcept
        {
            m_returnInsertsNewLine=enable;
        }

        bool isReturnInsertingNewLine() const noexcept
        {
            return m_returnInsertsNewLine;
        }

    signals:

        //! Emitted for a plain Return/Enter when isReturnInsertingNewLine() is false.
        void returnPressed();

    protected:

        void keyPressEvent(QKeyEvent* event) override;
        void changeEvent(QEvent* event) override;

    private:

        void updateSize();
        int verticalChrome() const;
        int contentHeight() const;
        int maxContentHeight() const;

        int m_maxLines;
        bool m_returnInsertsNewLine;
};

}

#endif // UISE_DESKTOP_AUTORESIZINGTEXTEDIT_HPP
