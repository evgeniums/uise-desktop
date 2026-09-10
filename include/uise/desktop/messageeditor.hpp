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
#include <uise/desktop/abstractmessageeditor.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class IconTextButton;
class MessageEditorToolbar;

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

    private:

        //! DefaultTabStopSpaces space-widths of the CURRENT font, applied in the ctor and again
        //! on every font change.
        void applyTabStopDistance();

        bool m_autoResize;
        bool m_newLineOnEnter;
        bool m_expanded=false;
        int m_maxHeight=DefaultMaxHeight;
        int m_maxHeightPercent=0;
        QPointer<QWidget> m_maxHeightReference;

        QColor m_blockquoteColor;
        QColor m_codeBlockColor;
        QColor m_linkColor;
        bool m_linkUnderline=false;

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

        bool canPasteFromClipboard() const override;

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
        void updateStackedArrangement() override;

    private:

        void setupReturnPressed();

        //! Reflects the caret's live formatting onto pimpl->toolbar. Connected to
        //! EnhancedTextEdit::cursorPositionChanged()/selectionChanged()/
        //! currentCharFormatChanged(), and called explicitly at the end of every format applier
        //! below (unconditional, correct either way).
        void syncToolbarState();

        //! Computed from the caret's live QTextCharFormat/QTextBlockFormat -- shared by
        //! syncToolbarState() (pushed to the toolbar) and showContextMenu() (used to compute the
        //! Formatting submenu's isChecked rows), so both always agree.
        MessageEditorFormatState currentFormatState() const;

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
         * @brief Extend `cursor`'s selection to the full contiguous anchor run it is inside.
         * @return false, cursor left untouched, if the position is not inside a link at all.
         *
         * "The whole link" is the widest run reachable from the caret's own fragment by walking
         * to the previous/next fragment IN THE SAME BLOCK while it is also an anchor with the
         * SAME href (measured: Qt merges adjacent same-href inserts into one fragment already,
         * but a run built by two separate applyLink()-style char-format writes, or one with
         * mixed bold/italic inside it, stays split across several fragments with identical
         * hrefs) -- a different href never merges, so this cannot walk past one link into an
         * adjacent one. Links do not cross block boundaries in this editor, so the walk is
         * block-local.
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

        std::unique_ptr<MessageEditor_p> pimpl;

    private slots:

        void showContextMenu(const QPoint& pos);
        void onContextMenuItemTriggered(int id);
        void onContextMenuItemToggled(int id, bool checked);
};

}

#endif // UISE_DESKTOP_MESSAGEEDITOR_HPP
