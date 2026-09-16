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

/** @file uise/desktop/messageeditor.hpp
*
*  Declares MessageEditor.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_MESSAGEEDITOR_HPP
#define UISE_DESKTOP_MESSAGEEDITOR_HPP

#include <QTextEdit>
#include <QPointer>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractspellchecker.hpp>
#include <uise/desktop/abstractmessageeditor.hpp>

class QTimer;
class QHideEvent;
class QBoxLayout;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class IconTextButton;
class MessageEditorToolbar;
class AbstractReactionIconPack;
class FloatingEmojiGalleryDialog;

//! Paints blockquotes and code blocks without writing to the document -- defined privately in
//! messageeditor.cpp, since nothing outside the editor has any reason to construct one. See
//! EnhancedTextEdit::setBlockquoteColor() for why it is a highlighter and not a char format.
class MessageEditorHighlighter;
struct MessageEditorFormatState;
enum class MessageEditorTableAction;

class UISE_DESKTOP_EXPORT EnhancedTextEdit : public QTextEdit
{
    Q_OBJECT

    //! QSS: qproperty-maxHeight: 400; -- the ceiling this editor never grows past, in BOTH
    //! modes: auto-resizing stops here and starts scrolling instead, and expanded mode jumps
    //! straight to it. Also the FLOOR under maxHeightPercent below, so a percentage cannot make
    //! the editor uselessly short on a small window.
    //!
    //! Consulted only as a fallback: effectiveMaxHeight() prefers whatever QSS "max-height" a
    //! host stylesheet already set on this widget (Qt turns that into a real
    //! QWidget::maximumHeight() via QStyleSheetStyle::setGeometry(), re-applied on every
    //! repolish), so a host that already caps this editor in its own stylesheet -- as
    //! whitemdesktop's chatpage.qss does, at 300px -- keeps exactly the cap it declared.
    //! Deliberately NOT shipped as a "qproperty-maxHeight" rule in this library's own
    //! messageeditor.qss -- see that file's header comment.
    Q_PROPERTY(int maxHeight READ maxHeight WRITE setMaxHeight)

    //! QSS: qproperty-maxHeightPercent: 40; -- express the ceiling as a percentage of the
    //! reference widget's height (the top-level window unless
    //! setMaxHeightReferenceWidget() says otherwise) instead of a fixed number of pixels, so a
    //! composer on a tall window may grow taller than one on a short window. 0 (the default)
    //! disables it and leaves maxHeight the only ceiling.
    //!
    //! The result is never below maxHeight, which is what keeps a small window usable.
    Q_PROPERTY(int maxHeightPercent READ maxHeightPercent WRITE setMaxHeightPercent)

    //! QSS: qproperty-maxLength: 100000; -- ceiling (Unicode code points) a single paste/drop may
    //! bring the document to; 0 (the default) means unlimited. Enforced in insertFromMimeData()
    //! only -- typing is not gated, see that override's own doc comment for why. task-message-
    //! text-length-limits.md's paste/insert gate; the value a host actually wires in is
    //! ChatSettings::maxTotalTextLength(), forwarded through MessageEditor::setMaxLength().
    Q_PROPERTY(int maxLength READ maxLength WRITE setMaxLength)

    /**
     * QSS: qproperty-blockquoteColor: #666666; -- colour of text inside a blockquote, matching the
     * `blockquote` colour in the per-theme messagetext.css so a quote reads the same in the
     * composer and in the chat bubble it becomes.
     *
     * An INVALID colour (the default, and what a host that ships no stylesheet gets) leaves quoted
     * text in the ordinary text colour; the indent alone then marks the quote.
     *
     * This one IS shipped as a `qproperty-` rule in the library's light/dark messageeditor.qss,
     * unlike maxHeight above, and deliberately so: it is a THEME colour, so being re-applied on
     * every repolish is the whole point -- that is what makes a quote recolour when the user
     * switches theme instead of staying at whatever the palette was when they typed it.
     */
    Q_PROPERTY(QColor blockquoteColor READ blockquoteColor WRITE setBlockquoteColor)

    /**
     * QSS: qproperty-codeBlockColor: #444444; -- colour of text inside a fenced code block.
     *
     * A code block is shown in a fixed-pitch font plus this colour, and that display IS the only
     * thing marking it: the "```" delimiters never exist in the document. Qt's markdown importer
     * consumes them into block properties on the way in, and the toolbar button sets the same
     * properties directly -- so a code block with no visible difference reads as ordinary prose.
     *
     * Like blockquoteColor this is applied through the highlighter, never written into the
     * document, so it costs no undo step and leaks into no export. An invalid colour keeps the
     * ordinary text colour, leaving the fixed-pitch font as the only marker.
     */
    Q_PROPERTY(QColor codeBlockColor READ codeBlockColor WRITE setCodeBlockColor)

    /**
     * QSS: qproperty-linkColor: #1A6FD4; -- colour of a hyperlink's text, deliberately named and
     * valued to match ChatMessageTextBrowser's own linkColor/linkUnderline properties, so a link
     * reads the same in the composer and in the chat bubble it becomes (the same rule
     * blockquoteColor follows against messagetext.css).
     *
     * This is not cosmetic polish: an anchor renders as ORDINARY TEXT without it. Measured, a
     * document with an anchor and one without paint pixel-for-pixel identically -- so an inserted
     * link is invisible AS a link until something paints it. Qt's own markdown importer hides
     * that by baking foreground=#0000ff onto every anchor it reads, which is why a freshly
     * inserted link looked plain while the same link looked blue after a round trip through
     * Markdown mode. That baked colour is frozen at the theme it was imported in and leaks into
     * toHtml(), so it is stripped on the way in rather than relied on.
     *
     * Applied through the highlighter like blockquoteColor: no document write, no undo step, no
     * export leakage, and a theme switch costs one rehighlight(). An INVALID colour (the default)
     * leaves link text in the ordinary text colour.
     */
    Q_PROPERTY(QColor linkColor READ linkColor WRITE setLinkColor)

    //! QSS: qproperty-linkUnderline: false; -- see linkColor. Set either way rather than only
    //! when true, so false also suppresses an underline an imported document carried.
    Q_PROPERTY(bool linkUnderline READ linkUnderline WRITE setLinkUnderline)

    /**
     * QSS: qproperty-mentionColor: #7A3FBF; -- colour of a MENTION anchor's text, task-message-
     * formatting-plan.md Stage 6, deliberately distinct from linkColor above and kept in step
     * with ChatMessageTextBrowser::mentionColor, so a mention reads the same in the composer and
     * in the bubble it becomes (the same rule linkColor/blockquoteColor already follow).
     *
     * Applied by the same highlighter, on the same display-only terms: no document write, no
     * undo step, no export leakage. An INVALID colour (the default) falls back to linkColor -- a
     * mention IS an anchor, so the blanket link colour is the right fallback, and it is exactly
     * what the viewer does too (its `a[href^="whitem-mention:"]` rule is emitted only when its
     * own mentionColor is valid, leaving the blanket `a` rule in charge otherwise).
     *
     * There is deliberately no separate "mentionUnderline" property: linkUnderline is one
     * app-wide decision about whether anchors underline at all, and the hover-underline machinery
     * on the viewer side already treats every anchor identically.
     */
    Q_PROPERTY(QColor mentionColor READ mentionColor WRITE setMentionColor)

    /**
     * QSS: qproperty-spellCheckUnderlineColor: #FF3B30; -- pen colour of the squiggle drawn under
     * a word no loaded dictionary accepts (task-spellcheck.md).
     *
     * Applied through the highlighter like blockquoteColor/linkColor/mentionColor above, on the
     * same display-only terms: no document write, no undo step, no export leakage, and a theme
     * switch costs one rehighlight(). Unlike linkColor, an INVALID colour does NOT disable the
     * pass -- the squiggle's SHAPE (QTextCharFormat::SpellCheckUnderline, the platform's OWN
     * spelling-underline style) is the marker and this colour is decoration on top of it, so a
     * host that ships no stylesheet still gets a working spellchecker, in the platform's own
     * default underline colour.
     */
    Q_PROPERTY(QColor spellCheckUnderlineColor READ spellCheckUnderlineColor WRITE setSpellCheckUnderlineColor)

    public:

        //! Ceiling used by effectiveMaxHeight() when no QSS "max-height" is in effect, and the
        //! floor under maxHeightPercent.
        constexpr static const int DefaultMaxHeight=300;

        //! Pixels one list/indent level is worth. Half Qt's own 40px default, which puts even a
        //! single-level list uncomfortably far from the margin in a chat composer -- see
        //! MessageEditor::setListIndentWidth(). Applied in this widget's constructor, and it
        //! survives every content reset (setMarkdown/setHtml/setPlainText/clear all leave
        //! QTextDocument::indentWidth() alone), so it never needs re-applying on load.
        constexpr static const qreal DefaultListIndentWidth=20.0;

        //! Width of a literal tab character, counted in space characters of the current font.
        //!
        //! Qt's own default is a flat 80px regardless of font, which at the composer's ~3.4px
        //! space width is over TWENTY spaces -- one pasted tab pushes the rest of the line off
        //! the visible area. Nothing the user types produces a tab any more (Tab is an indent
        //! gesture, see indentStepRequested()), but pasted and loaded text still can.
        constexpr static const int DefaultTabStopSpaces=4;

        explicit EnhancedTextEdit(QWidget *parent = nullptr);

        QSize sizeHint() const override;

        /**
         * @brief While auto-resizing, the minimum HEIGHT tracks the document instead of Qt's own
         *  fixed floor.
         *
         * QTextEdit::minimumSizeHint() is ~90px (QAbstractScrollArea reserves room for a couple
         * of lines plus scrollbars), and a layout can never size a widget below it -- so an
         * auto-resizing composer stays frozen at 90px until its content passes that mark, which
         * for a default font is the first FIVE lines. Measured: document 23/38/53/68/83px all
         * render at 90px, and only 98px content finally moves it. That makes "auto-resize" look
         * broken exactly where it matters most, at one or two lines.
         *
         * Only the height is overridden: sizeHint() reports the CURRENT width, so returning it
         * wholesale would pin the minimum width to whatever the widget happens to be, ratcheting
         * it wider and never letting it shrink again.
         *
         * A host that wants the old floor back sets a plain "min-height" in its own stylesheet:
         * an explicit QWidget::minimumHeight() overrides this hint outright (qSmartMinSize()).
         */
        QSize minimumSizeHint() const override;

        void setAutoResizingEnabled(bool enable);
        bool isAutoResizingEnabled() const noexcept
        {
            return m_autoResize;
        }

        //! See AbstractMessageEditor::setExpanded() -- MessageEditor forwards its expanded state
        //! down to this widget via this setter. While enabled, sizeHint() clamps to
        //! effectiveMaxHeight() instead of growing with the document, and the vertical scrollbar
        //! switches from ScrollBarAlwaysOff to ScrollBarAsNeeded so overflowing content stays
        //! reachable.
        void setExpandedEnabled(bool enable);
        bool isExpandedEnabled() const noexcept
        {
            return m_expanded;
        }

        void setMaxHeight(int height) noexcept
        {
            m_maxHeight=height;
        }

        int maxHeight() const noexcept
        {
            return m_maxHeight;
        }

        void setMaxHeightPercent(int percent) noexcept
        {
            m_maxHeightPercent=percent;
        }

        int maxHeightPercent() const noexcept
        {
            return m_maxHeightPercent;
        }

        void setMaxLength(int length) noexcept
        {
            m_maxLength=length;
        }

        int maxLength() const noexcept
        {
            return m_maxLength;
        }

        /**
         * @brief Widget whose height maxHeightPercent is a percentage OF. Defaults to the
         *  top-level window().
         *
         * Deliberately not the immediate parent: a composer's parent is typically sized BY the
         * editor, so making the editor's ceiling a fraction of that parent's height would be a
         * feedback loop -- the editor grows, its parent grows, the ceiling rises, the editor
         * grows again. A reference that does not depend on the editor's own height (the window,
         * or a page/panel the host names here) is the whole point.
         */
        void setMaxHeightReferenceWidget(QWidget* widget);
        QWidget* maxHeightReferenceWidget() const;

        /**
         * @brief Colour of text inside a blockquote -- see the blockquoteColor property.
         *
         * Applied through a QSyntaxHighlighter, NOT by writing char formats into the document, and
         * that distinction is the whole reason this is safe. Measured, all four:
         *
         *  - the document's own char formats are never touched, so nothing is baked in;
         *  - toMarkdown() and toHtml() are byte-identical with and without it, so no colour ever
         *    leaks into the message that gets sent;
         *  - not one undo step is added (a real mergeCharFormat() over the block appends one, and
         *    would put an invisible entry between the user's edits);
         *  - re-running it on a theme switch costs a rehighlight() and no document edit at all.
         *
         * That last point is what makes a theme-following colour possible here. Baking the colour
         * in would freeze it at whatever the theme was when the quote was typed -- the same trap
         * that made inserted table borders vanish on a theme switch earlier in this stage.
         *
         * Note that a QTextDocument supports ONE QSyntaxHighlighter: a second one attached to this
         * editor's document would fight this one over the same layout formats. Anything else this
         * editor needs to highlight later belongs in the same highlighter.
         */
        void setBlockquoteColor(const QColor& color);
        QColor blockquoteColor() const noexcept
        {
            return m_blockquoteColor;
        }

        //! See the codeBlockColor property. Applied by the same highlighter, on the same terms.
        void setCodeBlockColor(const QColor& color);
        QColor codeBlockColor() const noexcept
        {
            return m_codeBlockColor;
        }

        //! See the linkColor property. Applied by the same highlighter, on the same terms.
        void setLinkColor(const QColor& color);
        QColor linkColor() const noexcept
        {
            return m_linkColor;
        }

        //! See the linkUnderline property.
        void setLinkUnderline(bool enable);
        bool linkUnderline() const noexcept
        {
            return m_linkUnderline;
        }

        //! See the mentionColor property. Applied by the same highlighter, on the same terms.
        void setMentionColor(const QColor& color);
        QColor mentionColor() const noexcept
        {
            return m_mentionColor;
        }

        //! See the spellCheckUnderlineColor property. Applied by the same highlighter, on the
        //! same terms.
        void setSpellCheckUnderlineColor(const QColor& color);
        QColor spellCheckUnderlineColor() const noexcept
        {
            return m_spellCheckUnderlineColor;
        }

        /**
         * @brief Attach a spell checker (task-spellcheck.md). NOT owned -- the HOST owns it, and
         *  one checker is normally shared by every editor in the application, since a dictionary
         *  set is expensive to build.
         *
         * The editor ships no dictionary and never will -- same host-owns-the-data arrangement as
         * mentionRequested()'s user directory. Passing nullptr detaches and clears the verdict
         * cache. This widget connects the checker's AbstractSpellChecker::dictionaryChanged()
         * itself; re-attaching a different checker, or the current one being destroyed, is
         * handled here rather than left to the caller.
         */
        void setSpellChecker(AbstractSpellChecker* checker);
        AbstractSpellChecker* spellChecker() const noexcept
        {
            return m_spellChecker;
        }

        //! Toggle "check spelling as I type" on this widget. See
        //! AbstractMessageEditor::spellCheckEnabled -- MessageEditor forwards its own setting
        //! down to this widget via this setter.
        void setSpellCheckEnabled(bool enable);
        bool isSpellCheckEnabled() const noexcept
        {
            return m_spellCheckEnabled;
        }

        /**
         * @brief A spell-checkable word and where it sits in the document (task-spellcheck.md).
         *
         * Produced by the SAME tokenizer the highlighter's own pass uses (see spellTokens() in
         * messageeditor.cpp's anonymous namespace), so the context menu can never offer to fix a
         * word the highlighter would not have underlined, and vice versa.
         */
        struct SpellWord
        {
            bool isValid=false;

            //! Document position of the first character. -1 when !isValid.
            int position=-1;

            int length=0;
            QString text;
        };

        //! The spell-checkable word covering `documentPosition`, or an invalid SpellWord if that
        //! position is in whitespace, in a fenced code block, inside an anchor or an inline-code
        //! run, or in a token the tokenizer drops entirely (a URL, an "@handle", anything
        //! containing a digit, a camelCase/PascalCase identifier -- see spellTokens()).
        SpellWord spellWordAt(int documentPosition) const;

        //! spellWordAt(textCursor().position()).
        SpellWord spellWordAtCursor() const;

        /**
         * @brief Extend `cursor` to cover `word`.
         * @return false, `cursor` left untouched, for an invalid word.
         */
        bool selectSpellWord(QTextCursor& cursor, const SpellWord& word) const;

        /**
         * @brief The in-progress "@word" at the caret, if any -- task-message-formatting-plan.md,
         *  Stage 6. What mentionQueryChanged() reports, exposed for a host to query directly too
         *  (e.g. MessageEditor::onMentionButtonRequested() does, to fill
         *  AbstractMessageEditor::mentionRequested()'s `prefix` argument).
         *
         * Recomputed from scratch on every call and emits nothing, so it is safe to call from
         * anywhere.
         */
        struct MentionQuery
        {
            bool isActive=false;

            //! Document position of the '@' itself. -1 when !isActive.
            int position=-1;

            //! Text after the '@', up to the caret. Empty right after '@' is typed.
            QString prefix;
        };

        MentionQuery mentionQueryAtCursor() const;

        /**
         * @brief Extend `cursor` to cover the in-progress "@word" (the '@' included), so an
         *  insert REPLACES what the user typed rather than landing beside it.
         * @return false, `cursor` left untouched, if there is no candidate at the caret.
         */
        bool selectMentionQueryAtCursor(QTextCursor& cursor) const;

        /**
         * @brief Widen `cursor`'s selection outward so no mention run is left only PARTLY
         *  covered by it.
         * @return true if the selection was actually changed.
         *
         * The measured worst case this exists for: a selection starting inside a mention and
         * ending outside it, deleted, leaves the REMAINDER of the run as a smaller anchor
         * carrying the SAME href -- a live, clickable, wrong mention. Selecting the run whole
         * first turns every such gesture into the measured clean case (one edit, no residue).
         * Called by the Backspace/Delete/typing guard in keyPressEvent() and by
         * MessageEditor::cut().
         */
        bool snapSelectionToMentions(QTextCursor& cursor) const;

        /**
         * @brief The in-progress ":name" candidate ending right at the caret, if any -- what a
         *  typed closing ':' checks before being allowed to auto-replace into an emoji.
         *
         * Recomputed from scratch on every call and emits nothing, safe to call from anywhere --
         * same contract as mentionQueryAtCursor(), and deliberately modelled on it.
         */
        struct ShortcodeCandidate
        {
            bool isActive=false;

            //! Document position of the OPENING ':'. -1 when !isActive.
            int position=-1;

            //! Text strictly between the two colons, e.g. "star" for ":star:". Never empty when
            //! isActive -- see shortcodeCandidateAtCursor()'s own doc comment on why "::" is
            //! rejected rather than treated as a zero-length name.
            QString name;
        };

        /**
         * @brief Look immediately BEFORE the caret for "...:name" where the caret sits right
         *  after "name" and a closing ':' is ABOUT to be typed there -- i.e. this is called from
         *  the guard that handles the CLOSING colon keypress itself, before it is inserted.
         *
         * Charset for `name` is [A-Za-z0-9_+-], case-insensitive at lookup time (matches
         * AbstractReactionIconPack::findByShortcode()'s own case-folding) -- '+'/'-' so aliases
         * like "+1"/"-1" need no charset change later. Scans backwards from the caret within the
         * CURRENT BLOCK only, at most MaxShortcodeChars characters, stopping at the first
         * out-of-charset character. An inline emoji IMAGE is itself out-of-charset (its
         * placeholder is U+FFFC OBJECT REPLACEMENT CHARACTER), so the scan naturally stops there
         * with no special case -- which is exactly what makes ":star::fire:" work: after the
         * first replacement, the second opening ':' is preceded by the image, not by a colon.
         *
         * Rejected, isActive left false:
         *  - An empty name ("::") -- deliberately never triggers. ':: ' is the C++ scope
         *    operator, and this editor is used in developer chats.
         *  - The character before the opening ':' is itself ':' or in-charset ("a:star:" reads
         *    as "a:" followed by "star:", not as a shortcode "a:star" -- one test rejects both
         *    "foo::bar:"-style runs and this case at once).
         *  - No opening ':' found within MaxShortcodeChars -- a guard against a pathological run
         *    of charset characters with no colon at all.
         *  - cursor.hasSelection() -- a selection means the user is not mid-typing a token.
         *  - The same context gates insertEmoji()/normalizeImportedEmoji() already refuse:
         *    fixed-pitch (inline code) runs, a fenced code block, and inside a mention. Tables
         *    are NOT gated -- insertEmoji() allows them there.
         */
        ShortcodeCandidate shortcodeCandidateAtCursor() const;

        //! Turn ":shortcode:" auto-replace on/off at the WIDGET level -- pushed down from
        //! MessageEditor::applyEmojiShortcodeAutoReplace(), which is the only caller; a bare
        //! EnhancedTextEdit with nothing connected to emojiShortcodeTyped() simply never offers
        //! anything to replace with, so this alone does not make typing a colon do anything.
        //! Turning it OFF also clears any live revert/re-trigger-latch state, so a replacement
        //! armed while the feature was on does not keep reverting for one more Backspace after
        //! being switched off.
        void setEmojiShortcodeAutoReplaceEnabled(bool enable);

        bool isEmojiShortcodeAutoReplaceEnabled() const noexcept
        {
            return m_emojiShortcodeAutoReplaceEnabled;
        }

        /**
         * @brief Arm the ONE Backspace/Delete revert a just-completed shortcode replacement gets.
         * @param position Document position of the replacement's first character.
         * @param length Its length (an image is 1; Markdown mode's literal character(s) may be
         *  more -- measure with `textCursor().position()-position` after inserting, never assume
         *  1).
         * @param literal Exactly what a revert should restore, INCLUDING both colons.
         * @param format The char format in force BEFORE the replacement was inserted.
         *
         * Called by MessageEditor::onEmojiShortcodeTyped() -- see emojiShortcodeTyped()'s own doc
         * comment for why calling this SYNCHRONOUSLY, from within that signal's handler, is what
         * makes applyEmojiShortcodeGuard() consume the triggering ':' at all.
         *
         * Captures document()->revision() itself (right now, right after the caller's own edit
         * block closed) -- the caller does not supply it, since "now" is always the correct
         * value.
         */
        void armEmojiShortcodeRevert(int position, int length, const QString& literal,
                                     const QTextCharFormat& format);

        //! The ceiling actually in force: an already-set QSS/C++ QWidget::maximumHeight() if one
        //! is in effect, otherwise maxHeight() raised to maxHeightPercent() of the reference
        //! widget's height when that is larger.
        int effectiveMaxHeight() const;

        void setNewLineOnEnter(bool enable)
        {
            m_newLineOnEnter=enable;
        }

        bool isNewLineOnEnter() const noexcept
        {
            return m_newLineOnEnter;
        }

        /**
         * @brief Paste the clipboard's current content, image/file payloads included.
         *
         * Implemented as QTextEdit::paste() rather than e.g. insertPlainText(clipboard->text()):
         * QWidgetTextControl::paste() funnels straight into insertFromMimeData() with no
         * canPaste() gate in front of it, so an attachment payload still reaches
         * insertFromMimeData()'s attachmentsPasted() branch below, exactly as Ctrl+V does.
         */
        void pasteFromClipboard();

        /**
         * @brief Whether pasteFromClipboard() would do anything right now.
         *
         * Deliberately not just canPaste(): canPaste() consults canInsertFromMimeData(), which
         * returns false for an attachment payload by design (see canInsertFromMimeData() below),
         * so it would report nothing-to-paste for exactly the image/file case that has to work.
         */
        bool canPasteFromClipboard() const;

        /**
         * @brief Remove all text as a single undoable edit.
         *
         * Deliberately not QTextEdit::clear(): its documented behavior is to also purge the
         * undo/redo history, which is exactly right for MessageEditor::clear() (e.g. wiping the
         * composer after a message is sent -- an undo there must not resurrect the sent text)
         * but wrong for a user-facing "Clear" context-menu action, which is expected to be
         * undoable like any other edit.
         */
        void clearUndoable();

    signals:

        void returnPressed();
        void activated();

        /**
         * @brief See AbstractMessageEditor::attachmentsPasted() -- relayed there verbatim by
         *  MessageEditor. Emitted for Ctrl+V, context-menu Paste, and middle-click paste alike,
         *  since all three funnel through insertFromMimeData().
         */
        void attachmentsPasted(const QMimeData* mimeData);

        /**
         * @brief See AbstractMessageEditor::insertRejected() -- relayed there verbatim by
         *  MessageEditor. Emitted from insertFromMimeData() instead of inserting, when the
         *  incoming text would take the document past maxLength().
         */
        void insertRejected(int attemptedLength, int maxLength);

        /**
         * @brief See AbstractMessageEditor::editPreviousRequested() -- relayed there verbatim by
         *  MessageEditor. Emitted for a plain Up-arrow while the document is empty.
         */
        void editPreviousRequested();

        /**
         * @brief Tab (delta +1) or Shift+Tab (delta -1) was pressed outside a table.
         *
         * This widget only RECOGNIZES the gesture; what one indent step means depends on the
         * editing mode and on the document structure under the caret, both of which belong to
         * MessageEditor -- see MessageEditor::applyIndentStep(), which is connected to this.
         *
         * The key is consumed either way, so a Tab never reaches QTextEdit and never inserts a
         * literal tab character: in markdown a leading tab silently turns the line into an
         * indented CODE block, and a tab in the middle of a line is collapsed to a single space
         * by the HTML the message is finally rendered as -- neither is anything a person
         * pressing Tab asked for. Same arrangement as returnPressed() and
         * editPreviousRequested(): this widget spots the keystroke, MessageEditor gives it
         * meaning, and a bare EnhancedTextEdit with nothing connected simply ignores Tab.
         */
        void indentStepRequested(int delta);

        /**
         * @brief A rich-text/table paste was normalized (Stage 5b).
         *
         * Emitted right after insertFromMimeData() has stripped baked colours/fonts, fixed an
         * otherwise-invisible pasted table's border, and converted any pasted property-based
         * code block back to this editor's literal-fence form -- all of which need no state
         * beyond the document itself. What DOES need MessageEditor's own state is re-indenting a
         * pasted blockquote to blockquoteIndent() (Qt's HTML importer bakes its own 40px), so
         * that one step is left to MessageEditor's handler for this signal rather than done here
         * -- same division of labour as indentStepRequested() above.
         */
        void pastedRichText();

        //! See AbstractMessageEditor::mentionQueryChanged() -- relayed there verbatim by
        //! MessageEditor, same arrangement as attachmentsPasted()/editPreviousRequested().
        void mentionQueryChanged(const QString& prefix, int position);

        //! See AbstractMessageEditor::mentionQueryClosed().
        void mentionQueryClosed();

        //! See AbstractMessageEditor::mentionCompletionRequested() -- relayed there verbatim.
        void mentionCompletionRequested(const QString& prefix, int position);

        /**
         * @brief The user just typed the CLOSING ':' of a ":name:" shortcode candidate, and the
         *  guard is offering MessageEditor a chance to replace it before the key is consumed.
         * @param shortcode The name between the colons, e.g. "star" -- WITHOUT either colon,
         *  which has not been inserted (the closing one) or is about to be replaced along with
         *  the name (the opening one).
         * @param position Document position of the opening ':'.
         * @param length Length of "name" alone (position+length is where the caret sits, right
         *  before the not-yet-inserted closing ':').
         *
         * Emitted synchronously from keyPressEvent(), NOT deferred -- see
         * applyEmojiShortcodeGuard()'s own doc comment for why armEmojiShortcodeRevert() being
         * called (or not) inside this same call is what decides whether the ':' is consumed.
         */
        void emojiShortcodeTyped(const QString& shortcode, int position, int length);

    protected:

        void keyPressEvent(QKeyEvent* event) override;
        void focusInEvent(QFocusEvent* event) override;

        //! Re-derives the tab stop from the new font -- QSS drives fonts here, so the ctor's
        //! one-time computation would otherwise be stale from the first theme change onward.
        void changeEvent(QEvent* event) override;

        /**
         * @brief Refuse a payload mimeDataHasAttachments() recognizes as an attachment, so Qt's
         *  own drag-and-drop machinery lets the drag propagate to an ancestor (e.g. a chat page's
         *  FileDropOverlay) instead of the editor claiming it as the drop target.
         */
        bool canInsertFromMimeData(const QMimeData* source) const override;

        /**
         * @brief For an attachment payload, emit attachmentsPasted() instead of inserting it into
         *  the document. Covers Ctrl+V, context-menu Paste, and middle-click paste uniformly, since
         *  QTextEdit funnels all three through this one override.
         */
        void insertFromMimeData(const QMimeData* source) override;

    private:

        /**
         * @brief Whether a PASTED payload should go to the attachment flow rather than into the
         *  document -- a finer question than canInsertFromMimeData()'s.
         *
         * Narrower than mimeDataHasAttachments() on purpose: a payload carrying image bits AND
         * renderable text is a document selection with a picture preview (a Numbers or Pages
         * table, measured, advertises a zero-byte image rendition alongside its real HTML), not a
         * picture. A file payload, or image bits with no text, still goes to attachments.
         *
         * Used only on the paste path; drops keep routing through the broader
         * canInsertFromMimeData() test so they still propagate to a FileDropOverlay.
         */
        bool isAttachmentPaste(const QMimeData* source) const;

    private slots:

        void updateSize();

        //! Recompute the caret's "@word" candidate and emit mentionQueryChanged()/
        //! mentionQueryClosed() only when it has actually changed. Connected to BOTH
        //! QTextEdit::textChanged and QTextEdit::cursorPositionChanged: neither implies the
        //! other, and recomputing from scratch makes running twice for one edit harmless.
        void updateMentionQuery();

        //! Connected to AbstractSpellChecker::dictionaryChanged(). Coalesces a burst of these
        //! (several dictionaries finishing within a few ms is several signals) into ONE
        //! rehighlight() via m_spellRehighlightTimer, which is O(document) and not worth paying
        //! more than once for one dictionary-load event.
        void onSpellDictionaryChanged();

    private:

        //! DefaultTabStopSpaces space-widths of the CURRENT font, applied in the ctor and again
        //! on every font change.
        void applyTabStopDistance();

        //! The Backspace/Delete/typing guard documented at its call site in keyPressEvent().
        //! @return true if the key was fully handled here and must NOT reach QTextEdit.
        bool applyMentionAtomicityGuard(QKeyEvent* event);

        /**
         * @brief The ":shortcode:" auto-replace guard, documented at its call site in
         *  keyPressEvent(). Runs BEFORE applyMentionAtomicityGuard() -- an emoji image is never
         *  an anchor run, so nothing that guard does can conflict with this one.
         * @return true if the key was fully handled here and must NOT reach QTextEdit.
         *
         * Two independent jobs share this one entry point, both keyed off the same small set of
         * keys (Backspace, Delete, a typed ':'):
         *  - REVERT: a Backspace/Delete right at the edge of an armed replacement (see
         *    m_shortcodeReplacement) restores the literal ":name:" text and disarms.
         *  - FORWARD: a typed ':' that completes a ShortcodeCandidate emits emojiShortcodeTyped()
         *    and consumes the key ONLY if the handler called armEmojiShortcodeRevert()
         *    synchronously in response -- checked via m_shortcodeReplacement.armed right after
         *    the emit, which is what makes an unconnected signal, or a lookup miss, fall through
         *    to typing the ':' literally exactly as before this feature existed.
         *
         * m_revertedShortcodePosition is consulted (and, for an unrelated Backspace/Delete
         * elsewhere, cleared) here too -- see that member's own doc comment for why this guard,
         * lacking the continuous per-edit observer applyMentionAtomicityGuard() effectively gets
         * for free via updateMentionQuery(), only gets to re-evaluate it on these same few keys.
         */
        bool applyEmojiShortcodeGuard(QKeyEvent* event);

        bool m_autoResize;
        bool m_newLineOnEnter;
        bool m_expanded=false;
        int m_maxHeight=DefaultMaxHeight;
        int m_maxHeightPercent=0;
        QPointer<QWidget> m_maxHeightReference;
        int m_maxLength=0;

        QColor m_blockquoteColor;
        QColor m_codeBlockColor;
        QColor m_linkColor;
        bool m_linkUnderline=false;
        QColor m_mentionColor;
        QColor m_spellCheckUnderlineColor;
        bool m_spellCheckEnabled=true;

        //! Not owned -- see setSpellChecker(). Nulled automatically if the checker is destroyed
        //! first (connected to QObject::destroyed()).
        QPointer<AbstractSpellChecker> m_spellChecker;

        //! Lazily created in onSpellDictionaryChanged(); see that slot's own doc comment.
        QTimer* m_spellRehighlightTimer=nullptr;

        //! Last state reported through the two mention signals, so a keystroke that does not
        //! change it emits nothing at all (both signals drive a host popup).
        MentionQuery m_lastMentionQuery;

        //! Document position of an '@' the user dismissed with Escape, or -1. Cleared as soon as
        //! the caret leaves that word, so typing on after Escape does not silently reopen the
        //! host's selector, while starting a NEW "@word" does.
        int m_dismissedMentionPosition=-1;

        bool m_emojiShortcodeAutoReplaceEnabled=false;

        //! State for the ONE outstanding shortcode auto-replacement a Backspace/Delete can still
        //! revert -- armed by armEmojiShortcodeRevert() right after MessageEditor replaces a
        //! typed ":name" with its emoji, disarmed by a successful revert (or superseded by a new
        //! replacement; there is never more than one live at a time).
        struct EmojiShortcodeReplacement
        {
            bool armed=false;

            //! First character of the replacement (an image in Wysiwyg, 1-3 UTF-16 units of
            //! ReactionIconInfo::emojiText in Markdown).
            int position=-1;
            int length=0;

            //! Exactly what to restore on revert, INCLUDING both colons, case preserved as
            //! typed (e.g. ":Star:" if that is what the user actually wrote).
            QString literal;

            //! Captured BEFORE the replacement, same reasoning insertEmoji() already documents
            //! for its own `continuation`: after insertImage() the char format at that position
            //! IS the image format, so restoring text there without this would have it inherit
            //! ObjectType/width/height instead of ordinary text formatting.
            QTextCharFormat format;

            //! document()->revision() captured right after the replacement. The validity check
            //! this guards is declarative, not a web of invalidation callbacks: ANY further
            //! document mutation -- another key, paste, undo/redo, loadText(), a mode switch --
            //! changes the revision, which is exactly the set of things that should invalidate
            //! the revert. The one accepted consequence: a Left-then-Right round trip lands back
            //! on the same position with the same revision and leaves this still armed --
            //! "one Backspace restores :name: at this caret" is still true either way.
            int documentRevision=-1;
        };
        EmojiShortcodeReplacement m_shortcodeReplacement;

        //! Document position of a shortcode's OPENING ':' whose auto-replace the user just
        //! reverted with Backspace/Delete -- suppresses an immediate re-trigger if they then
        //! retype the same closing ':' (Backspace-Backspace-':' must not silently re-expand what
        //! was just rejected). Unlike m_dismissedMentionPosition, which a continuous
        //! textChanged/cursorPositionChanged observer (updateMentionQuery()) re-evaluates on
        //! every edit, this can only be re-examined from inside applyEmojiShortcodeGuard() --
        //! itself reachable only via Backspace/Delete/a typed ':' -- so it is cleared there
        //! whenever one of those keys lands clearly outside this word (see that method's own
        //! comment), an approximation of "the caret left that word" rather than an exact match
        //! for it.
        int m_revertedShortcodePosition=-1;

        //! Owned by this widget's document (QSyntaxHighlighter parents itself to it), so it is
        //! never deleted here.
        MessageEditorHighlighter* m_highlighter=nullptr;
};

