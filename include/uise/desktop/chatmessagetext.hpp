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

/** @file uise/desktop/chatmessagetext.hpp
*
*  Declares ChatMessageText.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATMESSAGETEXT_HPP
#define UISE_DESKTOP_CHATMESSAGETEXT_HPP

#include <vector>

#include <QTextBrowser>
#include <QColor>
#include <QPointer>
#include <QUrl>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractchatmessage.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
class QMimeData;
class QTextTable;

namespace uise {

class SyntaxHighlighter;
class Toast;

class UISE_DESKTOP_EXPORT ChatMessageTextBrowser : public QTextBrowser
{
    Q_OBJECT

    public:

        //! Pixels one list/indent level is worth in a rendered message. MUST stay equal to
        //! EnhancedTextEdit::DefaultListIndentWidth -- a message composed in the editor and the
        //! same message rendered in a bubble have to indent their lists identically, and a
        //! reader comparing the two notices a mismatch immediately. Duplicated rather than
        //! shared so neither header has to include the other; TestListIndentWidthMatchesViewer
        //! fails if the two ever drift apart.
        constexpr static const qreal DefaultListIndentWidth=20.0;

    // task-urls-and characters-in-messages.md, Stage 1: QSS can't reach an inline <a>'s color --
    // it comes from the document's char formats, not the widget's own palette/stylesheet -- so
    // these are exposed as qproperty- settable from QSS instead (same idiom as maxBubbleWidth on
    // AbstractChatMessageText, see chat.qss).
    Q_PROPERTY(QColor linkColor READ linkColor WRITE setLinkColor)
    Q_PROPERTY(bool linkUnderline READ linkUnderline WRITE setLinkUnderline)

    //! task-message-formatting-plan.md, Stage 6: a mention anchor is styled DISTINCTLY from an
    //! ordinary link, via the same QSS-reachable channel and for the same reason as linkColor
    //! above (QSS cannot reach a QTextDocument's char formats). See mentionColor()'s own doc
    //! comment.
    Q_PROPERTY(QColor mentionColor READ mentionColor WRITE setMentionColor)

    // task-message-formatting-plan.md, Stage 3: reactive, not a load-time gate -- the setter
    // itself attaches/detaches and repaints immediately, since a bubble can be constructed and
    // loaded before its first QStyle::polish() (Style::updateWidgetStyle() bails on an un-
    // polished widget), so a qproperty- value arriving from QSS after content is already showing
    // must still take effect. See setSyntaxHighlightingEnabled()'s own doc comment.
    Q_PROPERTY(bool syntaxHighlighting READ isSyntaxHighlightingEnabled WRITE setSyntaxHighlightingEnabled)

    // task-message-formatting-plan.md, Stage 4. Qt does not clip a table that is too wide for the
    // bubble -- it COMPRESSES it, wrapping every cell until the table fits, which turns a readable
    // 6-column table into a tall unreadable sliver (measured: natural 604x84 becomes 372x189 at a
    // 380px bubble, and 132x969 at 120px, where 132px is Qt's hard floor). This property enables
    // the treatment that fixes it -- see applyWideTableLayout().
    Q_PROPERTY(bool wideTableScroll READ isWideTableScrollEnabled WRITE setWideTableScrollEnabled)

    // Independent of wideTableScroll above: that one governs the pin+scrollbar treatment, which
    // only a table too wide to fit ever needs, while the expand button is offered on EVERY table
    // -- a table that fits is still worth opening larger, and it is the only route by which a
    // short table can be copied as a table at all.
    Q_PROPERTY(bool tableExpandButton READ isTableExpandButtonEnabled WRITE setTableExpandButtonEnabled)

    // Reveal-on-hover for that button, mirroring ChatMessageImageItem's own
    // menuButtonVisibleOnHover: an always-visible button over every table (including small ones)
    // is a lot of standing visual weight in a chat log.
    Q_PROPERTY(bool tableExpandButtonVisibleOnHover READ isTableExpandButtonVisibleOnHover WRITE setTableExpandButtonVisibleOnHover)

    /**
     * QSS: qproperty-codeBlockPadding: 8; -- breathing room between a code block's text and the
     * edge of its coloured background, in pixels.
     *
     * Painted rather than laid out, because Qt's rich text has no way to express it: `padding` on
     * a block is parsed and then **ignored** (measured -- `pre { padding: 6px 8px }` and no
     * padding rule at all produce byte-identical geometry), and `margin` moves the background box
     * and the text together, so the background keeps hugging the text to within a pixel either
     * way. The block therefore reserves the room as a margin and paintEvent() fills the rounded
     * rect back out over it -- see applyCodeBlockLayout().
     */
    Q_PROPERTY(int codeBlockPadding READ codeBlockPadding WRITE setCodeBlockPadding)

    /**
     * QSS: qproperty-codeBlockOverlay: true; -- show a small floating strip over each code block
     * carrying its language and a Copy button.
     *
     * The language half exists because Qt cannot render it: QTextDocument::toHtml() does not even
     * emit QTextFormat::BlockCodeLanguage, the layout never reads it, and Qt's CSS subset has no
     * `content:` property to inject it with. It has to be drawn as a widget or not at all.
     */
    Q_PROPERTY(bool codeBlockOverlay READ isCodeBlockOverlayEnabled WRITE setCodeBlockOverlayEnabled)

    //! Reveal-on-hover for that strip, mirroring tableExpandButtonVisibleOnHover for the same
    //! reason: standing chrome over every code block in a chat log is a lot of visual weight.
    Q_PROPERTY(bool codeBlockOverlayVisibleOnHover READ isCodeBlockOverlayVisibleOnHover WRITE setCodeBlockOverlayVisibleOnHover)

    //! QSS: qproperty-codeBlockRadius: 4; -- corner radius of that painted background. Also not
    //! expressible in the document: Qt ignores `border-radius` on a text block.
    Q_PROPERTY(int codeBlockRadius READ codeBlockRadius WRITE setCodeBlockRadius)

    public:

        //! Pixels between a code block's text and the edge of its painted background -- see the
        //! codeBlockPadding property for why this is painted rather than laid out.
        constexpr static const int DefaultCodeBlockPadding=8;

        //! Corner radius of that background, matching what the `pre` rule in messagetext.css used
        //! to ask for with a `border-radius` Qt ignores.
        constexpr static const int DefaultCodeBlockRadius=4;

        explicit ChatMessageTextBrowser(QWidget *parent = nullptr);

        //! One fenced code block on the current document. Positions rather than QTextBlock
        //! handles, matching TrackedTable: they survive the re-layouts that happen between
        //! loading the content and painting it.
        struct TrackedCodeBlock
        {
            int firstPosition=0;
            int lastPosition=0;
            QString language;       //!< empty for an untagged fence
            QPointer<QWidget> overlay;
        };

        //! Every code block applyCodeBlockLayout() found, in document order.
        const std::vector<TrackedCodeBlock>& codeBlocks() const noexcept
        {
            return m_codeBlocks;
        }

        //! Bounding rect of one tracked code block in VIEWPORT coordinates, already inflated by
        //! codeBlockPadding() -- i.e. the rect paintEvent() fills, and the one an overlay should
        //! position itself against. Invalid if the block is no longer in the document.
        QRect codeBlockViewportRect(const TrackedCodeBlock& codeBlock) const;

        //! The tracked code block under a viewport point, or nullptr.
        const TrackedCodeBlock* codeBlockAt(const QPoint& viewportPos) const;

        /**
         * @brief Put one tracked code block's text on the clipboard, exactly as the overlay's Copy
         *  button does.
         *
         * Always emits codeBlockCopied(); additionally shows a confirmation toast unless that was
         * turned off. Same contract as ChatMessageTableViewer::copyTable(), deliberately -- a host
         * should not have to learn two ways of being told that something was copied.
         */
        void copyCodeBlock(const TrackedCodeBlock& codeBlock);

        /**
         * @brief Show a "Copied" toast after copyCodeBlock(). On by default.
         *
         * Turn it off when the host wants to present the confirmation itself -- codeBlockCopied()
         * is emitted either way, so a host can disable this and react to the signal instead.
         */
        void setCopyToastEnabled(bool enable) noexcept
        {
            m_copyToastEnabled=enable;
        }

        bool isCopyToastEnabled() const noexcept
        {
            return m_copyToastEnabled;
        }

        /**
         * @brief Use a host-supplied Toast instead of this widget's own private one.
         * @param toast Not owned; pass nullptr to go back to the built-in fallback.
         *
         * Same idiom, and the same reasoning, as ChatMessageTableViewer::setToast(): routing every
         * confirmation through one shared toast keeps position and styling consistent instead of
         * each widget popping its own. A chat log full of these widgets makes that matter more
         * here than anywhere -- a host will almost certainly want to set one.
         */
        void setToast(Toast* toast) noexcept
        {
            m_toast=toast;
        }

        Toast* toast() const noexcept
        {
            return m_toast;
        }

        void setCodeBlockOverlayEnabled(bool enable);
        bool isCodeBlockOverlayEnabled() const noexcept
        {
            return m_codeBlockOverlay;
        }

        void setCodeBlockOverlayVisibleOnHover(bool enable);
        bool isCodeBlockOverlayVisibleOnHover() const noexcept
        {
            return m_codeBlockOverlayOnHover;
        }

        void setCodeBlockPadding(int padding);
        int codeBlockPadding() const noexcept
        {
            return m_codeBlockPadding;
        }

        void setCodeBlockRadius(int radius) noexcept
        {
            m_codeBlockRadius=radius;
            viewport()->update();
        }
        int codeBlockRadius() const noexcept
        {
            return m_codeBlockRadius;
        }

        void setMessageTextWidget(AbstractChatMessageText* widget);

        AbstractChatMessageText* messageTextWidget() const
        {
            return m_messageTextWidget;
        }

        QSize sizeHint() const override;

        void setWrapWidth(int w);

        int textWidthHint() const;

        //! Rect of the last rendered line of text, in THIS widget's own coordinates (i.e.
        //! including its own frameWidth()/contentsMargins(), unlike a plain QTextLine, which is
        //! in the document's coordinate space). Invalid/null when there is no text at all, when
        //! the last block resolves right-to-left, when the last block sits inside a table -- see
        //! the implementation's own doc comment for why RTL/tables are excluded -- or when this
        //! browser reservesHorizontalScrollBar(): the bottom row is a fixed sibling overlay
        //! positioned once per negotiation pass, so tucking it inline onto a line the user can
        //! then scroll horizontally would slide the text out from under it. Only meaningful
        //! immediately after this browser's wrap width was last set (setWrapWidth()/updateSize())
        //! for the CURRENT layout pass -- it reads the already-computed QTextLine geometry, it
        //! does not lay anything out itself.
        QRect lastLineRect() const;

        /**
         * @brief Toggle focusability + a minimal Copy/Select All context menu.
         * @param enable Off by default -- see AbstractChatMessageBody::setCopyable()'s own doc
         *  comment for why (live chat page vs. a static preview like AbstractReplyDialog's).
         *  Deliberately NOT createStandardContextMenu(): that pulls in Qt's full standard
         *  action set (Cut/Paste, irrelevant since this is read-only, and "Copy Link Location"
         *  for any markdown-rendered link, which a chat message has no meaningful separate
         *  "location" to expose) -- this shows only what actually applies here.
         */
        void setCopyable(bool enable);

        bool isCopyable() const noexcept
        {
            return m_copyable;
        }

        /**
         * @brief Suppress this widget's own built-in Copy/Select All context menu.
         * @param enable On by default -- a host showing this widget's own message-level context
         *  menu instead (e.g. a static preview bubble embedded in a dialog) turns this off so the
         *  two menus don't compete over the same right-click. Independent of setCopyable(): with
         *  this off, the widget stays focusable/selectable (Ctrl+C still works), it just never
         *  pops its own menu -- see updateContextMenuPolicy().
         */
        void setOwnContextMenuEnabled(bool enable);

        bool isOwnContextMenuEnabled() const noexcept
        {
            return m_ownContextMenu;
        }

        //! Like QTextBrowser::setHtml(), but also remembers `html` so linkColor/linkUnderline can
        //! REAPPLY it after rebuilding the document's stylesheet (setDefaultStyleSheet() only
        //! affects content set afterwards -- see applyDocumentStyle()). ChatMessageText::loadText()'s
        //! Html branch calls this instead of setHtml() directly; setPlainText()/setMarkdown() are
        //! unaffected -- Stage 1 never produces a link outside the Html path (see chattextrender.h),
        //! so plain/markdown content has nothing to re-style on a later color/theme change. Also
        //! lazily attaches/re-triggers a code-block syntax highlighter when `html` actually
        //! contains a highlightable fenced code block -- see ensureSyntaxHighlighter()'s own doc
        //! comment (task-message-formatting-plan.md, Stage 3).
        void setHtmlContent(const QString& html);

        //! Like QTextBrowser::setPlainText(), but also clears m_lastHtml -- WITHOUT this, a later
        //! QEvent::StyleChange's applyDocumentStyle() would replay a PREVIOUS message's HTML back
        //! over this plain-text content (m_lastHtml is otherwise only ever cleared by loading new
        //! HTML), which in a recycled flyweight bubble can resurrect another message entirely.
        //! ChatMessageText::loadText()'s Plain branch calls this instead of setPlainText()
        //! directly for exactly that reason (task-message-formatting-plan.md, Stage 3 -- a pre-
        //! existing bug this stage's own testing made visible, not something Stage 3 introduced).
        void setPlainTextContent(const QString& text);

        QColor linkColor() const noexcept
        {
            return m_linkColor;
        }
        void setLinkColor(const QColor& color);

        //! The NON-hovered baseline underline state -- a hovered anchor is always underlined
        //! regardless of this property (see updateHoveredAnchor()), reverting to this value once
        //! the mouse leaves it.
        bool linkUnderline() const noexcept
        {
            return m_linkUnderline;
        }
        void setLinkUnderline(bool enable);

        /**
         * @brief Colour of a `whitem-mention:` anchor's text (task-message-formatting-plan.md,
         *  Stage 6). INVALID by default, which leaves a mention on the blanket `a` rule (i.e.
         *  linkColor above) -- distinctness is opt-in.
         *
         * Emitted by applyDocumentStyle() as a CSS ATTRIBUTE selector,
         * `a[href^="whitem-mention:"]`, deliberately not a `class="mention"` attribute: Qt's HTML
         * parser maps only `class="language-x"` on `<pre>` onto a QTextFormat
         * (BlockCodeLanguage), so a generic class attribute would be discarded on the way in,
         * whereas the href is already there for activation regardless. Measured: an attribute
         * selector outranks a bare element selector in Qt's CSS engine, so the rule wins over the
         * blanket `a` rule regardless of which order the two are appended in.
         *
         * Text-decoration is taken from linkUnderline() above, not a property of its own -- see
         * EnhancedTextEdit::mentionColor's doc comment for why there is no separate
         * "mentionUnderline".
         */
        QColor mentionColor() const noexcept
        {
            return m_mentionColor;
        }
        void setMentionColor(const QColor& color);

        /**
         * @brief Enable/disable code-block syntax highlighting (task-message-formatting-plan.md,
         *  Stage 3). On by default.
         *
         * Reactive rather than a load-time gate: turning this off immediately detaches and
         * destroys the highlighter (QSyntaxHighlighter::setDocument(nullptr) clears every layout
         * format it applied, inside an edit block that does NOT itself trigger a repaint -- see
         * the implementation's own comment for why an explicit viewport()->update() follows it);
         * turning it on immediately (re)attaches and runs one synchronous rehighlight(). Either
         * way there is no "only affects content loaded from now on" gap for a bubble whose
         * qproperty- value arrives from QSS after setHtmlContent() already ran.
         */
        void setSyntaxHighlightingEnabled(bool enable);

        bool isSyntaxHighlightingEnabled() const noexcept
        {
            return m_syntaxHighlightingEnabled;
        }

        /**
         * @brief Enable/disable the wide-table treatment (task-message-formatting-plan.md,
         *  Stage 4). On by default.
         *
         * When on, a table whose natural width exceeds the wrap width is pinned to that natural
         * width instead of being compressed to fit, the browser grows an as-needed horizontal
         * scrollbar to reach the overflow, and a small expand button is floated over the table to
         * open it in a resizable window. Surrounding paragraphs are unaffected -- they keep
         * wrapping at the wrap width. Turning this off restores Qt's own compress-to-fit
         * behaviour immediately, on already-loaded content.
         */
        void setWideTableScrollEnabled(bool enable);

        bool isWideTableScrollEnabled() const noexcept
        {
            return m_wideTableScroll;
        }

        //! Whether this browser's own size hint currently reserves a band for the horizontal
        //! scrollbar, because applyWideTableLayout() pinned at least one table too wide to fit.
        //! Unlike QAbstractScrollArea::horizontalScrollBar()->isVisible(), this is a pure function
        //! of the document + wrap width -- it flips the moment applyWideTableLayout() decides to
        //! pin, not once the scroll area has later caught up and actually shown the bar -- which
        //! is what keeps sizeHint() and lastLineRect() (both of which read it) consistent with
        //! each other and with the rest of the SAME negotiation pass. See applyWideTableLayout()'s
        //! own doc comment for where it is computed.
        bool reservesHorizontalScrollBar() const noexcept
        {
            return m_hScrollReserved;
        }

        /**
         * @brief Show the expand button on every table, not just ones too wide to fit. On by
         *  default.
         *
         * A table that already fits still gets the button: opening it larger is useful in its own
         * right, and it is the only route by which a short table can reach the clipboard AS a
         * table (the expanded viewer is where Copy table lives -- see ChatMessageTableViewer).
         * Turning this off leaves the wide-table pin/scroll treatment untouched.
         */
        void setTableExpandButtonEnabled(bool enable);

        bool isTableExpandButtonEnabled() const noexcept
        {
            return m_tableExpandButton;
        }

        /**
         * @brief Show the expand buttons only while the pointer is over this widget. On by
         *  default.
         *
         * Same idea as ChatMessageImageItem::setMenuButtonVisibleOnHover(), but deliberately
         * coarser: hovering anywhere in the message reveals the button on EVERY table it
         * contains, rather than hit-testing which table the pointer is actually over. A message
         * rarely holds more than one or two tables, and the per-table version would need a
         * mouse-move handler doing a frame hit-test on every pixel of travel.
         */
        void setTableExpandButtonVisibleOnHover(bool enable);

        bool isTableExpandButtonVisibleOnHover() const noexcept
        {
            return m_tableExpandButtonOnHover;
        }

    public slots:

        void updateSize();

    signals:

        /**
         * @brief Emitted whenever a code block reaches the clipboard, regardless of whether a
         *  toast was shown -- the hook for a host that presents its own confirmation.
         * @param language The fence's language tag, empty for an untagged block.
         */
        void codeBlockCopied(const QString& language);

        //! See AbstractChatMessageBody::linkActivated() -- ChatMessageText relays this signal
        //! there. Emitted from anchorClicked(), not from a mouse-press handler, so link
        //! activation always uses Qt's own hit-testing (hyperlinks can wrap across lines, etc.).
        void linkActivated(const QUrl& url);

    protected:

        /**
         * @brief Fills each tracked code block's rounded background, then paints the document over
         *  it as usual.
         *
         * The colour is taken from the block's OWN background brush, which is where
         * messagetext.css's `pre { background-color }` already put it -- deliberately not a second
         * qproperty of this widget, so the editor-side lesson does not repeat itself: two places
         * naming the same colour is two places to forget on a theme change.
         */
        void paintEvent(QPaintEvent* event) override;

        void wheelEvent(QWheelEvent *event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

        //! Re-applies the document stylesheet on QEvent::StyleChange (e.g. a theme switch via
        //! Style::instance().applyStyleSheet()), unconditionally -- see applyDocumentStyle()'s own
        //! doc comment for why this can't be left to the linkColor/linkUnderline qproperty setters
        //! alone (task-message-formatting-plan.md, Stage 1).
        void changeEvent(QEvent* event) override;

        //! Repositions the wide-table expand buttons, which are anchored to the viewport's right
        //! edge and so move whenever the viewport's width does (Stage 4).
        void resizeEvent(QResizeEvent* event) override;

    private slots:

        void showCopyMenu(const QPoint& pos);

    private:

        //! Rebuilds the document's default stylesheet from Style::instance().css() (the theme's
        //! document-level CSS, e.g. resources/style/messagetext.css) plus the link colour/underline
        //! rule built from m_linkColor/m_linkUnderline, appended last so it always wins over
        //! anything messagetext.css declares for `a` (messagetext.css deliberately declares none).
        //! setDefaultStyleSheet() only affects content set AFTERWARDS, so the last-known HTML is
        //! re-applied below to pick up the new style immediately instead of only on the next
        //! message that happens to load.
        void applyDocumentStyle();

        //! Recomputes contextMenuPolicy() from m_copyable/m_ownContextMenu -- the two setters
        //! share this instead of each duplicating the combination.
        void updateContextMenuPolicy();

        //! Underlines/un-underlines the hovered anchor at `pos` (task-urls-and characters-in-
        //! messages.md follow-up: hover feedback on hyperlinks). Cursor shape (arrow vs. pointing
        //! hand) is NOT handled here -- QTextEdit's own base mouseMoveEvent() already does that
        //! automatically for any anchor once LinksAccessibleByMouse is set (the QTextBrowser
        //! default), so duplicating it here would only risk fighting Qt's own IBeam/pointing-hand
        //! switching over plain text vs. a link.
        void updateHoveredAnchor(const QPoint& pos);

        //! Reverts the currently-hovered anchor (if any) back to the base linkUnderline() state.
        //! Called on leaveEvent() and whenever mouseMoveEvent() can't forward to Qt's own anchor
        //! hit-testing (selection mode / outside the widget rect) -- both cases would otherwise
        //! leave a stale hover-underline behind with no further mouse-move event to clear it.
        void clearHoveredAnchor();

        //! Sets fontUnderline on every char-format run in the document whose anchorHref() equals
        //! `href` (a chat bubble's text is short, so a full block/fragment walk per call is cheap).
        void setAnchorUnderline(const QString& href, bool enable);

        /**
         * @brief Lazily attach a SyntaxHighlighter to this document's already-loaded content, if
         *  (a) syntax highlighting is enabled and (b) the document actually contains a fenced
         *  code block with a language tag.
         *
         * Called from setHtmlContent() AFTER setHtml(html) -- attaching before content is loaded
         * would mean `document()->isEmpty()` at attach time, which skips Qt's own
         * `rehighlightPending` gate entirely and makes every future setHtml() reformat
         * synchronously; attaching to an ALREADY non-empty document instead sets that flag and
         * queues a deferred first pass, silently discarding the very reformat this call is meant
         * to trigger (qsyntaxhighlighter.cpp's own setDocument()/_q_reformatBlocks()) -- so this
         * method follows attachment with an explicit, synchronous SyntaxHighlighter::rehighlight()
         * call, which is mandatory here, not an optimisation.
         *
         * `html` is checked with a cheap case-insensitive `contains("<pre"` pre-filter first,
         * then (only if that passes) an exact walk of the now-parsed document() for a non-empty
         * QTextFormat::BlockCodeLanguage -- the same predicate
         * demo/messageformatting/main.cpp's logCodeLanguages() already uses. The exact check is
         * what actually decides: it is immune to attribute quoting/case that the pre-filter can't
         * rule out, and it is also what keeps an untagged fence from attaching a highlighter at
         * all (decision 4: no tag, no highlighting).
         *
         * Idempotent: a no-op once a highlighter is already attached to this document (the
         * document-identity check in the docs of setHtmlContent()'s own call site covers the case
         * where QTextEdit::setDocument() ever swaps documents out from under this widget).
         */
        void ensureSyntaxHighlighter(const QString& html);

        //! The exact predicate behind ensureSyntaxHighlighter()'s pre-filter -- walks the
        //! CURRENTLY LOADED document(), not raw html, so it is immune to how BlockCodeLanguage's
        //! class="language-x" attribute happened to be quoted/cased in the source markup.
        bool documentHasCodeLanguage() const;

        /**
         * @brief Pin every table that is too wide for the wrap width to its own natural width,
         *  and switch the horizontal scrollbar on when any table was pinned.
         *
         * Qt's own behaviour for an oversized table is to COMPRESS it -- shrink columns and wrap
         * cell text until it fits, down to a hard floor set by the longest unbreakable word in
         * each column. That keeps the table inside the bubble but makes it unreadably tall (see
         * the wideTableScroll property's own comment for measured numbers). Pinning the table's
         * frame format to the width it wants (`QTextTableFormat::setWidth()`, FixedLength) makes
         * Qt lay it out at full size and overflow the document's right edge instead -- crucially,
         * WITHOUT affecting any other block: paragraphs around the table keep wrapping at the
         * wrap width, because only the table's own frame carries the override.
         *
         * Natural width is measured by briefly laying the live document out unconstrained
         * (`setTextWidth(-1)`) and reading each table's `frameBoundingRect()`, then restoring the
         * real wrap width -- cheaper and more accurate than re-parsing the HTML into a probe
         * document, and only ever done for a document that actually contains a table.
         *
         * Called after every content load and re-load (setHtmlContent(), applyDocumentStyle()'s
         * replay), because setHtml() rebuilds the document and discards the pinned formats.
         */
        void applyWideTableLayout();

        //! Assigns m_hScrollReserved, invalidating this widget's cached size hint only when the
        //! value actually changed -- called from every applyWideTableLayout() return path (both
        //! early-outs and the pinning path itself) so it always tracks that pass' own decision.
        void setHScrollReserved(bool reserve);

        /**
         * @brief Find every fenced code block on the freshly loaded document, record it, and give
         *  each one room for its painted padding.
         *
         * Called from setHtmlContent() right after setHtml(), for the same reason
         * applyWideTableLayout() is: the document has just been rebuilt, so anything recorded for
         * the previous message is gone with it.
         *
         * **How a code block is recognised, and why it takes a step to do it.** Qt's HTML parser
         * marks a `<pre>` with QTextBlockFormat::nonBreakableLines(), which is the one signal that
         * does not depend on the markup being tagged -- QTextFormat::BlockCodeLanguage is only set
         * for `<pre class="language-x">`, so an UNTAGGED fence carries nothing at all. But our own
         * messagetext.css used to clear even that: `white-space: pre-wrap` overrides the white-space
         * mode Qt derives the flag from, and measured, it took `nonBreakableLines` from 1 to 0 and
         * left an untagged code block indistinguishable from a styled paragraph. So the rule is
         * gone from the stylesheet, and the flag is instead read here and then CLEARED, which
         * restores exactly the wrapping `pre-wrap` was there to provide. The runs live in
         * m_codeBlocks from then on, the same way applyWideTableLayout() keeps m_tables.
         */
        void applyCodeBlockLayout();


        //! Create/position/show one expand button per pinned table, or hide them all when nothing
        //! is pinned. Buttons are children of viewport() anchored to its RIGHT edge (not the
        //! table's own right edge, which by definition is scrolled off-screen) at the table's own
        //! vertical band, so a button stays put while its table scrolls underneath it.
        void updateTableExpandButtons();

        //! Show/hide every expand button according to tableExpandButtonVisibleOnHover() and
        //! whether the pointer is currently over this widget.
        void updateTableExpandButtonVisibility();

        //! Create (or reuse) and position the floating language+Copy strip over each tracked code
        //! block. Called from applyWideTableLayout(), which is the pass that already runs on every
        //! bubble-width negotiation -- the strip has to follow the block when the width changes.
        //! The toast to show a copy confirmation on: the host-supplied one if setToast() was
        //! called, otherwise a lazily created private fallback. See the identical helper on
        //! ChatMessageTableViewer for why it is parented to the window and drawn in-parent.
        Toast* ensureCopyToast();

        void updateCodeBlockOverlays();

        //! Hover gate for those strips, mirroring updateTableExpandButtonVisibility() exactly.
        void updateCodeBlockOverlayVisibility();

        //! Plain text of one tracked code block, as it would be copied.
        QString codeBlockText(const TrackedCodeBlock& codeBlock) const;

        //! Open the pinned table at `index` in a resizable top-level window (FloatingDialogFrame,
        //! the only shell in this library that hosts an arbitrary widget) showing just that table
        //! with both scrollbars.
        void openTableViewer(int index);

        //! Every table applyWideTableLayout() found on the current document, in document order --
        //! NOT only the wide ones. Pinning and the expand button are independent: `pinned` marks
        //! the subset that was too wide to fit and so got its frame width pinned (and, between
        //! them, brought the horizontal scrollbar on), while `button` is created for *every*
        //! table, since a table that fits still benefits from being openable and copyable.
        struct TrackedTable
        {
            int firstPosition=0;    //!< QTextFrame::firstPosition(), to re-find the table later
            qreal naturalWidth=0;
            bool pinned=false;
            QPointer<QWidget> button;
        };

        AbstractChatMessageText* m_messageTextWidget=nullptr;
        bool m_copyable=false;
        bool m_ownContextMenu=true;
        QColor m_linkColor;
        bool m_linkUnderline=false;
        QColor m_mentionColor;
        QString m_lastHtml;
        QString m_hoveredAnchor;
        SyntaxHighlighter* m_highlighter=nullptr;
        bool m_syntaxHighlightingEnabled=true;
        bool m_wideTableScroll=true;
        //! See reservesHorizontalScrollBar()'s own doc comment. Set only from
        //! applyWideTableLayout(), never from the scroll area's own asynchronous visibility.
        bool m_hScrollReserved=false;
        bool m_tableExpandButton=true;
        bool m_tableExpandButtonOnHover=true;
        std::vector<TrackedTable> m_tables;
        std::vector<TrackedCodeBlock> m_codeBlocks;
        int m_codeBlockPadding=DefaultCodeBlockPadding;
        int m_codeBlockRadius=DefaultCodeBlockRadius;
        bool m_codeBlockOverlay=true;
        bool m_codeBlockOverlayOnHover=true;
        bool m_copyToastEnabled=true;
        Toast* m_toast=nullptr;         //!< host-supplied, not owned
        Toast* m_ownToast=nullptr;      //!< lazy fallback, parented to the hosting window
};

