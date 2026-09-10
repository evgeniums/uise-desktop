/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/chatmessagetext.cpp
*
*  Defines ChatMessageText.
*
*/

/****************************************************************************/

#include <limits>

#include <QCoreApplication>
#include <QTimer>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QMenu>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextLayout>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextTable>
#include <QTextFrame>
#include <QTextDocumentFragment>
#include <QPushButton>
#include <QScrollBar>
#include <QMimeData>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QShortcut>
#include <QAbstractTextDocumentLayout>
#include <QtMath>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/markdownrenderer.hpp>
#include <uise/desktop/syntaxhighlighter.hpp>
#include <uise/desktop/floatingdialog.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/toast.hpp>
#include <uise/desktop/chatmessagetext.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

/******************************EnhancedTextEdit********************************/

//--------------------------------------------------------------------------

ChatMessageTextBrowser::ChatMessageTextBrowser(QWidget* parent) : QTextBrowser(parent)
{
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Preferred);

    setLineWrapMode(FixedPixelWidth);

    // Qt's own 40px per indent level leaves even a one-level list a long way from the bubble's
    // edge, and it has to agree with the composer's -- see DefaultListIndentWidth.
    document()->setIndentWidth(DefaultListIndentWidth);

    // task-urls-and characters-in-messages.md, Stage 1: without this, QTextBrowser's default
    // openLinks=true/openExternalLinks=false makes a click call setSource() on itself, which
    // blanks the bubble instead of doing anything useful. Activation is relayed via
    // linkActivated() instead -- the host (whitemdesktop) decides what a click actually does.
    setOpenLinks(false);
    connect(this,&QTextBrowser::anchorClicked,this,&ChatMessageTextBrowser::linkActivated);

    // This is a read-only display widget -- nothing ever exposes Ctrl+Z/Ctrl+Shift+Z here, and
    // updateHoveredAnchor()'s underline toggling would otherwise silently grow an unused undo
    // stack every time the mouse crosses a link.
    document()->setUndoRedoEnabled(false);

    // Qt's macOS style draws the native Cocoa focus ring itself, outside the stylesheet paint
    // path entirely -- chat.qss's "outline: none;" on :focus (a QSS-level property) has no
    // effect on it. WA_MacShowFocusRect is the Qt-level switch for that ring specifically; a
    // no-op on every other platform. Only reachable at all once a host opts into
    // setCopyable(true), see that method's own doc comment.
    setAttribute(Qt::WA_MacShowFocusRect,false);

    // Wired unconditionally -- inert while contextMenuPolicy() stays Qt::NoContextMenu (the
    // default, see setCopyable()), so this only ever fires once a host actually opts in.
    connect(this,&QWidget::customContextMenuRequested,this,&ChatMessageTextBrowser::showCopyMenu);

    Style::updateWidgetStyle(this);

#if 0
    // task-message-formatting-plan.md, Stage 3: do NOT re-enable this. A SyntaxHighlighter's
    // explicit rehighlight() call (ensureSyntaxHighlighter(), applyDocumentStyle()) wraps its
    // pass in QTextCursor::beginEditBlock()/endEditBlock(), and QTextDocumentPrivate::finishEdit()
    // emits contentsChanged() UNCONDITIONALLY at the end of that block -- so with this connected,
    // every theme switch (and every first highlight of a freshly-loaded code block) would re-
    // enter updateSize() via this queued singleShot, which was never exercised while this block
    // stayed disabled and is not something Stage 3 verified.
    connect(this,
        &QTextBrowser::textChanged,
        this,
        [this]()
        {
            QTimer::singleShot(
                0,
                this,
                [this]()
                {
                    updateSize();
                }
            );
        }
    );