class MessageEditor_p;

class UISE_DESKTOP_EXPORT MessageEditor : public AbstractMessageEditor
{
    Q_OBJECT

    public:

        //! Characters one Tab is worth when it indents a plain paragraph -- see
        //! setParagraphIndentSpaces().
        constexpr static const int DefaultParagraphIndentSpaces=4;

        //! Pixels one blockquote level indents by -- see setBlockquoteIndent(). Matches
        //! EnhancedTextEdit::DefaultListIndentWidth so a quote and a list at the same depth line
        //! up, and deliberately NOT Qt's own 40px (see setBlockquoteIndent()).
        constexpr static const qreal DefaultBlockquoteIndent=20.0;

        explicit MessageEditor(QWidget* parent=nullptr);

        ~MessageEditor();

        MessageEditor(const MessageEditor&) =delete;
        MessageEditor& operator=(const MessageEditor&) =delete;
        MessageEditor(MessageEditor&&) =delete;
        MessageEditor& operator=(MessageEditor&&) =delete;

        void loadText(const QString& text, TextFormat format=TextFormat::Markdown) override;

        QString text(TextFormat format=TextFormat::Markdown) const override;

        QString selectedText(TextFormat format=TextFormat::Markdown) const override;

        void setFocusIn() override;

        void setPlaceHolderText(const QString& text) override;