/**
 * @brief Read-only viewer for a single table, shown by
 *  ChatMessageTextBrowser::openTableViewer() when a wide table's expand affordance is used
 *  (task-message-formatting-plan.md, Stage 4).
 *
 * Exists as its own class purely for clipboard behaviour. Qt's own copy already offers four
 * flavours for a table selection -- `text/html` (a real `<table>`, which is what Excel, Word,
 * Google Sheets and LibreOffice actually consume when pasting), `text/markdown` (a correctly
 * aligned GFM pipe table, so a copied table pastes straight back into another chat message),
 * and ODF -- so pasting into an external app as a genuine table already works. The one flavour
 * Qt gets wrong for this content is `text/plain`: it emits ONE CELL PER LINE, where every
 * spreadsheet's convention is a tab between cells and a newline between rows. This class
 * replaces just that flavour and leaves the other three untouched.
 */
class UISE_DESKTOP_EXPORT ChatMessageTableViewer : public QTextBrowser
{
    Q_OBJECT

    public:

        explicit ChatMessageTableViewer(QWidget* parent=nullptr);

        //! Select the whole table and copy it, i.e. what the Copy table button and context-menu
        //! item do. Goes through the ordinary copy path, so the clipboard ends up with exactly
        //! the same flavour set as a manual selection would produce, then CLEARS the selection
        //! again -- selecting was a means to that end, and leaving the table highlighted would
        //! just look like a stray selection. Always emits tableCopied(); additionally shows a
        //! confirmation toast unless that was turned off.
        void copyTable();

