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

/** @file uise/desktop/messageeditortoolbar.hpp
*
*  Declares MessageEditorToolbar.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_MESSAGEEDITORTOOLBAR_HPP
#define UISE_DESKTOP_MESSAGEEDITORTOOLBAR_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/messageeditingmode.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class IconTextButton;
class DropdownMenu;
class MessageEditorToolbar_p;

/**
 * @brief Addressable buttons of MessageEditorToolbar, for setButtonVisible()/setButtonEnabled()/
 *  button().
 *
 * Declared with no explicit values -- a dense 0..N-1 range used as an array index (see
 * MessageEditorToolbar_p::ButtonCount) -- so a new entry MUST be appended at the end, never
 * inserted: inserting would renumber every value after it. SpellCheck (task-spellcheck.md) is
 * appended after Close for that reason; its LAYOUT position (built beside Mention, in the ctor)
 * is unrelated to this enum's declaration order.
 */
enum class MessageEditorToolbarButton
{
    Mode,
    Undo,
    Redo,
    Bold,
    Italic,
    Underline,
    Strikethrough,
    InlineCode,
    Heading,
    BulletList,
    NumberedList,
    IndentIncrease,
    IndentDecrease,
    Blockquote,
    CodeBlock,
    Table,
    HorizontalRule,
    Link,
    RemoveLink,
    Mention,
    ClearFormatting,
    Close,
    SpellCheck
};

/**
 * @brief Row/column edits applied to the table the caret is already inside.
 *
 * Why these exist alongside the fixed insert presets: a table's size does not have to be known
 * before it is created. Growing one in place covers every dimension the presets do not, without
 * a size dialog and without the user having to count rows up front.
 */
enum class MessageEditorTableAction
{
    InsertRowAbove,
    InsertRowBelow,
    InsertColumnLeft,
    InsertColumnRight,
    RemoveRow,
    RemoveColumn,
    RemoveTable
};

/**
 * @brief Live formatting of whatever sits under the editor's caret, pushed into the toolbar so
 *  its checkable buttons reflect the DOCUMENT rather than their own click history.
 *
 * The distinction matters: IconTextButton::click() emits clicked() and then unconditionally
 * toggle()s, so a button's own checked state is never authoritative on its own -- it is written
 * only by setFormatState(); every clicked() handler computes the value it requests from THIS
 * struct instead (same trap and same fix as HTreeTabBarItem's close button,
 * src/htreetabbar.cpp:125-139).
 */
struct MessageEditorFormatState
{
    bool bold=false;
    bool italic=false;
    bool underline=false;
    bool strikeOut=false;
    bool inlineCode=false;
    bool bulletList=false;
    bool numberedList=false;
    bool blockquote=false;
    bool codeBlock=false;

    //! Caret is inside an ORDINARY hyperlink -- a MENTION anchor is excluded (Stage 6, see
    //! insideMention below), so the Remove-link button/row this drives never offers to unlink a
    //! mention, and "Edit link" never opens the hyperlink dialog on one.
    bool insideLink=false;

    //! Caret is inside a mention anchor (a `whitem-mention:` href). Mutually exclusive with
    //! insideLink above. Not reflected on any toolbar button of its own -- a mention has no
    //! "remove" action to offer; Backspace/Delete already delete it whole (see the atomicity
    //! guard in EnhancedTextEdit::keyPressEvent()). Exists so MessageEditor's own gates
    //! (canInsertMentionAtCursor(), the context-menu row's isEnabled) can tell a mention and an
    //! ordinary link apart.
    bool insideMention=false;

    //! Caret is inside a QTextTable -- gates the row/column edits in the table drop-down, which
    //! are meaningless anywhere else.
    bool insideTable=false;

    //! 0 == normal (non-heading) text.
    int headingLevel=0;

    //! task-spellcheck.md. Editor-WIDE state, not caret state, unlike every other field here --
    //! it rides in this struct only so the Check-spelling button can be driven by wireCheckable()
    //! like every other checkable button, i.e. from the state the EDITOR reports rather than from
    //! the button's own click history (see this struct's own doc comment above for that trap).
    bool spellCheckEnabled=false;
};

/**
 * @brief The formatting bar MessageEditor shows above its text edit while expanded
 *  (task-message-formatting-plan.md, Stage 5a).
 *
 * A dumb view: it never touches a QTextDocument itself. Every button emits a *Requested() signal;
 * the host (MessageEditor) applies the edit to the document and pushes the resulting state back
 * via setFormatState()/setMode().
 *
 * Layout, left to right, per task-message-formatting-plan.md §5: Close -- a mode switcher
 * (Wysiwyg/Markdown/Plaintext) -- separator -- Undo/Redo -- separator --
 * Bold/Italic/Underline/Strikethrough/Inline code --
 * separator -- Heading dropdown, Bulleted/Numbered list, Blockquote, Code block, Table --
 * separator -- Link/Remove link (Stage 5b, hidden by default), Mention (Stage 6, hidden by
 * default), Clear formatting -- stretch.
 *
 * Close LEADS on every platform, rather than taking the platform-dependent placement the
 * original brief asked for: it lines up directly above the editor's own bottom-left expand
 * button, the control that opened this toolbar, so the bar is opened and closed from the same
 * column of blank padding beside the text edit.
 */