        bool hasSelection() const override;

        bool isEmpty() const override;

        //! See AbstractMessageEditor::hasFormatting(). Answered in two steps -- what the user
        //! APPLIED (exact, read straight off the document) and, failing that, whether the literal
        //! text would render as anything but plain paragraphs (which is what catches markdown
        //! SYNTAX typed by hand, a fenced code block above all: this editor's fences are ordinary
        //! text carrying no block properties at all, see convertCodeBlocksToText()).
        bool hasFormatting() const override;

        //! See AbstractMessageEditor::hasAppliedFormatting() -- hasFormatting()'s first step (the
        //! exact document scan) on its own, without its second, rendering-based step.
        bool hasAppliedFormatting() const override;

        //! See AbstractMessageEditor::hasEmoji(). Scans for either form an emoji can take in this
        //! editor's document: an image fragment whose src is an emojiSrc() (gallery-inserted, or
        //! re-imported by normalizeImportedEmoji()), or a literal default-pack CHARACTER (typed
        //! with the OS picker, or Markdown mode's own insertEmoji() output). Same code-point scan
        //! normalizeImportedEmoji() uses, factored out as matchEmojiCodePoints() so the two can
        //! never disagree about what counts.
        bool hasEmoji() const override;

        bool canPasteFromClipboard() const override;