        /**
         * @brief Show a "Copied" toast after copyTable(). On by default.
         *
         * Turn it off when the host wants to present the confirmation itself -- tableCopied() is
         * emitted either way, so a host can disable this and react to the signal instead.
         */
        void setCopyToastEnabled(bool enable) noexcept
        {
            m_copyToastEnabled=enable;
        }

        bool isCopyToastEnabled() const noexcept
        {
            return m_copyToastEnabled;
        }

        /**
         * @brief Use a host-supplied Toast instead of this viewer's own private one.
         * @param toast Not owned; pass nullptr to go back to the built-in fallback.
         *
         * Same idiom as AbstractNewPasswordPanel::setGlobalToast() -- letting an app route every
         * confirmation through one shared toast keeps position and styling consistent, instead of
         * each widget popping its own.
         */
        void setToast(Toast* toast) noexcept
        {
            m_toast=toast;
        }

        Toast* toast() const noexcept
        {
            return m_toast;
        }

    signals:

        //! Emitted by copyTable() whenever the table reaches the clipboard, regardless of whether
        //! a toast was shown -- the hook for a host that presents its own confirmation.
        void tableCopied();

    protected:

        //! Replaces the `text/plain` flavour with tab-separated rows (cells joined by '\t', rows
        //! by '\n') built from the cells the selection actually covers, leaving `text/html`,
        //! `text/markdown` and ODF exactly as Qt produced them.
        QMimeData* createMimeDataFromSelection() const override;