#endif
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::updateSize()
{
    // Shrink-wrap the document to its content -- but NEVER wider than the wrap width. Without
    // the clamp, a pinned wide table (applyWideTableLayout()) pushes idealWidth() past the wrap
    // width (measured: 380 -> 612 for one 6-column table), and setting THAT as the text width
    // would re-lay every surrounding paragraph out at 612 too, unwrapping the prose the pinning
    // was careful not to touch. Clamping is a no-op for content without a pinned table: Qt
    // already caps idealWidth() at the text width in that case.
    auto ideal=document()->idealWidth();
    auto cap=lineWrapColumnOrWidth();
    document()->setTextWidth((cap>0 && ideal>cap) ? static_cast<qreal>(cap) : ideal);
    updateGeometry();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setWrapWidth(int w)
{
    setLineWrapColumnOrWidth(w);
    // Re-evaluate wide tables against the NEW width before shrink-wrapping: this is where the
    // wrap width first becomes known at all (setHtmlContent() runs before any negotiation pass,
    // so pinning cannot happen there), and a later pass can widen the bubble enough that a
    // previously pinned table now fits and should be un-pinned again.
    applyWideTableLayout();
    updateSize();
}

//--------------------------------------------------------------------------

int ChatMessageTextBrowser::textWidthHint() const
{
    // Rounded UP, not truncated: callers use this as a width the document is guaranteed to fit
    // into without any further wrapping -- see ChatMessageText::bubbleWidthHint()'s identical
    // reasoning for its own lastHintWidth.
    return qCeil(document()->idealWidth());
}

//--------------------------------------------------------------------------

QRect ChatMessageTextBrowser::lastLineRect() const
{
    auto* doc=document();
    if (doc==nullptr || doc->isEmpty())
    {
        return {};
    }

    // Walk back from the last block to the last one that actually rendered a line -- a trailing
    // empty block (e.g. text ending in a blank line) has a valid QTextBlock but an empty layout.
    auto block=doc->lastBlock();
    while (block.isValid() && (!block.isVisible() || block.layout()==nullptr
                                || block.layout()->lineCount()==0))
    {
        block=block.previous();
    }
    if (!block.isValid())
    {
        return {};
    }

    // Trailing-space overlay only makes sense for LTR text: the blank space after the last word
    // sits on the RIGHT for LTR, but on the LEFT for RTL, and naturalTextWidth() below always
    // measures the line's own extent regardless of direction -- treating that as "room after the
    // text" would be backwards for RTL and would incorrectly widen every RTL message. RTL inline
    // placement is a separate task; these messages simply keep the full-width row (see
    // AbstractChatMessageContent::evaluateInlineBottom()'s handling of an invalid rect here).
    if (block.textDirection()==Qt::RightToLeft)
    {
        return {};
    }

    auto* lay=block.layout();
    auto line=lay->lineAt(lay->lineCount()-1);
    // blockBoundingRect() reads the layout bubbleWidthHint()/updateMaximumBubbleWidth() already
    // computed -- no second document layout pass here, which matters since this can run once per
    // message during a chat load (see [[todo-chat-message-paint-and-layout-cost]]). Its origin
    // already carries the document's own margin (QTextDocument::documentMargin()).
    auto br=doc->documentLayout()->blockBoundingRect(block);
    int offX=frameWidth()+contentsMargins().left();
    int offY=frameWidth()+contentsMargins().top();
    return QRect{qFloor(br.x()+line.x())+offX, qFloor(br.y()+line.y())+offY,
                 qCeil(line.naturalTextWidth()), qCeil(line.height())};
}

//--------------------------------------------------------------------------

QSize ChatMessageTextBrowser::sizeHint() const
{
    if (document())
    {
        QSizeF docSize = document()->size();
        int height = static_cast<int>(docSize.height() + 2 * frameWidth());
        int width = static_cast<int>(document()->idealWidth() + 2 * frameWidth());

        // Same clamp, and for the same reason, as updateSize(): a pinned wide table inflates
        // idealWidth() beyond the wrap width, and reporting THAT here would ask the layout for a
        // bubble as wide as the table -- defeating maxBubbleWidth and the whole point of scrolling
        // to the overflow instead. ChatMessageText::bubbleWidthHint() already clamps its own
        // return value; this is the same guarantee for Qt's direct sizeHint() path.
        auto cap=lineWrapColumnOrWidth();
        if (cap>0 && width>cap+2*frameWidth())
        {
            width=cap+2*frameWidth();
        }

        // An as-needed horizontal scrollbar is drawn INSIDE this widget, so its height has to be
        // part of the hint or the last line of the document is clipped behind it -- the same
        // bookkeeping NavigationBar does for its own as-needed horizontal bar (see
        // src/navigationbar.cpp's sizeHint()).
        if (horizontalScrollBar()!=nullptr && horizontalScrollBar()->isVisible())
        {
            height+=horizontalScrollBar()->sizeHint().height();
        }

        return QSize{width,height};
    }

    return QTextBrowser::sizeHint();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::wheelEvent(QWheelEvent *event)
{
    event->ignore();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setMessageTextWidget(AbstractChatMessageText* widget)
{
    m_messageTextWidget=widget;
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setHtmlContent(const QString& html)
{
    m_lastHtml=html;
    setHtml(html);
    // setHtml() does NOT replace the underlying QTextDocument object -- QWidgetTextControlPrivate::
    // setContent() only allocates a new one when this widget has none yet, otherwise it calls
    // doc->setHtml() on the SAME object (verified against Qt 6.9.0 source; the comment this
    // replaced claimed the opposite). Any style already set via setDefaultStyleSheet() before
    // this call is therefore naturally still in effect, AND a SyntaxHighlighter already attached
    // to document() survives this call unaffected -- nothing else to do for either concern.
    if (m_syntaxHighlightingEnabled)
    {
        ensureSyntaxHighlighter(html);
    }
    // setHtml() rebuilt the document, so any table pinned for the PREVIOUS content is gone along
    // with it -- re-measure and re-pin for this one (task-message-formatting-plan.md, Stage 4).
    applyWideTableLayout();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setPlainTextContent(const QString& text)
{
    // WITHOUT clearing m_lastHtml here, the next QEvent::StyleChange's applyDocumentStyle() would
    // replay a PREVIOUS message's HTML back over this plain-text content (m_lastHtml is otherwise
    // only ever cleared by loading new HTML) -- in a recycled flyweight bubble, potentially
    // someone else's message. See this method's own header doc comment.
    m_lastHtml.clear();
    setPlainText(text);
    // Plain text has no tables -- this clears any pin/button left over from previous HTML content
    // (whose document setPlainText() has just discarded) and puts the scrollbar policy back.
    applyWideTableLayout();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setLinkColor(const QColor& color)
{
    if (m_linkColor==color)
    {
        return;
    }
    m_linkColor=color;
    applyDocumentStyle();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setLinkUnderline(bool enable)
{
    if (m_linkUnderline==enable)
    {
        return;
    }
    m_linkUnderline=enable;
    applyDocumentStyle();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setSyntaxHighlightingEnabled(bool enable)
{
    if (m_syntaxHighlightingEnabled==enable)
    {
        return;
    }
    m_syntaxHighlightingEnabled=enable;

    if (!enable)
    {
        if (m_highlighter!=nullptr)
        {
            // setDocument(nullptr) clears every layout format the highlighter applied
            // (QSyntaxHighlighter::setDocument()'s own blk.layout()->clearFormats() loop), but
            // does so inside an edit block that leaves QTextDocumentPrivate's docChangeFrom at
            // -1 -- the layout is never told, so nothing repaints on its own. Force it.
            m_highlighter->setDocument(nullptr);
            delete m_highlighter;
            m_highlighter=nullptr;
            viewport()->update();
        }
        return;
    }

    ensureSyntaxHighlighter(m_lastHtml);
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::changeEvent(QEvent* event)
{
    QTextBrowser::changeEvent(event);
    if (event->type()==QEvent::StyleChange)
    {
        // Style::instance().css() can change on a theme switch even when this widget's OWN
        // linkColor/linkUnderline qproperty values do not (e.g. a switch that only touches
        // messagetext.css) -- setLinkColor()/setLinkUnderline() early-return in that case (see
        // their bodies above) and never reapply, so the base document CSS is re-pulled
        // unconditionally here instead of depending on the qproperty writers alone.
        applyDocumentStyle();
    }
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::applyDocumentStyle()
{
    // Theme's document-level CSS (task-message-formatting-plan.md, Stage 1) -- e.g.
    // resources/style/messagetext.css -- forms the base; the link colour/underline rule is
    // appended last so it always wins over anything messagetext.css declares for `a` (it
    // deliberately declares none, to avoid needing to reason about override order between two
    // sources of anchor styling).
    QString css=Style::instance().css();

    QString linkCss=QStringLiteral("a { text-decoration: %1; }").arg(m_linkUnderline ? "underline" : "none");
    if (m_linkColor.isValid())
    {
        linkCss=QStringLiteral("a { color: %1; text-decoration: %2; }")
                .arg(m_linkColor.name(),m_linkUnderline ? "underline" : "none");
    }
    css+=QStringLiteral("\n")+linkCss;

    document()->setDefaultStyleSheet(css);

    if (m_syntaxHighlightingEnabled)
    {
        // Idempotent in the overwhelmingly common case (a highlighter attached during the
        // original setHtmlContent()/setSyntaxHighlightingEnabled(true) call already exists here
        // and this just re-syncs its document() pointer, a no-op absent an external
        // QTextEdit::setDocument() call) -- see ensureSyntaxHighlighter()'s own doc comment.
        ensureSyntaxHighlighter(m_lastHtml);
        if (m_highlighter!=nullptr)
        {
            // Re-pulled BEFORE the replay/rehighlight below, so either path repaints with the
            // NEW palette in one pass rather than the old one followed immediately by a second.
            m_highlighter->refreshColors();
        }
    }

    // setDefaultStyleSheet() only affects content set AFTERWARDS -- reapply the last HTML we
    // know about so an already-rendered bubble picks up a theme/color change immediately (e.g.
    // Style::updateWidgetStyle()'s repolish on a light/dark switch) instead of only the next
    // message that happens to load.
    if (!m_lastHtml.isEmpty())
    {
        setHtml(m_lastHtml);
        // The replay rebuilt the document, discarding every pinned table format with it -- same
        // reason setHtmlContent() re-pins after its own setHtml() (Stage 4).
        applyWideTableLayout();
        // The reload above reset every anchor to the base style, including one that was mid-hover
        // -- re-apply its hover-underline immediately rather than waiting for the next mouse move.
        if (!m_hoveredAnchor.isEmpty())
        {
            setAnchorUnderline(m_hoveredAnchor,true);
        }
    }
    else if (m_highlighter!=nullptr)
    {
        // No HTML to replay (e.g. this bubble currently holds plain text) -- nothing else would
        // repaint the code-block colours, so trigger the pass explicitly rather than leaving them
        // stale until the next message that happens to load.
        m_highlighter->rehighlight();
    }
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::ensureSyntaxHighlighter(const QString& html)
{
    if (m_highlighter!=nullptr)
    {
        // Defensive only -- nothing in this codebase calls QTextEdit::setDocument() on a live
        // ChatMessageTextBrowser today, but SyntaxHighlighter's underlying QPointer<QTextDocument>
        // would otherwise go silently inert (not crash) if it ever did; see this method's own
        // header doc comment.
        if (m_highlighter->document()!=document())
        {
            m_highlighter->setDocument(document());
        }
        return;
    }

    // Cheap reject first -- avoids walking the document at all for the overwhelmingly common
    // case of a message with no code block.
    if (!html.contains(QStringLiteral("<pre"),Qt::CaseInsensitive))
    {
        return;
    }

    // The exact predicate: only the ALREADY-PARSED document is authoritative on whether
    // QTextFormat::BlockCodeLanguage actually survived -- immune to how the class="language-x"
    // attribute happened to be quoted/cased in the source markup, and what actually keeps an
    // untagged fence from attaching a highlighter at all (decision 4: no tag, no highlighting).
    if (!documentHasCodeLanguage())
    {
        return;
    }

    m_highlighter=new SyntaxHighlighter(document());
    // Mandatory, not an optimisation -- attaching to this already-non-empty document set Qt's own
    // rehighlightPending flag and silently discarded the very reformat this call now replaces
    // (see this method's own header doc comment for the qsyntaxhighlighter.cpp citations).
    m_highlighter->rehighlight();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::resizeEvent(QResizeEvent* event)
{
    QTextBrowser::resizeEvent(event);
    updateTableExpandButtons();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setWideTableScrollEnabled(bool enable)
{
    if (m_wideTableScroll==enable)
    {
        return;
    }
    m_wideTableScroll=enable;

    if (!enable && !m_lastHtml.isEmpty())
    {
        // Cheapest way to discard every pinned QTextTableFormat so Qt goes back to compressing
        // tables to fit: rebuild the document. applyWideTableLayout() below then re-tracks the
        // tables WITHOUT pinning any of them -- expand buttons stay, they are governed
        // separately by tableExpandButton.
        setHtml(m_lastHtml);
    }

    applyWideTableLayout();
    updateGeometry();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setTableExpandButtonEnabled(bool enable)
{
    if (m_tableExpandButton==enable)
    {
        return;
    }
    m_tableExpandButton=enable;

    if (!enable)
    {
        for (auto& tracked : m_tables)
        {
            if (!tracked.button.isNull())
            {
                // Deleted outright, not deleteLater()'d, for the same reason as
                // applyWideTableLayout()'s own dropUnusedButtons(): a deferred delete leaves the
                // widget parented and findable until the event loop next spins.
                delete tracked.button.data();
                tracked.button=nullptr;
            }
        }
        return;
    }

    updateTableExpandButtons();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setTableExpandButtonVisibleOnHover(bool enable)
{
    if (m_tableExpandButtonOnHover==enable)
    {
        return;
    }
    m_tableExpandButtonOnHover=enable;
    updateTableExpandButtonVisibility();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::updateTableExpandButtonVisibility()
{
    // Deliberately coarse: one answer for every table in the message rather than hit-testing
    // which table the pointer is over. Note underMouse() stays true while the pointer is on one
    // of the buttons themselves -- they are children of viewport(), and Qt only sends Leave to
    // widgets BELOW the common ancestor of the old and new widget
    // (QApplicationPrivate::dispatchEnterLeave), so a button cannot hide itself out from under
    // the cursor.
    bool visible=!m_tableExpandButtonOnHover || underMouse();
    for (auto& tracked : m_tables)
    {
        if (!tracked.button.isNull())
        {
            tracked.button->setVisible(visible);
        }
    }
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::enterEvent(QEnterEvent* event)
{
    QTextBrowser::enterEvent(event);
    updateTableExpandButtonVisibility();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::applyWideTableLayout()
{
    // Buttons are REUSED across passes, not torn down and rebuilt. applyWideTableLayout() runs on
    // every setWrapWidth(), i.e. on every bubble-width negotiation, so recreating them here meant
    // an icon lookup plus a widget construction per table per negotiation -- and, because
    // deleteLater() only runs when the event loop next spins, the outgoing buttons briefly
    // co-existed with their replacements.
    std::vector<QPointer<QWidget>> recycled;
    recycled.reserve(m_tables.size());
    for (auto& tracked : m_tables)
    {
        recycled.push_back(tracked.button);
    }
    m_tables.clear();

    auto dropUnusedButtons=[&recycled](std::size_t keep)
    {
        for (std::size_t i=keep;i<recycled.size();++i)
        {
            if (!recycled[i].isNull())
            {
                // Deleted outright rather than deleteLater()'d: nothing here is running inside one
                // of these buttons' own signal handlers, and a deferred delete would leave the
                // widget parented (and findable) until the next event-loop turn.
                delete recycled[i].data();
            }
        }
        recycled.clear();
    };

    if (document()==nullptr)
    {
        dropUnusedButtons(0);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        return;
    }

    std::vector<QTextTable*> tables;
    for (auto* frame : document()->rootFrame()->childFrames())
    {
        auto* table=qobject_cast<QTextTable*>(frame);
        if (table!=nullptr)
        {
            tables.push_back(table);
        }
    }
    if (tables.empty())
    {
        dropUnusedButtons(0);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        return;
    }

    // Drop any override left from a PREVIOUS pass before measuring: a table pinned for a narrow
    // bubble must be able to un-pin when the bubble grows, and measuring a still-pinned table
    // would just read its pinned width straight back as if that were its natural one.
    for (auto* table : tables)
    {
        auto format=table->format();
        if (format.width().type()!=QTextLength::VariableLength)
        {
            format.setWidth(QTextLength());
            table->setFormat(format);
        }
    }

    // The width each table WANTS, measured by laying the whole document out unconstrained.
    // Only the tables' own rects are read back, so the fact that this also unwraps every
    // paragraph for the duration does not matter -- the real wrap width is restored below,
    // before anything can be painted at this width.
    auto restoreWidth=document()->textWidth();
    document()->setTextWidth(-1);
    std::vector<qreal> naturalWidths;
    naturalWidths.reserve(tables.size());
    for (auto* table : tables)
    {
        naturalWidths.push_back(document()->documentLayout()->frameBoundingRect(table).width());
    }
    document()->setTextWidth(restoreWidth);

    // EVERY table is tracked, not just the wide ones -- the expand button is offered on all of
    // them (a table that fits is still worth opening larger, and it is the only way a short table
    // can be copied AS a table). Pinning stays restricted to those that genuinely do not fit.
    auto wrapWidth=lineWrapColumnOrWidth();
    bool anyPinned=false;
    for (std::size_t i=0;i<tables.size();++i)
    {
        TrackedTable tracked;
        tracked.firstPosition=tables[i]->firstPosition();
        tracked.naturalWidth=naturalWidths[i];

        // A wrap width of 0 means none has been negotiated yet (applyWideTableLayout() also runs
        // from setHtmlContent(), which can precede the first bubbleWidthHint()); there is nothing
        // to compare against, so nothing is pinned this pass -- setWrapWidth() calls back once it
        // knows. Tracking and the buttons do not depend on it, so they happen either way.
        // Otherwise: only a table that does not already fit is worth pinning -- leaving a fitting
        // table alone keeps Qt's own column balancing, which is better than a hard pin.
        if (m_wideTableScroll && wrapWidth>0 && naturalWidths[i]>static_cast<qreal>(wrapWidth))
        {
            auto format=tables[i]->format();
            format.setWidth(QTextLength(QTextLength::FixedLength,naturalWidths[i]));
            tables[i]->setFormat(format);
            tracked.pinned=true;
            anyPinned=true;
        }

        if (i<recycled.size())
        {
            tracked.button=recycled[i];
        }
        m_tables.push_back(tracked);
    }
    dropUnusedButtons(tables.size());

    setHorizontalScrollBarPolicy(anyPinned ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    if (anyPinned)
    {
        // The pins changed the layout; re-shrink-wrap so the document's own height reflects the
        // now-unwrapped cells (a pinned table is typically much SHORTER than the compressed one).
        updateSize();
    }
    updateTableExpandButtons();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::updateTableExpandButtons()
{
    if (document()==nullptr)
    {
        return;
    }

    if (!m_tableExpandButton)
    {
        return;
    }

    for (std::size_t i=0;i<m_tables.size();++i)
    {
        auto& tracked=m_tables[i];

        // Re-find the table by the position recorded when it was tracked -- the QTextTable* itself
        // is not safe to keep across a re-layout.
        auto* frame=document()->frameAt(tracked.firstPosition);
        auto* table=qobject_cast<QTextTable*>(frame);
        if (table==nullptr)
        {
            if (!tracked.button.isNull())
            {
                tracked.button->hide();
            }
            continue;
        }

        if (tracked.button.isNull())
        {
            // Icon-only IconTextButton with the curated tabler "arrows-diagonal" glyph, resolved
            // through the ordinary SVG icon locator so it follows the icon theme (and any label's
            // icon substitutions) like every other icon in the library. The alias lives in
            // resources/style/chat.json under this class's own context name.
            auto* button=new IconTextButton(
                Style::instance().svgIconLocator().icon(
                    QStringLiteral("ChatMessageTextBrowser::expandTable"),this),
                viewport());
            button->setObjectName(QStringLiteral("tableExpandButton"));
            button->setCursor(Qt::ArrowCursor);
            button->setFocusPolicy(Qt::NoFocus);
            button->setToolTip(tr("Show the full table"));
            auto index=static_cast<int>(i);
            connect(button,&IconTextButton::clicked,this,[this,index](){openTableViewer(index);});
            tracked.button=button;
        }

        auto* button=qobject_cast<IconTextButton*>(tracked.button.data());
        if (button==nullptr)
        {
            continue;
        }

        auto rect=document()->documentLayout()->frameBoundingRect(table);

        // adjustSize() rather than positioning straight from sizeHint(): the button's size is
        // owned by QSS (min-/max-width in chat.qss), and a font-size large enough to make the
        // glyph readable can push sizeHint() past that max -- positioning from the unclamped hint
        // would then shove the button left of where it actually ends up. adjustSize() applies the
        // constraints, so width()/height() below are the real, final size.
        button->adjustSize();
        auto size=button->size();

        // Anchored to the VIEWPORT's right edge, not the table's -- a pinned table's own right
        // edge is by definition scrolled off-screen, so a button placed there would be invisible
        // until the user had already scrolled to the far end (i.e. exactly when they no longer
        // need it). Only the vertical position tracks the table, and this widget never scrolls
        // vertically (sizeHint() reports the full document height; the chat list scrolls instead),
        // so no vertical scroll offset is involved.
        constexpr int margin=4;
        auto x=viewport()->width()-size.width()-margin;
        auto y=static_cast<int>(rect.top())+margin;
        button->move(x,y);
        button->raise();
    }

    // Newly created buttons must start out matching the current hover state rather than simply
    // appearing -- otherwise a message that re-lays out while the pointer is elsewhere would
    // flash its buttons on.
    updateTableExpandButtonVisibility();
}

//--------------------------------------------------------------------------

/******************************ChatMessageTableViewer**************************/

//--------------------------------------------------------------------------

namespace {

//! Text of one table cell, blocks joined by a space -- a cell that wrapped onto several blocks is
//! still ONE field in a tab-separated row, so its own internal line breaks must not become row
//! breaks in the output.
QString tableCellText(const QTextTableCell& cell)
{
    QStringList parts;
    for (auto it=cell.begin();it!=cell.end();++it)
    {
        auto text=it.currentBlock().text().trimmed();
        if (!text.isEmpty())
        {
            parts<<text;
        }
    }
    return parts.join(QLatin1Char(' '));
}

/**
 * @brief Tab-separated rendering of the cells a selection covers.
 * @param selectionStart/selectionEnd Document positions; pass -1 for "the whole table".
 *
 * Cells are joined by '\t' and rows by '\n' -- the convention every spreadsheet expects, and the
 * one thing Qt's own text/plain for a table selection does not do (it emits one cell per line).
 */
QString tableToTabSeparated(QTextTable* table, int selectionStart, int selectionEnd)
{
    QString out;
    for (int r=0;r<table->rows();++r)
    {
        QStringList cells;
        for (int c=0;c<table->columns();++c)
        {
            auto cell=table->cellAt(r,c);
            if (!cell.isValid())
            {
                continue;
            }
            // A merged cell is reported at every position it spans -- take it only at its own
            // origin, the same dedup markdownrenderer.cpp's writeTable() does.
            if (cell.row()!=r || cell.column()!=c)
            {
                continue;
            }
            if (selectionStart>=0)
            {
                auto cellStart=cell.firstCursorPosition().position();
                auto cellEnd=cell.lastCursorPosition().position();
                if (cellEnd<selectionStart || cellStart>selectionEnd)
                {
                    continue;
                }
            }
            cells<<tableCellText(cell);
        }
        if (!cells.isEmpty())
        {
            out+=cells.join(QLatin1Char('\t'));
            out+=QLatin1Char('\n');
        }
    }
    return out;
}

}

//--------------------------------------------------------------------------

ChatMessageTableViewer::ChatMessageTableViewer(QWidget* parent) : QTextBrowser(parent)
{
    setReadOnly(true);
    setOpenLinks(false);
    setLineWrapMode(QTextEdit::NoWrap);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

//--------------------------------------------------------------------------

QTextTable* ChatMessageTableViewer::tableOfDocument() const
{
    if (document()==nullptr)
    {
        return nullptr;
    }
    for (auto* frame : document()->rootFrame()->childFrames())
    {
        auto* table=qobject_cast<QTextTable*>(frame);
        if (table!=nullptr)
        {
            return table;
        }
    }
    return nullptr;
}

//--------------------------------------------------------------------------

Toast* ChatMessageTableViewer::ensureToast()
{
    if (m_toast!=nullptr)
    {
        return m_toast;
    }
    if (m_ownToast==nullptr)
    {
        // Parented to the hosting window rather than to this browser: a Toast child of a
        // QAbstractScrollArea would be positioned against the widget rather than its viewport,
        // and the window is the surface the confirmation logically belongs to anyway.
        auto* host=window();
        m_ownToast=new Toast(host!=nullptr ? host : static_cast<QWidget*>(this));

        // Drawn as a CHILD widget rather than Toast's default Qt::Tool window. A Tool window
        // would already be safe -- FloatingDialogFrame's outside-click filter explicitly exempts
        // Qt::Tool/Popup/ToolTip (see isOutsidePress() in floatingdialog.cpp) -- but a child
        // cannot cause a window activation change at all, which makes "copying must not close
        // the expanded view" structural instead of dependent on that exemption staying in place.
        m_ownToast->setDrawInParent(true);
    }
    return m_ownToast;
}

//--------------------------------------------------------------------------

void ChatMessageTableViewer::copyTable()
{
    // Going through selectAll()+copy() rather than assembling a QMimeData by hand keeps this on
    // the ordinary copy path, so the clipboard gets exactly the same flavour set (html, markdown,
    // ODF, and our own text/plain override) as a manual selection would.
    selectAll();
    copy();

    // The select-all is a means to that end, not something the user asked for -- leaving the whole
    // table highlighted afterwards just looks like a stray selection. The toast is the
    // confirmation. Cleared AFTER copy(), which has already taken the clipboard data.
    auto cursor=textCursor();
    cursor.clearSelection();
    setTextCursor(cursor);

    // Emitted unconditionally, before the toast: a host that turned the built-in toast off is
    // relying on this to show its own.
    emit tableCopied();

    if (m_copyToastEnabled)
    {
        auto* toast=ensureToast();
        if (toast!=nullptr)
        {
            toast->show(tr("Copied"));
        }
    }
}

//--------------------------------------------------------------------------

QMimeData* ChatMessageTableViewer::createMimeDataFromSelection() const
{
    auto* mime=QTextBrowser::createMimeDataFromSelection();
    if (mime==nullptr)
    {
        return mime;
    }

    auto* table=tableOfDocument();
    if (table!=nullptr)
    {
        auto cursor=textCursor();
        auto tabbed=cursor.hasSelection()
                        ? tableToTabSeparated(table,cursor.selectionStart(),cursor.selectionEnd())
                        : tableToTabSeparated(table,-1,-1);
        if (!tabbed.isEmpty())
        {
            // Replaces ONLY text/plain; text/html, text/markdown and ODF stay as Qt built them.
            mime->setText(tabbed);
        }
    }

    return mime;
}

//--------------------------------------------------------------------------

void ChatMessageTableViewer::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);

    auto* copyAction=menu.addAction(tr("Copy"));
    copyAction->setEnabled(textCursor().hasSelection());
    connect(copyAction,&QAction::triggered,this,[this](){copy();});

    auto* copyTableAction=menu.addAction(tr("Copy table"));
    connect(copyTableAction,&QAction::triggered,this,[this](){copyTable();});

    menu.addSeparator();

    auto* selectAllAction=menu.addAction(tr("Select all"));
    connect(selectAllAction,&QAction::triggered,this,[this](){selectAll();});

    menu.exec(event->globalPos());
}

//--------------------------------------------------------------------------

void ChatMessageTableViewer::keyPressEvent(QKeyEvent* event)
{
    // QTextEdit::copy() returns early when the cursor has no selection, so a plain Ctrl+C in a
    // freshly opened viewer would do nothing at all -- which reads as "copying is broken" rather
    // than "you were supposed to select something first". Copy the whole table instead.
    if (event->matches(QKeySequence::Copy) && !textCursor().hasSelection())
    {
        copyTable();
        event->accept();
        return;
    }
    QTextBrowser::keyPressEvent(event);
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::openTableViewer(int index)
{
    if (index<0 || static_cast<std::size_t>(index)>=m_tables.size() || document()==nullptr)
    {
        return;
    }

    auto* table=qobject_cast<QTextTable*>(document()->frameAt(m_tables[static_cast<std::size_t>(index)].firstPosition));
    if (table==nullptr)
    {
        return;
    }

    // Select the whole frame, boundaries included -- Qt's own documented idiom for extracting a
    // frame (the positions either side of firstPosition()/lastPosition() are the frame markers;
    // selecting strictly between them would yield the cell contents WITHOUT the table structure).
    QTextCursor cursor(document());
    cursor.setPosition(std::max(0,table->firstPosition()-1));
    cursor.setPosition(std::min(document()->characterCount()-1,table->lastPosition()+1),
                       QTextCursor::KeepAnchor);
    auto tableHtml=cursor.selection().toHtml();

    auto* container=new QFrame();
    container->setObjectName(QStringLiteral("tableViewerFrame"));
    auto* containerLayout=Layout::vertical(container);

    auto* view=new ChatMessageTableViewer(container);
    view->setObjectName(QStringLiteral("tableViewer"));
    // Same document CSS the bubble itself uses, so the expanded table keeps the theme's own
    // border/padding treatment (resources/style/messagetext.css) instead of Qt's bare defaults.
    view->document()->setDefaultStyleSheet(Style::instance().css());
    view->setHtml(tableHtml);
    containerLayout->addWidget(view,1);

    // Copying already works by selection + Ctrl+C (Qt puts text/html and text/markdown on the
    // clipboard, which is what external apps paste a real table from) -- this button is purely
    // about discoverability, since nothing else tells the user that is possible.
    auto* buttonRow=new QFrame(container);
    buttonRow->setObjectName(QStringLiteral("tableViewerButtons"));
    auto* buttonLayout=Layout::horizontal(buttonRow);
    buttonLayout->addStretch(1);
    // uise::PushButton rather than a bare QPushButton: it carries the library's own click-ripple
    // overlay (see its rippleOverlay()), so the button gives the same feedback as every other
    // button in the app instead of silently doing something invisible.
    auto* fullScreenButton=new PushButton(tr("Full screen"),buttonRow);
    fullScreenButton->setObjectName(QStringLiteral("tableViewerFullScreenButton"));
    buttonLayout->addWidget(fullScreenButton);

    auto* copyButton=new PushButton(tr("Copy table"),buttonRow);
    copyButton->setObjectName(QStringLiteral("tableViewerCopyButton"));
    connect(copyButton,&PushButton::clicked,view,[view](){view->copyTable();});
    buttonLayout->addWidget(copyButton);

    auto* closeButton=new PushButton(tr("Close"),buttonRow);
    closeButton->setObjectName(QStringLiteral("tableViewerCloseButton"));
    buttonLayout->addWidget(closeButton);

    containerLayout->addWidget(buttonRow);

    container->resize(qMin(static_cast<int>(m_tables[static_cast<std::size_t>(index)].naturalWidth)+48,900),400);

    // FloatingDialogFrame is the only shell in this library that hosts an arbitrary widget as a
    // resizable top-level window -- ChatImageViewerWindow, despite the name, is hard-wired to a
    // ChatImageViewer and has no setWidget().
    auto* frame=new FloatingDialogFrame(this);
    frame->setWidget(container,true);
    frame->setAutoCloseOnOutsideClick(true);

    // Toggle, not a one-way trip -- and the button says which way it will go. A wide table is
    // exactly the content most likely to want the whole screen, so this is more than a nicety.
    // The frame lays its content out with a QBoxLayout under SetDefaultConstraint (our content is
    // a plain QFrame, not an AbstractDialog, so it never takes FloatingDialogFrame's
    // isResizable()==false / SetFixedSize path), which is what lets the table actually grow into
    // the extra space rather than sit at its old size in a huge translucent window.
    auto toggleFullScreen=[frame,fullScreenButton]()
    {
        if (frame->isFullScreen())
        {
            frame->showNormal();
            fullScreenButton->setText(tr("Full screen"));
        }
        else
        {
            frame->showFullScreen();
            fullScreenButton->setText(tr("Exit full screen"));
        }
    };
    connect(fullScreenButton,&PushButton::clicked,frame,toggleFullScreen);

    // F11 as well, matching ChatImageViewerWindow's own fullscreen shortcut so the two viewers
    // behave the same way. Escape stays the frame's own close shortcut, as it is everywhere else.
    auto* fullScreenShortcut=new QShortcut(Qt::Key_F11,frame);
    fullScreenShortcut->setContext(Qt::WindowShortcut);
    connect(fullScreenShortcut,&QShortcut::activated,frame,toggleFullScreen);

    connect(closeButton,&PushButton::clicked,frame,[frame](){frame->close();});

    // The frame is parented to this browser, so without this every expand/close cycle would
    // leave one behind for the lifetime of the message bubble. setWidget(...,true) already
    // disposes of the content; this disposes of the shell around it.
    connect(frame,&FloatingDialogFrame::closed,frame,&QObject::deleteLater);

    frame->popup();
}

//--------------------------------------------------------------------------

bool ChatMessageTextBrowser::documentHasCodeLanguage() const
{
    for (auto block=document()->begin();block!=document()->end();block=block.next())
    {
        auto fmt=block.blockFormat();
        if (fmt.hasProperty(QTextFormat::BlockCodeLanguage)
            && !fmt.stringProperty(QTextFormat::BlockCodeLanguage).isEmpty())
        {
            return true;
        }
    }
    return false;
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setAnchorUnderline(const QString& href, bool enable)
{
    if (href.isEmpty())
    {
        return;
    }
    QTextCursor cursor(document());
    for (auto block=document()->begin();block!=document()->end();block=block.next())
    {
        for (auto it=block.begin();!it.atEnd();++it)
        {
            auto fragment=it.fragment();
            if (!fragment.isValid())
            {
                continue;
            }
            auto format=fragment.charFormat();
            if (!format.isAnchor() || format.anchorHref()!=href)
            {
                continue;
            }
            cursor.setPosition(fragment.position());
            cursor.setPosition(fragment.position()+fragment.length(),QTextCursor::KeepAnchor);
            format.setFontUnderline(enable);
            cursor.setCharFormat(format);
        }
    }
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::updateHoveredAnchor(const QPoint& pos)
{
    auto href=anchorAt(pos);
    if (href==m_hoveredAnchor)
    {
        return;
    }
    if (!m_hoveredAnchor.isEmpty())
    {
        setAnchorUnderline(m_hoveredAnchor,m_linkUnderline);
    }
    m_hoveredAnchor=href;
    if (!m_hoveredAnchor.isEmpty())
    {
        setAnchorUnderline(m_hoveredAnchor,true);
    }
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::clearHoveredAnchor()
{
    if (m_hoveredAnchor.isEmpty())
    {
        return;
    }
    setAnchorUnderline(m_hoveredAnchor,m_linkUnderline);
    m_hoveredAnchor.clear();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::mousePressEvent(QMouseEvent* event)
{
    // Don't also relay a link click to the parent as a bubble-selection/content-menu gesture --
    // task-urls-and characters-in-messages.md, Stage 1. Only suppressed outside selection mode,
    // where a click already means "select this message", not "follow this link" -- anchorAt()
    // still resolves an anchor there, but activation itself is separately gated the same way in
    // mouseMoveEvent() below.
    bool onLink=!anchorAt(event->pos()).isEmpty();
    bool selecting=m_messageTextWidget && m_messageTextWidget->chatMessage()->isSelectionMode();

    QTextBrowser::mousePressEvent(event);
    if (parentWidget() && !(onLink && !selecting))
    {
        QMouseEvent *cloned = event->clone();
        QCoreApplication::sendEvent(parentWidget(), cloned);
        delete cloned;
    }
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::mouseMoveEvent(QMouseEvent* event)
{
    if (m_messageTextWidget && m_messageTextWidget->chatMessage()->isSelectionMode())
    {
        clearHoveredAnchor();
        event->ignore();
    }
    else
    {
        if (!rect().contains(event->pos()))
        {
            clearHoveredAnchor();
            event->ignore();
        }
        else
        {
            updateHoveredAnchor(event->pos());
            QTextBrowser::mouseMoveEvent(event);
        }
    }
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::leaveEvent(QEvent* event)
{
    clearHoveredAnchor();
    QTextBrowser::leaveEvent(event);
    updateTableExpandButtonVisibility();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setCopyable(bool enable)
{
    if (m_copyable==enable)
    {
        return;
    }
    m_copyable=enable;

    // NoFocus (the ctor's default) means Ctrl+C/Cmd+C can never reach this widget -- keyboard
    // shortcuts go to whichever widget currently HAS focus, and a widget with Qt::NoFocus can
    // never receive it via click or Tab. Mouse-drag selection (and so selectedText()) works
    // regardless of focus policy, which is why the live chat page's "Quote selected" flow was
    // never affected by this.
    setFocusPolicy(enable ? Qt::StrongFocus : Qt::NoFocus);
    updateContextMenuPolicy();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::setOwnContextMenuEnabled(bool enable)
{
    if (m_ownContextMenu==enable)
    {
        return;
    }
    m_ownContextMenu=enable;
    updateContextMenuPolicy();
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::updateContextMenuPolicy()
{
    // CustomContextMenu routes right-clicks to showCopyMenu() instead of Qt's own
    // createStandardContextMenu() -- see setCopyable()'s own doc comment for why. Suppressed
    // entirely (NoContextMenu, letting a right-click bubble up to a host's own message-level
    // menu) when m_ownContextMenu is off -- see setOwnContextMenuEnabled()'s own doc comment.
    setContextMenuPolicy((m_copyable && m_ownContextMenu) ? Qt::CustomContextMenu : Qt::NoContextMenu);
}

//--------------------------------------------------------------------------

void ChatMessageTextBrowser::showCopyMenu(const QPoint& pos)
{
    QMenu menu(this);

    auto copyAction=menu.addAction(tr("Copy"));
    copyAction->setEnabled(textCursor().hasSelection());
    connect(copyAction,&QAction::triggered,this,&QTextEdit::copy);

    auto selectAllAction=menu.addAction(tr("Select All"));
    connect(selectAllAction,&QAction::triggered,this,&QTextEdit::selectAll);

    menu.exec(mapToGlobal(pos));
}

/********************************ChatMessageText****************************/

//--------------------------------------------------------------------------

class ChatMessageText_p
{
    public:

        QBoxLayout* layout;

        ChatMessageTextBrowser* text;

        //! The width bubbleWidthHint() last wrapped the document at (0 = none yet, or the
        //! content has changed since -- see loadText()/clearText()). updateMaximumBubbleWidth()
        //! pins the final re-wrap to this instead of re-deriving it from the bubble's own final
        //! width -- see that method's own doc comment for why.
        int lastHintWidth=0;

        //! The source text/format last passed to loadText() (task-message-formatting-plan.md,
        //! Stage 2). Not consumed by anything in THIS stage -- markdownToHtml()'s output is
        //! theme-independent, so ChatMessageTextBrowser::applyDocumentStyle()'s existing
        //! m_lastHtml replay already restyles a rendered markdown bubble correctly on its own.
        //! Not needed by Stage 3 either: markdownToHtml() emits a code block's language as
        //! `<pre class="language-x">`, and Qt's own HTML parser reads "class=language-x"
        //! specifically on <pre> back into QTextFormat::BlockCodeLanguage, so that survives a
        //! setHtml() round-trip and Stage 3's highlighter can read it straight off the live
        //! rendered document. Cached anyway, since it costs one refcount bump per message, for a
        //! later stage that genuinely needs the ORIGINAL markdown source rather than the
        //! rendered document (Stage 5b re-render, Stage 6 mentions).
        QString sourceText;
        TextFormat sourceFormat=TextFormat::Markdown;
};

//--------------------------------------------------------------------------

ChatMessageText::ChatMessageText(QWidget* parent)
    : AbstractChatMessageText(parent),
      pimpl(std::make_unique<ChatMessageText_p>())
{
    pimpl->layout=Layout::horizontal(this);

    pimpl->text=new ChatMessageTextBrowser(this);
    pimpl->layout->addWidget(pimpl->text);

    pimpl->text->setContextMenuPolicy(Qt::NoContextMenu);

    // QTextBrowser inherits QTextEdit::selectionChanged() -- relayed here so a host (e.g.
    // ReplyDialog's Save/"Quote selected" button swap) can react to selection changes via
    // AbstractChatMessageBody alone, without depending on this concrete body type.
    connect(pimpl->text,&QTextEdit::selectionChanged,this,&AbstractChatMessageBody::selectionChanged);

    // Same idiom, for a clicked hyperlink (task-urls-and characters-in-messages.md, Stage 1) --
    // see AbstractChatMessageBody::linkActivated()'s own doc comment.
    connect(pimpl->text,&ChatMessageTextBrowser::linkActivated,this,&AbstractChatMessageBody::linkActivated);

    setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

ChatMessageText::~ChatMessageText()
{}

//--------------------------------------------------------------------------

void ChatMessageText::loadText(const QString& text, TextFormat format)
{
    pimpl->sourceText=text;
    pimpl->sourceFormat=format;

    switch (format)
    {
        case TextFormat::Html:
            pimpl->text->setHtmlContent(text);
            break;
        case TextFormat::Markdown:
            // Rendered to sanitized HTML and routed through setHtmlContent() -- NOT
            // QTextBrowser::setMarkdown() directly -- so this content gets messagetext.css,
            // linkColor/linkUnderline and theme-switch replay the same way Html content already
            // does (task-message-formatting-plan.md, Stage 2; see src/newpasswordwizard.cpp's
            // own comment on why setMarkdown() bypasses setDefaultStyleSheet() entirely).
            pimpl->text->setHtmlContent(markdownToHtml(text));
            break;
        case TextFormat::Plain:
            // setPlainTextContent(), not setPlainText() directly -- clears m_lastHtml, without
            // which a later theme switch would replay a PREVIOUS message's HTML back over this
            // plain-text content (task-message-formatting-plan.md, Stage 3's own fix; see
            // ChatMessageTextBrowser::setPlainTextContent()'s doc comment).
            pimpl->text->setPlainTextContent(text);
            break;
    }
    // A stale pin from the PREVIOUS content must never survive a content change -- the new
    // document's true idealWidth() (measured fresh by the next bubbleWidthHint() call) is what
    // updateMaximumBubbleWidth() must pin to instead.
    pimpl->lastHintWidth=0;
}

//--------------------------------------------------------------------------

void ChatMessageText::clearText()
{
    pimpl->text->setHtmlContent(QString{});
    pimpl->text->clear();
    pimpl->lastHintWidth=0;
    pimpl->sourceText.clear();
    pimpl->sourceFormat=TextFormat::Markdown;
}

//--------------------------------------------------------------------------

void ChatMessageText::clearContentSelection()
{
    auto cur=pimpl->text->textCursor();
    cur.clearSelection();
    pimpl->text->setTextCursor(cur);
}

//--------------------------------------------------------------------------

void ChatMessageText::updateChatMessage()
{
    pimpl->text->setMessageTextWidget(this);
}

//--------------------------------------------------------------------------

int ChatMessageText::bubbleWidthHint(int forMaxWidth)
{
    auto wrapWidth=clampToMaxBubbleWidth(forMaxWidth);
    pimpl->text->setWrapWidth(wrapWidth);
    // qCeil, not a plain truncating cast: lastHintWidth must be >= the document's true
    // idealWidth() (the natural width of its widest line) for updateMaximumBubbleWidth()'s pin
    // below to be sound -- rounding DOWN could land under idealWidth and force an extra wrap
    // when re-applied there, moving the very line lastTextLineRect() measures.
    auto w=qCeil(pimpl->text->document()->idealWidth());
    if (w>wrapWidth)
    {
        w=wrapWidth;
    }
    pimpl->lastHintWidth=w;
    return w;
}

//--------------------------------------------------------------------------

void ChatMessageText::updateMaximumBubbleWidth()
{
    auto w=clampToMaxBubbleWidth(chatContent()->maximumBubbleWidth());

    // Pin the re-wrap to the width bubbleWidthHint() actually measured this pass' document at,
    // instead of re-deriving it from the bubble's own final width -- which, in inline mode, can
    // be WIDER than what was measured (the bubble widened specifically to seat the bottom row
    // beside this text). Re-wrapping at ANY width >= lastHintWidth reproduces byte-identical
    // line breaks: by definition of idealWidth(), every line already fit within lastHintWidth,
    // so a wider (or equal) constraint cannot force any line to wrap differently. This is what
    // keeps lastTextLineRect() -- measured during bubbleWidthHint(), before this call -- still
    // accurate afterwards, with no re-measurement needed. Same idiom, same reason, as
    // ChatMessageImages::updateMaximumBubbleWidth()'s own lastLayoutForMaxWidth pin on the album
    // body -- see its doc comment for the real-world case where re-laying out at a different
    // budget genuinely changes the result.
    if (pimpl->lastHintWidth>0 && pimpl->lastHintWidth<=w)
    {
        w=pimpl->lastHintWidth;
    }
    pimpl->text->setWrapWidth(w);
}

//--------------------------------------------------------------------------

QRect ChatMessageText::lastTextLineRect() const
{
    auto rect=pimpl->text->lastLineRect();
    if (!rect.isValid())
    {
        return {};
    }
    // ChatMessageText itself carries no QSS padding today (Layout::horizontal(this) zeroes its
    // contentsMargins, and nothing overrides them) -- translating by them anyway keeps this
    // correct if that ever changes, matching how bubbleWidthHint()/updateMaximumBubbleWidth()
    // treat the browser as filling this frame's own contents rect.
    return rect.translated(contentsMargins().left(),contentsMargins().top());
}

//--------------------------------------------------------------------------

int ChatMessageText::ownWidthCeiling() const
{
    // clampToMaxBubbleWidth() against the largest possible value is exactly maxBubbleWidth()
    // itself when the cap is enabled (>0), or a pass-through (no cap) when it's disabled -- same
    // helper bubbleWidthHint()/updateMaximumBubbleWidth() already use, so this can never
    // disagree with what they actually enforced.
    return clampToMaxBubbleWidth(std::numeric_limits<int>::max());
}

//--------------------------------------------------------------------------

QString ChatMessageText::selectedText() const
{
    // QTextCursor::selectedText() uses U+2029 (ParagraphSeparator) for a line break, not '\n' --
    // left as-is, a multi-line selection would reach the clipboard/composer as one run of text
    // with an invisible separator instead of a real line break.
    auto text=pimpl->text->textCursor().selectedText();
    text.replace(QChar(QChar::ParagraphSeparator),QChar('\n'));
    return text;
}

//--------------------------------------------------------------------------

bool ChatMessageText::hasSelectableText() const
{
    return !pimpl->text->toPlainText().trimmed().isEmpty();
}

//--------------------------------------------------------------------------

void ChatMessageText::setCopyable(bool enable)
{
    pimpl->text->setCopyable(enable);
}

//--------------------------------------------------------------------------

void ChatMessageText::setOwnContextMenuEnabled(bool enable)
{
    pimpl->text->setOwnContextMenuEnabled(enable);
}

//--------------------------------------------------------------------------

void ChatMessageText::selectText(const QString& text)
{
    if (text.isEmpty())
    {
        return;
    }
    auto cursor=pimpl->text->document()->find(text);
    if (cursor.isNull())
    {
        // Not found -- e.g. the quote was picked before an edit changed this text. Best-effort,
        // see this method's own doc comment (abstractchatmessage.hpp): silently do nothing.
        return;
    }
    pimpl->text->setTextCursor(cursor);
}

//--------------------------------------------------------------------------

QString ChatMessageText::linkAt(const QPoint& pos) const
{
    return pimpl->text->anchorAt(pimpl->text->mapFrom(this,pos));
}

//--------------------------------------------------------------------------

}