        //! See AbstractMessageEditor::setMaxLength(). Forwarded to the embedded EnhancedTextEdit,
        //! which owns the actual enforcement (insertFromMimeData()) and storage.
        void setMaxLength(int length) override;
        int maxLength() const override;

        void addLeadingWidget(QWidget* widget) override;
        void addTrailingWidget(QWidget* widget) override;

        //! Frame holding the leading widgets and the expand button (objectName
        //! "leadingWidgets"), and the one holding the trailing widgets ("trailingWidgets").
        //! Exposed so a host can style them; their layout DIRECTION is owned by the editor, see
        //! isStackedArrangement(). Never nullptr.
        QFrame* leadingWidgetsFrame() const;
        QFrame* trailingWidgetsFrame() const;

        //! The formatting toolbar shown above the text edit while isExpanded() -- always
        //! present (built hidden in the ctor), so this is never nullptr.
        MessageEditorToolbar* toolbar() const;

        //! The checkable icon-only button in the bottom-left corner -- always present (built
        //! hidden in the ctor, see AbstractMessageEditor::expandButtonVisible), never nullptr.
        IconTextButton* expandButton() const;

        //! The checkable emoji button on the right of the text area -- always present (built
        //! hidden in the ctor, see AbstractMessageEditor::emojiButtonVisible), never nullptr.
        IconTextButton* emojiButton() const;