class UISE_DESKTOP_EXPORT MessageEditorToolbar : public Frame
{
    Q_OBJECT

    public:

        explicit MessageEditorToolbar(QWidget* parent=nullptr);

        ~MessageEditorToolbar();
        MessageEditorToolbar(const MessageEditorToolbar&)=delete;
        MessageEditorToolbar(MessageEditorToolbar&&)=delete;
        MessageEditorToolbar& operator=(const MessageEditorToolbar&)=delete;
        MessageEditorToolbar& operator=(MessageEditorToolbar&&)=delete;

        void setButtonVisible(MessageEditorToolbarButton button, bool visible);
        bool isButtonVisible(MessageEditorToolbarButton button) const;

        void setButtonEnabled(MessageEditorToolbarButton button, bool enable);
        bool isButtonEnabled(MessageEditorToolbarButton button) const;

        //! Live widget for a button, for host-side QSS/tooltip tweaks. Never nullptr.
        IconTextButton* button(MessageEditorToolbarButton button) const;

        //! Reflect the caret's current formatting on the checkable buttons. Emits none of this
        //! class's own *Requested() signals -- guarded internally -- so it is safe to call
        //! straight from a cursorPositionChanged/currentCharFormatChanged handler.
        void setFormatState(const MessageEditorFormatState& state);
        const MessageEditorFormatState& formatState() const noexcept;

        //! Reflect (never change on its own) the editor's mode in the mode drop-down. Guarded
        //! the same way as setFormatState().
        void setMode(MessageEditingMode mode);
        MessageEditingMode mode() const noexcept;

        //! Enable/disable the whole formatting half of the bar in one call -- MessageEditor
        //! greys it out outside MessageEditingMode::Wysiwyg (Stage 5a decision: formatting is
        //! WYSIWYG-only). The mode switcher and Close stay live regardless.
        void setFormattingEnabled(bool enable);
        bool isFormattingEnabled() const noexcept;

        DropdownMenu* modeMenu() const;
        DropdownMenu* headingMenu() const;
        DropdownMenu* tableMenu() const;

    signals:

        void modeRequested(UISE_DESKTOP_NAMESPACE::MessageEditingMode mode);

        //! Undo/redo are document EDITS, not formatting, so they stay live in every
        //! MessageEditingMode -- setFormattingEnabled() does not touch them. Their enabled state
        //! is driven by the host instead, from the document's own undo/redo availability.
        void undoRequested();
        void redoRequested();

        //! `enable` is the state the DOCUMENT should end up in, derived from the last
        //! setFormatState() call -- NOT from the button's own checked state. See
        //! MessageEditorFormatState's own doc comment.
        void boldRequested(bool enable);
        void italicRequested(bool enable);
        void underlineRequested(bool enable);
        void strikethroughRequested(bool enable);
        void inlineCodeRequested(bool enable);
        void bulletListRequested(bool enable);
        void numberedListRequested(bool enable);

        //! Nesting level of the list item (or the indent of a plain block) under the caret.
        //! Plain clicks, not toggles -- there is no "on/off" state for a level.
        void indentIncreaseRequested();
        void indentDecreaseRequested();
        void blockquoteRequested(bool enable);

        //! 0 == normal (non-heading) text.
        void headingRequested(int level);

        void codeBlockRequested();

        //! Insert a thematic break at the caret. No argument and no state: unlike the character
        //! and block toggles, a rule is inserted rather than switched on.
        void horizontalRuleRequested();
        void tableRequested(int rows, int columns);

        //! Row/column edit on the table the caret is already in. Emitted only from rows the
        //! toolbar keeps disabled unless MessageEditorFormatState::insideTable says otherwise.
        void tableActionRequested(UISE_DESKTOP_NAMESPACE::MessageEditorTableAction action);
        void clearFormattingRequested();

        //! Stage 5b. Emitted like every other button, but Link/RemoveLink default to hidden
        //! (see setButtonVisible()) and MessageEditor leaves these two signals unconnected until
        //! that stage lands -- no toolbar API change is needed to wire them up later.
        void linkRequested();
        void removeLinkRequested();

        //! Stage 6, same arrangement as linkRequested()/removeLinkRequested() above.
        void mentionRequested();

        //! task-spellcheck.md. `enable` is the state the editor should end up in -- same
        //! wireCheckable() contract as boldRequested() and friends, NOT the button's own checked
        //! state. Unlike Bold/Italic/etc. this reflects EDITOR-wide state, not caret state -- see
        //! MessageEditorFormatState::spellCheckEnabled.
        void spellCheckRequested(bool enable);

        //! MessageEditor reacts to this by calling setExpanded(false).
        void closeRequested();

    private:

        //! Wire a checkable button's clicked()/toggled() pair against one bool field of the
        //! live MessageEditorFormatState, emitting the given *Requested(bool) signal. See
        //! MessageEditorFormatState's own doc comment for why both connections are needed.
        void wireCheckable(
            IconTextButton* btn,
            bool MessageEditorFormatState::* field,
            void (MessageEditorToolbar::*requestedSignal)(bool)
        );

        std::unique_ptr<MessageEditorToolbar_p> pimpl;
};

}

#endif // UISE_DESKTOP_MESSAGEEDITORTOOLBAR_HPP