        //! Copy / Copy table / Select all.
        void contextMenuEvent(QContextMenuEvent* event) override;

        //! Makes Ctrl+C with NO selection copy the whole table, instead of doing nothing as
        //! QTextEdit::copy() would (it returns early unless the cursor has a selection).
        void keyPressEvent(QKeyEvent* event) override;

    private:

        //! The single table this viewer was built around, or nullptr if the content somehow has
        //! none (defensive -- openTableViewer() only ever loads a table into it).
        QTextTable* tableOfDocument() const;

        //! The toast to show a copy confirmation on: the host-supplied one if setToast() was
        //! used, otherwise this viewer's own, created on first use and drawn INSIDE the viewer
        //! (Toast::setDrawInParent()) rather than as a separate top-level window.
        Toast* ensureToast();

        bool m_copyToastEnabled=true;
        Toast* m_toast=nullptr;         //!< host-supplied, not owned
        Toast* m_ownToast=nullptr;      //!< lazy fallback, child of this widget
};

class ChatMessageText_p;

class UISE_DESKTOP_EXPORT ChatMessageText : public AbstractChatMessageText
{
    Q_OBJECT

    public:

        ChatMessageText(QWidget* parent=nullptr);

        ~ChatMessageText();
        ChatMessageText(const ChatMessageText&)=delete;
        ChatMessageText& operator=(const ChatMessageText&)=delete;
        ChatMessageText(ChatMessageText&&)=delete;
        ChatMessageText& operator=(ChatMessageText&&)=delete;

        void loadText(const QString& text, TextFormat format=TextFormat::Markdown) override;

        void clearText() override;

        void clearContentSelection() override;

        int bubbleWidthHint(int forMaxWidth) override;

        void updateMaximumBubbleWidth() override;

        QRect lastTextLineRect() const override;

        int ownWidthCeiling() const override;

        QString selectedText() const override;

        bool hasSelectableText() const override;

        void setCopyable(bool enable) override;

        void setOwnContextMenuEnabled(bool enable) override;

        void selectText(const QString& text) override;

        QString linkAt(const QPoint& pos) const override;

    protected:

        void updateChatMessage() override;

        //! Re-renders the last loadText() source through the new allowlist -- see
        //! AbstractChatMessageText::setMentionsEnabled(). This is exactly what
        //! ChatMessageText_p::sourceText was cached for (its own doc comment names Stage 6).
        void updateMentionsEnabled() override;

        //! Re-renders the last loadText() source with the new extraLinkify hook threaded through
        //! -- see AbstractChatMessageText::setExtraLinkify().
        void updateExtraLinkify() override;

    private:

        std::unique_ptr<ChatMessageText_p> pimpl;
};

}

#endif // UISE_DESKTOP_CHATMESSAGETEXT_HPP