        //! The embedded EnhancedTextEdit, for host-side tweaks this interface does not expose
        //! (e.g. FileUploadWidget's own max-height clamp).
        EnhancedTextEdit* textEdit() const;

        /**
         * @brief Pixels one indent level is worth, for list nesting and indented blocks.
         *
         * Qt's default is 40px per level, which is what makes a plain one-level list look so
         * far from the margin. NOT a CSS knob: a list's indent comes from
         * QTextListFormat::indent() multiplied by this document-wide width, so
         * messagetext.css-style rules cannot reach it -- and in WYSIWYG mode the content is not
         * parsed from HTML at all, so a stylesheet would not apply even if one were set.
         */
        void setListIndentWidth(qreal width);
        qreal listIndentWidth() const;

        /**
         * @brief How many characters one indent step prepends to a plain paragraph.
         *
         * Deliberately a count of CHARACTERS, not a pixel width: unlike a list's indent (see
         * setListIndentWidth(), a document-wide geometry property), a paragraph indent has to
         * survive being serialized to markdown and rendered back as HTML somewhere else
         * entirely, and markdown has no concept of an indented paragraph at all. The only thing
         * that crosses that boundary is literal text, so the indent IS literal text -- see
         * applyIndentStep() for which character, and why it is not an ordinary space.
         */
        void setParagraphIndentSpaces(int count);
        int paragraphIndentSpaces() const;

        /**
         * @brief Pixels one blockquote level indents by.
         *
         * A blockquote's only visible form in this editor is an indented paragraph, because
         * QTextFormat::BlockQuoteLevel on its own draws NOTHING -- it is export metadata that
         * qtextmarkdownwriter reads, and Qt's layout ignores it entirely. The margin set
         * alongside it is the whole of the rendering.
         *
         * Qt's own importers hardcode 40px per level; this defaults to 20 so a quote lines up
         * with a list at the same depth (EnhancedTextEdit::DefaultListIndentWidth), and content
         * arriving through setMarkdown()/setHtml() is re-indented to match rather than being left
         * at Qt's wider value. Keep this in step with the `blockquote` rule in
         * resources/style/messagetext.css, which is what indents the same message once it is
         * rendered into a chat bubble.
         */
        void setBlockquoteIndent(qreal indent);
        qreal blockquoteIndent() const;

        //! Forwarded to the embedded text edit -- see EnhancedTextEdit::maxHeight(),
        //! maxHeightPercent() and setMaxHeightReferenceWidget().
        void setMaxHeight(int height);
        int maxHeight() const;
        void setMaxHeightPercent(int percent);
        int maxHeightPercent() const;
        void setMaxHeightReferenceWidget(QWidget* widget);

        /**
         * @brief Apply the link a host's AbstractHyperlinkDialog-family dialog collected, in
         *  response to AbstractMessageEditor::linkRequested().
         *
         * MessageEditingMode::Markdown inserts the LITERAL text "[title](url)" -- that mode's
         * document is markdown source, so this is a plain text insert, no escaping (same
         * philosophy as applySourceIndentStep()). MessageEditingMode::Wysiwyg builds a real
         * QTextCharFormat anchor instead (measured: round-trips through toMarkdown()/
         * setMarkdown() as "[title](url)" bit-identical), inheriting whatever bold/italic is
         * already at the caret but never baking a colour -- link colour is the viewer's job
         * (ChatMessageTextBrowser::applyLinkStyle()), not this editor's.
         *
         * If linkRequested()'s own selection is still in force (either the user's own selection,
         * or the whole existing link run the editor selected before emitting the signal for an
         * "edit" case), that selection is REPLACED by url/title; otherwise the link is inserted
         * at the caret. A call while the caret is inside a fenced code block is a no-op -- an
         * anchor's href is not backslash-escaped by Qt's markdown writer the way fence content
         * is, so restoreCodeFences() cannot safely unescape it (measured).
         */
        void insertLink(const QString& url, const QString& title);

        /**
         * @brief Insert a mention in its hidden-UID ANCHOR form, in response to
         *  AbstractMessageEditor::mentionRequested() (task-message-formatting-plan.md, Stage 6).
         *
         * MessageEditingMode::Wysiwyg builds a real QTextCharFormat anchor with href
         * mentionHref(uid) and `title` as its display text -- measured to round-trip through
         * toMarkdown()/setMarkdown() as "[title](whitem-mention:uid)" byte-identically, for every
         * uid shape this project produces and for every title free of unescaped '['/']'. (A title
         * containing a literal ']' or an unmatched '[' does NOT round-trip -- but that is a
         * pre-existing property of Qt's markdown writer for ANY link title, equally true of
         * insertLink(), and deliberately not special-cased here.)
         *
         * MessageEditingMode::Markdown inserts the literal text "[title](whitem-mention:uid)" --
         * that mode's document IS markdown source. MessageEditingMode::Plaintext REFUSES: there
         * is no way to carry a hidden uid in a document with no markup, and quietly writing the
         * title instead would send a message that mentions nobody. insertMentionText() is that
         * mode's route.
         *
         * If the caret is inside an in-progress "@word" (the one mentionQueryChanged() reports),
         * that word is REPLACED, '@' included. An explicit selection wins over it, and is
         * replaced instead -- same rule as insertLink().
         *
         * A no-op inside a fenced code block, inside an existing link, or inside an existing
         * mention -- see canInsertMentionAtCursor().
         *
         * @param uid Character uid. Must be non-empty and contain no raw space (see
         *  mentionHref()).
         * @param title Display text. Falls back to `uid` when empty, same as insertLink() falls
         *  back to the url.
         */
        void insertMention(const QString& uid, const QString& title);

        /**
         * @brief Insert a mention in its PLAIN "@username" text form.
         *
         * Unlike insertLink() and insertMention() above, valid in EVERY MessageEditingMode
         * including Plaintext: the payload is ordinary text carrying no markup meaning, so there
         * is no mode that cannot express it. The '@' is supplied here rather than asked of the
         * caller (and is not doubled if the caller already prefixed it).
         *
         * Always inserted with the anchor properties cleared off the inherited char format, so a
         * plain mention typed right after a link is provably plain.
         *
         * Replaces the in-progress "@word" / the current explicit selection, same rule as
         * insertMention().
         */
        void insertMentionText(const QString& username);

        /**
         * @brief Insert an emoji at the caret -- see AbstractMessageEditor::insertEmoji() for the
         *  per-mode contract and the full list of cases this refuses.
         *
         * WYSIWYG detail worth knowing at the call site: the image is registered as a document
         * resource under its own emojiSrc() URL immediately before it is inserted, so it is
         * visible in the editor right away. The size is derived from the text edit's CURRENT
         * font, and is baked into that image's QTextImageFormat -- a later font change therefore
         * affects only emoji inserted after it, which is the honest behaviour for a document
         * whose other content is likewise already laid out.
         */
        void insertEmoji(const QString& reactionId) override;

        //! @copydoc AbstractMessageEditor::closeEmojiGallery()
        void closeEmojiGallery() override;

        //! @copydoc AbstractMessageEditor::setEmojiRecentIds()
        void setEmojiRecentIds(QStringList ids) override;
        QStringList emojiRecentIds() const override;

        //! How many entries the picker's recents row keeps. Matches the 7 basics the row falls
        //! back to when no history exists, so promoting an emoji never changes the row's width --
        //! a recents row that grew a slot on first use would shift every icon under the pointer.
        constexpr static const int EmojiRecentsMax=7;

        /**
         * @brief Open the emoji gallery, anchored so it unfolds UP and to the RIGHT of the emoji
         *  button. A no-op in MessageEditingMode::Plaintext.
         *
         * @param pinned True (the CLICK behaviour, and the default): the gallery stays until it
         *  is dismissed explicitly -- its X, unchecking the button, Escape, closeEmojiGallery()
         *  -- and the emoji button reads as checked the whole time it is up.
         *  False (the HOVER behaviour): the button stays unchecked, and the gallery closes itself
         *  once the pointer has been away from both it and the button for EmojiHoverCloseDelayMs.
         *  See isEmojiGalleryPinned().
         *
         * Calling this while the gallery is already open only ever PINS it -- it is never
         * re-anchored or reloaded under a user who is already using it.
         */
        void openEmojiGallery(bool pinned=true);

        //! Whether the emoji gallery is currently open, pinned or not. This -- not the emoji
        //! button's own checked state -- is authoritative: IconTextButton::click() toggles
        //! unconditionally after emitting clicked(), so the button's state is only ever written
        //! from this.
        bool isEmojiGalleryOpen() const noexcept;

        /**
         * @brief Whether an open gallery is PINNED (opened or promoted by an explicit CLICK on the
         *  emoji button) rather than merely hovered into view.
         *
         * The distinction is what the emoji button's checked state actually shows: a hover-opened
         * gallery leaves the button UNCHECKED, because the spec's own rule -- "when the emoji
         * button is not checked, hovering it shows the gallery" -- only makes sense if hovering
         * does not itself check the button.
         *
         * A hovered gallery stays up for as long as the pointer is over it or over the button, and
         * closes itself once the pointer has been away from BOTH for EmojiHoverCloseDelayMs -- any
         * number of picks in between changes nothing, since picking deliberately does not pin (the
         * pointer is over the gallery while picking, which is what keeps it up). A pinned one never
         * auto-closes; only an explicit dismissal -- the button, its X, Escape, an outside click,
         * closeEmojiGallery() -- takes it down.
         */
        bool isEmojiGalleryPinned() const noexcept;

        //! Suggestion rows offered per misspelling in the context menu (task-spellcheck.md).
        //! Kept small: a suggestion list is read at a glance, not scanned, and hunspell routinely
        //! returns far more than a short screen has room for above Cut/Copy/Paste.
        constexpr static const int MaxSpellSuggestions=8;

        //! Gap in pixels between the top of the emoji button and the bottom of the gallery it
        //! opens, so the picker does not sit flush against the control that spawned it.
        constexpr static const int EmojiGalleryGap=4;

        /**
         * @brief How long the pointer must REST on the emoji button before hovering opens the
         *  gallery.
         *
         * Small enough to read as instant -- under the ~100ms at which a response stops feeling
         * like a delay at all -- but not zero. The button sits immediately beside a composer's
         * Send button, and a pointer crossing it on the way there covers its ~26px in well under
         * this, so the threshold still costs a deliberate hover nothing while filtering out every
         * pass-through. A click never waits for it.
         *
         * Note this is only ONE of the three things between the hover and a visible gallery; the
         * other two are the first-open construction cost (paid up front instead, see
         * warmEmojiGallery()) and the frame's fade-in (shortened in chatreactions.qss).
         */
        constexpr static const int EmojiHoverOpenDelayMs=80;

        /**
         * @brief Grace period after the pointer leaves BOTH the emoji button and the gallery
         *  before a hover-opened gallery closes itself.
         *
         * The gap between the button and the dialog has to be crossable, and a pointer travelling
         * from one to the other is briefly over neither.
         */
        constexpr static const int EmojiHoverCloseDelayMs=600;

        //! How often the "is the pointer still on the button or the gallery" check runs while a
        //! HOVER-opened gallery is up. Polled rather than driven by enter/leave events: the
        //! gallery is a separate top-level window full of child widgets, and Qt's Enter/Leave
        //! pairs across that boundary (and between the grid's own cells) are far harder to get
        //! right than simply asking where the cursor is.
        constexpr static const int EmojiHoverPollMs=150;

        //! Inline emoji sizes are rounded UP to a multiple of this before they are rasterized.
        //! SvgIcon's pixmap cache is keyed by exact QSize, so an unrounded per-font size would
        //! grow it by one entry for every font tweak the app ever makes; quantizing keeps the
        //! working set to a handful of sizes. A cache-pressure guard, not an optimisation.
        constexpr static const int EmojiSizeQuantum=4;

        /**
         * @brief Pixel size an inline emoji image is given, so it mirrors the surrounding text.
         *
         * Derived from the font's ASCENT rather than its full height: Qt lays an inline image out
         * with its bottom on the baseline, so the ascent is the room available above it -- a
         * full-height image would push the line taller than the text around it. Rounded up to
         * EmojiSizeQuantum.
         */
        static int emojiInlineSizeForFont(const QFont& font);

        //! Forwarded to the embedded EnhancedTextEdit -- see EnhancedTextEdit::setSpellChecker().
        void setSpellChecker(AbstractSpellChecker* checker);
        AbstractSpellChecker* spellChecker() const;

        /**
         * @brief Replace `word` with `replacement` as ONE undoable edit (task-spellcheck.md).
         *
         * Keeps the word's own char format, so a misspelling fixed inside a bold sentence stays
         * bold, and leaves the caret after the replacement with focus back in the text edit. Same
         * shape as insertMentionText(), minus its anchor-clearing step: this never runs inside an
         * anchor, since the tokenizer that produced `word` never yields a word inside one.
         */
        void replaceSpellWord(const EnhancedTextEdit::SpellWord& word, const QString& replacement);

    public slots:

        void selectAll() override;

        void clearSelection() override;

        void clear() override;

        void cut() override;

        void copy() override;

        void paste() override;

    protected:

        void updateMessageEditingMode() override;
        virtual void updateFinishOnEnter() override;
        virtual void updateEditingFinished() override;
        void updateExpanded() override;
        void updateExpandButtonVisible() override;
        void updateMentionButtonVisible() override;
        void updateEmojiButtonVisible() override;
        void updateEmojiShortcodeAutoReplace() override;

        //! Closes the emoji gallery when this editor is hidden -- a floating top-level picker
        //! left over a composer that is no longer on screen would otherwise hang around.
        void hideEvent(QHideEvent* event) override;

        //! Watches the emoji button for Enter/Leave, which is what arms and disarms the
        //! hover-open timer. Never consumes anything.
        bool eventFilter(QObject* watched, QEvent* event) override;
        void updateStackedArrangement() override;
        void updateSpellCheckButtonVisible() override;
        void updateSpellCheckEnabled() override;

    private:

        void setupReturnPressed();

        /**
         * @brief Push the host's placeholder into the text edit, or suppress it.
         *
         * Qt draws the placeholder whenever QTextDocument::isEmpty(), which counts CHARACTERS only
         * -- so an empty block carrying a list, heading or quote is still "empty" to it and the
         * placeholder lands on top of the bullet the layout is painting. Called from the
         * textChanged relay, which also fires for format-only edits.
         */
        void updatePlaceHolderText();

        //! Whether the (single, textually empty) block has block formatting worth showing on its
        //! own -- see updatePlaceHolderText(). False whenever the placeholder could not be drawn
        //! anyway, so a caller never has to check isEmpty() itself.
        bool hasVisibleBlockFormatting() const;

        //! Reflects the caret's live formatting onto pimpl->toolbar. Connected to
        //! EnhancedTextEdit::cursorPositionChanged()/selectionChanged()/
        //! currentCharFormatChanged(), and called explicitly at the end of every format applier
        //! below (unconditional, correct either way).
        void syncToolbarState();

        //! Computed from the caret's live QTextCharFormat/QTextBlockFormat -- shared by
        //! syncToolbarState() (pushed to the toolbar) and showContextMenu() (used to compute the
        //! Formatting submenu's isChecked rows), so both always agree.
        MessageEditorFormatState currentFormatState() const;

        //! emojiButtonVisible() AND a mode that can actually express an emoji. Called from both
        //! updateEmojiButtonVisible() and updateMessageEditingMode(), the two things that can
        //! change either half of that.
        void applyEmojiButtonVisibility();

        //! isEmojiShortcodeAutoReplaceEnabled() AND a mode that can actually express an emoji --
        //! same shape as applyEmojiButtonVisibility() just above, called from both
        //! updateEmojiShortcodeAutoReplace() and updateMessageEditingMode(). Pushes the computed
        //! bool down to pimpl->editor (EnhancedTextEdit is where the keystroke is actually
        //! caught), rather than checking the property there directly, so EnhancedTextEdit itself
        //! never has to know about MessageEditingMode.
        void applyEmojiShortcodeAutoReplace();

        //! Resolve a typed ":shortcode:" against the pack the CURRENT mode can actually insert
        //! from (emojiPackForCurrentMode(), the same source insertEmoji()'s gallery uses), then
        //! replace the typed range with it in one edit block and arm the caller's revert.
        //! Connected to EnhancedTextEdit::emojiShortcodeTyped().
        void onEmojiShortcodeTyped(const QString& shortcode, int position, int length);

        //! Write the emoji button's checked state from the gallery's PINNED state -- the only
        //! authority on it, and deliberately not merely "is it open": see isEmojiGalleryPinned().
        void syncEmojiButtonChecked();

        /**
         * @brief Build the gallery dialog (hidden) if it does not exist yet, and return it.
         *
         * Split out of openEmojiGallery() so the cost can be paid BEFORE the user is waiting on
         * it: constructing the dialog builds a cell per pack entry and rasterizes an SVG for each
         * one, which is by far the largest part of the delay on the FIRST open and is invisible on
         * every one after. warmEmojiGallery() runs it off the hover path entirely.
         *
         * @return The frame, or nullptr in MessageEditingMode::Plaintext (which never shows a
         *  picker) or if the dialog could not be built.
         */
        FloatingEmojiGalleryDialog* ensureEmojiGallery();

        //! Build the gallery ahead of time, on the next event-loop turn, so a later hover or
        //! click shows an already-constructed dialog. Triggered when the emoji button first
        //! becomes visible -- a composer that never opts in never pays for this.
        void warmEmojiGallery();

        //! Push the current recents list at the gallery, if one has been built. A no-op otherwise
        //! -- openEmojiGallery() re-pushes on every open, so a list set before the dialog exists
        //! reaches it on first show.
        void applyEmojiRecentIds();

        //! Move `iconId` to the front of the recents list (capped at EmojiRecentsMax), push the
        //! result at an open gallery and emit emojiRecentIdsChanged(). A no-op when the id is
        //! already first, so re-picking the same emoji does not churn the host's store.
        void promoteEmojiRecent(const QString& iconId);

        //! Promote a hover-opened gallery to pinned -- only ever on a CLICK on the emoji button,
        //! never on a pick (see ensureEmojiGallery()'s emojiPicked handler). A no-op when the
        //! gallery is closed or already pinned.
        void pinEmojiGallery();

        //! Start/stop the poll that closes a hover-opened gallery once the pointer has left both
        //! it and the emoji button. Never runs for a pinned gallery.
        void startEmojiHoverPoll();
        void stopEmojiHoverPoll();

        //! One tick of that poll -- see EmojiHoverPollMs.
        void onEmojiHoverPoll();

        //! Whether the cursor is currently over the emoji button or anywhere over the gallery
        //! window. Asked of QCursor::pos() rather than tracked through Enter/Leave, see
        //! EmojiHoverPollMs.
        bool isCursorOverEmojiUi() const;

        //! The pack the gallery should show for the CURRENT editing mode: the default pack in
        //! Wysiwyg, an EmojiCodeReactionIconPack view of it in Markdown (which can only insert a
        //! literal character, so a codeless icon has nothing to offer there).
        std::shared_ptr<AbstractReactionIconPack> emojiPackForCurrentMode() const;

        //! Hand the gallery the pack for the current mode, skipping the work entirely when the
        //! mode has not changed since it was last filled -- setPack() rebuilds the whole grid,
        //! and a hover must not pay for that.
        void applyEmojiPackForCurrentMode();

        /**
         * @brief Hand keyboard focus back to the text edit after a user-initiated toolbar or
         *  context-menu action, so typing continues where the caret already is.
         *
         * DEFERRED via a zero-delay singleShot, never called inline: a drop-down row's own
         * activation closes its DropdownFrame AFTER the itemToggled()/itemTriggered() handler
         * that lands here has already run (see DropdownMenu::onItemToggled()), and that close
         * moves focus itself -- setting focus inline would simply be undone a moment later.
         * Same deferral rule the rest of this project's focus-restore paths follow.
         */
        void restoreEditorFocus();

        //! Index of a side frame's stretch within its layout, or -1. The stretch is found by
        //! asking for spacerItem() rather than assumed to be first or last -- applyArrangement()
        //! moves the trailing group's from one end to the other.
        static int stretchIndex(QBoxLayout* layout);

        //! Move a side frame's stretch to the front of its layout (or back to the end), so the
        //! group stays packed toward the text area whichever direction the layout runs in.
        static void moveStretch(QBoxLayout* layout, bool toFront);

        //! Point the leading/trailing frames' own layouts along the axis the current arrangement
        //! calls for. Moves nothing: the frames stay in their permanently horizontal row beside
        //! the text area, and only their internal QBoxLayout direction changes.
        void applyArrangement();

        //! Re-evaluates the content against the one-line boundary and flips the arrangement when
        //! it is crossed. Asymmetric on purpose -- see isStackedArrangement().
        void updateArrangementForContent();

        //! Laid-out line count, WRAPPED lines included, capped early since callers only ever ask
        //! "is it more than one".
        int textLineCount() const;

        //! Tail shared by every format applier: reflect the document's new state on the toolbar,
        //! then hand focus back to the text edit. Applies to the toolbar and the context menu
        //! alike -- both are user-initiated, and both should leave the caret ready to type.
        void finishFormatAction();

        void applyBold(bool enable);
        void applyItalic(bool enable);
        void applyUnderline(bool enable);
        void applyStrikethrough(bool enable);
        void applyInlineCode(bool enable);
        void applyBulletList(bool enable);
        void applyNumberedList(bool enable);
        void applyBlockquote(bool enable);
        void applyHeading(int level);
        void applyCodeBlock();

        //! Insert a thematic break in a block of its own, leaving the caret on a clean block below
        //! it. Unlike the toggles this is a pure insert, so there is no "off" and no state for
        //! syncToolbarState() to reflect.
        void applyHorizontalRule();
        void applyTable(int rows, int columns);

        //! Row/column edit on the table the caret is in. A no-op outside one -- the toolbar
        //! already greys these rows, but the guard keeps the applier safe if a host wires the
        //! signal up itself.
        void applyTableAction(MessageEditorTableAction action);

        /**
         * @brief One indent step (delta +1) or outdent step (delta -1), from Tab/Shift+Tab or
         *  from the toolbar's indent buttons.
         *
         * What a step MEANS is picked from the document under the caret, in this order:
         *
         *  1. Caret in a LIST -> step the item's nesting level (every selected item, if there
         *     is a selection). See setListIndentWidth() for how wide one level renders.
         *  2. Something SELECTED -> step the blockquote level of every selected paragraph.
         *     Markdown has exactly one portable way to indent a run of paragraphs and this is
         *     it; QTextFormat::BlockQuoteLevel round-trips through toMarkdown() and back,
         *     nesting included (verified, both directions).
         *  3. Otherwise (a caret with nothing selected) -> insert or remove
         *     paragraphIndentSpaces() NO-BREAK SPACEs AT THE CARET. Not at the start of the
         *     line: Tab is a "widen the gap here" gesture as much as a "shift this line right"
         *     one, and pressing it mid-sentence has to act mid-sentence. Outdent removes only
         *     no-break spaces directly behind the caret, so it is the exact inverse.
         *
         * Case 3 uses U+00A0 rather than an ordinary space for a reason that is not
         * cosmetic: four leading ORDINARY spaces are the markdown syntax for an indented code
         * block, so an indented paragraph would arrive in the chat bubble as monospaced,
         * syntax-highlighted code. A no-break space carries no markdown meaning at all, is not
         * whitespace for CommonMark's indentation rules, and -- unlike a run of ordinary
         * spaces -- is not collapsed by the HTML renderer at the far end. Verified end to end:
         * NBSP-indented text survives toMarkdown(), setMarkdown() and markdownToHtml() intact
         * and stays an ordinary paragraph.
         */
        void applyIndentStep(int delta);

        //! applyIndentStep() for Wysiwyg mode, where the indent is real document structure:
        //! QTextList levels, QTextFormat::BlockQuoteLevel, and NBSP text.
        void applyRichIndentStep(int delta);

        /**
         * @brief applyIndentStep() for the two modes whose document holds plain text.
         *
         * Markdown mode edits markdown SOURCE, so a step is a source edit: leading spaces
         * before a list marker (markdown's own nesting), a "> " prefix for a selection, NBSPs
         * otherwise. Plaintext mode has no markup meaning by definition, so only the NBSP
         * branch applies there -- hence the flag rather than a second near-identical function.
         */
        void applySourceIndentStep(int delta, bool markdownSource);

        /**
         * @brief Re-indent every quoted block to blockquoteIndent(), replacing the flat 40px per
         *  level Qt's markdown and HTML importers bake in. Called after each of those imports, so
         *  a quote applied here and a quote loaded from markdown render identically.
         *
         * @param suppressUndo Disable undo around the re-indent. Right for the whole-document
         *  loads this was written for; WRONG for the paste path, since
         *  QTextDocument::setUndoRedoEnabled(false) clears the undo stack outright (measured) --
         *  see the note in the implementation.
         */
        void normalizeBlockquoteIndent(bool suppressUndo=true);
        void applyClearFormatting();

        /**
         * @brief Extend `cursor`'s selection to the full contiguous ORDINARY-HYPERLINK run it is
         *  inside.
         * @return false, cursor left untouched, if the position is not inside one.
         *
         * "The whole link" is the widest run reachable from the caret's own fragment by walking
         * to the previous/next fragment IN THE SAME BLOCK while it is also an anchor with the
         * SAME href (measured: Qt merges adjacent same-href inserts into one fragment already,
         * but a run built by two separate applyLink()-style char-format writes, or one with
         * mixed bold/italic inside it, stays split across several fragments with identical
         * hrefs) -- a different href never merges, so this cannot walk past one link into an
         * adjacent one. Links do not cross block boundaries in this editor, so the walk is
         * block-local.
         *
         * A MENTION run is deliberately NOT one (Stage 6): "Remove link" must never offer to
         * unlink a mention, and "Edit link" must never open the hyperlink dialog on one. Both
         * gate on MessageEditorFormatState::insideLink, which excludes mentions for the same
         * reason. Implemented as a thin wrapper over the generalized selectAnchorRun() helper
         * (anonymous namespace in messageeditor.cpp), which the Stage 6 atomicity guard in
         * EnhancedTextEdit also reuses for the mention case.
         */
        bool selectLinkRunAtCursor(QTextCursor& cursor) const;

        //! Handles MessageEditorToolbar::linkRequested() and the context menu's Insert-link row.
        //! See AbstractMessageEditor::linkRequested()'s own doc comment for the argument
        //! contract this computes.
        void onLinkButtonRequested();

        //! Handles MessageEditorToolbar::removeLinkRequested() and the context menu's
        //! Remove-link row. A pure document edit with no external input, unlike Link -- never
        //! relayed outward.
        void removeLink();

        /**
         * @brief Whether a mention can be inserted at the caret right now.
         *
         * Three gates: not inside a fenced code block (an anchor's href is not backslash-escaped
         * by Qt's markdown writer the way fence content is, so restoreCodeFences() cannot safely
         * unescape it -- the same measured reason insertLink() refuses there), not inside an
         * existing hyperlink, and not inside an existing mention.
         *
         * Deliberately does NOT include the insideTable gate '@'-DETECTION applies (see
         * EnhancedTextEdit::mentionQueryAtCursor()): auto-popping a host's selector inside a
         * compact table cell is a positioning problem, while a deliberate toolbar click or
         * context-menu selection is an explicit request, and "| @alice | done |" is a perfectly
         * ordinary thing to compose.
         */
        bool canInsertMentionAtCursor() const;

        //! Handles MessageEditorToolbar::mentionRequested() and the context menu's "Mention
        //! someone" row. See AbstractMessageEditor::mentionRequested() for the argument contract.
        void onMentionButtonRequested();

        //! Handles the context menu's suggestion rows (task-spellcheck.md), `index` counted from
        //! MessageEditorMenuAction::SpellSuggestionFirst -- see onContextMenuItemTriggered()'s
        //! range dispatch.
        void applySpellSuggestion(int index);

        //! Handles the context menu's "Add to dictionary" row.
        void addSpellWordToDictionary();

        //! Handles the context menu's "Ignore word" row.
        void ignoreSpellWord();

        std::unique_ptr<MessageEditor_p> pimpl;

    private slots:

        void showContextMenu(const QPoint& pos);
        void onContextMenuItemTriggered(int id);
        void onContextMenuItemToggled(int id, bool checked);
};

}

#endif // UISE_DESKTOP_MESSAGEEDITOR_HPP
